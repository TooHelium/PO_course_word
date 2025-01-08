#include <iostream>
#include <regex>
#include <fstream>
#include <algorithm> 
#include <cctype>

#include <filesystem>

#include <unordered_map>

#include <cstdint> //for uint...

#include <shared_mutex> //for class

#include <functional> // For std::hash



void split(const std::string& file_path)
{
	std::ifstream file(file_path);
	
	if (!file)
	{
		std::cout << "Error opening file\n";
		return;
	}
	
	std::string line;
	std::regex word_regex("(\\w+)");
	
	while ( std::getline(file, line) )
	{
		auto first_word = std::sregex_iterator(line.begin(), line.end(), word_regex);
		auto last_word = std::sregex_iterator();
		
		for (std::sregex_iterator i = first_word; i != last_word; ++i)
		{
			std::smatch match = *i;
			std::string match_str = match.str();
			
			std::transform(match_str.begin(), match_str.end(), match_str.begin(),
						   [](unsigned char c) { return std::tolower(c); });
						   
			std::cout << match_str << " "; //<< '\n';
		}
		std::cout << '\n';
	}
	
	file.close();
}


namespace fs = std::filesystem;

//std::string allowable_extensions[] = {".txt"} 

void walkdirs(const std::string& directory_path)
{
	if (fs::exists(directory_path) && fs::is_directory(directory_path))
	{
		for (const auto& entry : fs::directory_iterator(directory_path))
		{
			if (entry.path().extension() == ".txt")
			{
				split(entry.path().string());//std::cout << entry.path() << std::endl;
			}
		}
	}
	else
	{
		std::cout << "directory does not exist or not a directory\n";
	}
}















class AuxiliaryIndex
{
private:
	std::unordered_map<std::string, std::vector<std::pair<uint32_t, std::vector<uint32_t>>>> table_;
	size_t num_segments_;
	std::shared_mutex segments_[num_segments_];
	
public:
	AuxiliaryIndex(size_t s)
	{	
		num_segments_ = s ? s : 1; //to make sure s is always > 0
		
		for (int i = 0; i < num_segments_; ++i)
		{
			shared_mutexes[i] = std::shared_mutex();
		}
	}

	size_t GetSegment(const std::string& term) 
	{
		size_t hash_value = std::hash<std::string> {}(term);
		return hash_value % num_segments_;
	}

	void Read(const std::string& term) 
	{
		size_t i = GetSegment(term);
		std::shared_lock<std::shared_mutex> _(segments_[i]);
		
		if ( table_.contains(term) )
		{
			table_
		}
		//TODO
	}
	
	void Write(const std::string& term, uint32_t doc_id, uint32_t term_position)
	{
		size_t i = GetSegment(term);
		
		std::unique_lock<std::shared_mutex> _(segments_[i]);
		
		if ( table_.contains(term) )
		{
			table_[]
		}
		else
		{
			table_[term] = std::pair<uint32_t, std::vector<uint32_t>>;
		}                                                                //TODO
	}
}

int main()
{
	
	
	
	//split("msg.txt");
	//walkdirs("C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR");
	
	return 0;
}


















/*

std::unordered_map<std::string, std::vector<std::pair<int,int>>> my_map;
std::vector<std::shared_mutex> shared_mutexes;
	
std::string terms[] = {
	"apple", "bicycle", "candle", "dragon", "elephant", 
	"flower", "guitar", "honey", "island", "jungle",
	"kitten", "lemon", "mountain", "notebook", "orange", 
	"piano", "quartz", "rainbow", "sunflower", "tulip",
	"umbrella", "violet", "whale", "xylophone", "yogurt", 
	"zebra", "arrow", "butterfly", "castle", "dolphin",
	"eagle", "falcon", "grape", "hazelnut", "igloo", 
	"jellyfish", "kite", "lantern", "marble", "nectar"
};

std::string tmp[] = {
	"a", "b", "c", "d", "e",
	"f", "g", "h", "j", "k"
};

void read(int index)
{
	std::shared_lock<std::shared_mutex> _(shared_mutexes[index]);
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 39);
	
	int random_index = dis(gen);
	
	std::cout << "reader: " << terms[random_index] << std::endl;
}

void write(int index)
{
	std::unique_lock<std::shared_mutex> _(shared_mutexes[index]);
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 39);
	
	int random_index = dis(gen);
	
	std::uniform_int_distribution<> dis2(0, 9);
	int r2 = dis2(gen);
	
	terms[random_index] = tmp[r2];
	
	std::cout << "writer: " << terms[random_index] << std::endl;
}

int main()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 9);
		
	unsigned int table_segments = 10;
	for (int i = 0; i < table_segments; ++i)
	{
		shared_mutexes.push_back(std::shared_mutex());
	}
	
	unsigned int writers_amount = 4;
	unsigned int readers_amount = 8;
	std::vector<std::thread> writers;
	std::vector<std::thread> readers;
	for (int i = 0; i < writers_amount; ++i)
	{
		int random_index = dis(gen);
		writers.push_back(std::thread(write, random_index));
	}
	for (int i = 0; i < readers_amount; ++i)
	{
		int random_index = dis(gen);
		readers.push_back(std::thread(read, random_index));
	}
	
	for (auto& t : writers) { t.join(); }
	for (auto& t : readers) { t.join(); }
	
	return 0;
}*/
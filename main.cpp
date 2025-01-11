#include <iostream>
#include <regex>
#include <fstream>
#include <algorithm> 
#include <cctype>

#include <thread>

#include <filesystem>

#include <unordered_map>

#include <cstdint> //for uint...

#include <shared_mutex> //for class

#include <functional> // For std::hash

#include <mutex>

#include <queue>

class AuxiliaryIndex
{
private:
	std::vector<std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<uint32_t>>>> table_;
	size_t num_segments_;
	std::vector<std::unique_ptr<std::shared_mutex>> segments_;
	
	std::string merge_path_ = "C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\merged index";
	
	size_t num_top_doc_ids_ = 5;
	
public:
	AuxiliaryIndex(size_t s)
	{	
		num_segments_ = s ? s : 1; //to make sure s is always > 0
		
		table_.resize(num_segments_); // +++
		
		segments_.reserve(num_segments_); //add some try catch
		for (size_t i = 0; i < num_segments_; ++i)
		{
			segments_.emplace_back(std::make_unique<std::shared_mutex>());
		}
	}

	size_t GetSegmentIndex(const std::string& term) 
	{
		size_t hash_value = std::hash<std::string> {}(term);
		return hash_value % num_segments_;
	}

	uint32_t Read(const std::string& term) 
	{
		size_t i = GetSegmentIndex(term);
		
		std::shared_lock<std::shared_mutex> _(*segments_[i]);
		
		auto it = table_[i].find(term);
		if ( it == table_[i].end() ) //maybe go out mutex
		{
			return 0;
			//return -1;
		}
		
		uint32_t max_freq = 0;
		uint32_t doc_id = 0;
		
		for (const auto& pair : table_[i][term])
		{
			if (pair.second.size() > max_freq)
			{
				max_freq = pair.second.size();
				doc_id = pair.first;
			}
		}
		
		return doc_id;
	}
	
	void Write(const std::string& term, uint32_t doc_id, uint32_t term_position)
	{
		size_t i = GetSegmentIndex(term);
		
		std::unique_lock<std::shared_mutex> _(*segments_[i]);
		
		table_[i][term][doc_id].push_back(term_position);
	}
	
	size_t SegmentSize(size_t i)
	{
		if (i < table_.size())
			return table_[i].size();
		
		return 0;
	}
	
	void WriteToDisk()
	{
		for (size_t i = 0; i < num_segments_; ++i)
		{
			std::unique_lock<std::shared_mutex> _(*segments_[i]); //maybe block readers?
			
			std::string merge_filename = merge_path_ + "\\m" + std::to_string(i) + ".txt"; 
			
			std::ofstream file(merge_filename);
			
			if (!file)
			{
				std::cerr << "Error opening " << merge_filename << std::endl;
				//return; // VERY DANGEROUS
			}
			else
			{
				std::vector<std::string> keys;
				keys.reserve( table_[i].size() );
				
				for (const auto& pair : table_[i])
				{
					keys.push_back(pair.first);
				}
				
				std::sort(keys.begin(), keys.end());
				
				//now write to merge file
				//term1:doc_id1=freaq,pos1,pos2,pos3,...,posn;doc_id2=...;
				//term2:doc_id1=...;
				for (const auto& term : keys)
				{
					file << term << ":";
					
					std::vector<uint32_t> doc_ids;
					doc_ids.reserve( table_[i][term].size() );
					
					////////////////////////
					auto cmp = [](const std::pair<uint32_t, size_t>& l, const std::pair<uint32_t, size_t>& r){
						return l.second > r.second; //compare by frequency
					};
					std::priority_queue<std::pair<uint32_t, size_t>, std::vector<std::pair<uint32_t, size_t>>, decltype(cmp)> most_freq_doc_ids(cmp);
					//most_freq_doc_ids.reserve( num_top_doc_ids_ );
					
					for (const auto& pair : table_[i][term])
					{
						doc_ids.push_back(pair.first);
						
						most_freq_doc_ids.push( {pair.first, pair.second.size()} ); //maybe swap with inner if .top()
						if (most_freq_doc_ids.size() > num_top_doc_ids_)
							most_freq_doc_ids.pop();
					}
					
					std::sort(doc_ids.begin(), doc_ids.end());
					
					//printing  most_freq_doc_ids in [] maybe change the collection
					std::vector<uint32_t> mfdi;
					while ( !most_freq_doc_ids.empty() )
					{
						mfdi.push_back(most_freq_doc_ids.top().first);
						most_freq_doc_ids.pop();
					}
					std::sort(mfdi.begin(), mfdi.end(), std::greater<uint32_t>());
					/// very uneficient !!!!!!!!!!!!!
					
					file<<"[";
					for (const auto& v : mfdi)
						file<<std::to_string(v)<<","; //the last coma is 
					file<<"]";
					
					for (const auto& doc_id : doc_ids)
					{
						file<<std::to_string(doc_id)<<"="<<std::to_string(table_[i][term][doc_id].size());
						
						for (const auto& pos : table_[i][term][doc_id])
						{
							file << "," << std::to_string(pos);
						}
						
						file << ";";
					}
					
					file << std::endl;
				}
			}
			
			file.close();
		}
	}
	
	void ReadFromDiskIndex(const std::string& term)
	{
		size_t i = GetSegmentIndex(term);
		
		std::string index_filename = merge_path_ + "\\m" + std::to_string(i) + ".txt"; //change path to main index
			
		std::ifstream file(index_filename);
		
		if (!file)
		{
			std::cout << "Error opening file (index) " << index_filename << std::endl;
		}
		else
		{
			std::string line;
			std::regex term_regex("^" + term + ":");    // "\\w+(['-]\\w+)*");
			
			//while ( std::getline(file, line) )
			//{
				
			//} TODO
		}
		
		file.close(); //maybe after fault i do not need to close the file
	}
};

void split(const std::filesystem::path& file_path, AuxiliaryIndex& ai)
{
	std::ifstream file(file_path.string());
	
	if (!file)
	{
		std::cout << "Error opening file\n";
		return;
	}
	
	std::string filename = file_path.stem().string();
	filename.erase(filename.size() - 2); // erasing '_i' part
	uint32_t doc_id = static_cast<uint32_t>( std::stoul(filename) );
	
	// ATTENTION uint32_t doc_id = static_cast<uint32_t>( std::stoul(file_path.stem().string()) );
	
	uint32_t word_position = 1;
	
	std::string line;
	std::regex word_regex("\\w+(['-]\\w+)*");
	
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
						   
			ai.Write(match_str, doc_id, word_position++);
			//std::cout << match_str << " " << doc_id << " " << word_position++ << std::endl;
			//std::cout << match_str << " " << word_position++ << ", "; //<< '\n';
		}
		//std::cout << '\n';
	}
	
	file.close();
}


namespace fs = std::filesystem;

void walkdirs(const std::string& directory_path, AuxiliaryIndex& ai)
{
	if (fs::exists(directory_path) && fs::is_directory(directory_path))
	{
		for (const auto& entry : fs::directory_iterator(directory_path))
		{
			if (entry.path().extension() == ".txt") //or maybe .html also
			{
				split(entry.path(), ai);
			}
		}
	}
	else
	{
		std::cout << "directory does not exist or not a directory\n";
	}
}


void testIndex(AuxiliaryIndex& ai)
{
	std::string s = "somelonglonglongword";
	for (int i = 0; i < 100000; ++i)
		ai.Write(s+std::to_string(i), i, i);
	
	std::cout << ai.Read("somelonglonglongword99999") << std::endl;
}

int main()
{
	std::string dirs[5] = {
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\1",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\2",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\3",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\4",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\5"
	};
	
	AuxiliaryIndex ai_many(10);
	
	std::thread writers[5];
	for (int i = 0; i < 5; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < 5; ++i)
		writers[i].join();
	
	std::cout << ai_many.Read("windows11hplaptop") << std::endl;
	for (size_t i = 0; i < 10; ++i)
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
	
	std::cout << "Writing to disk..." << std::endl;
	
	ai_many.WriteToDisk();
	
	return 0;
}


















/*
std::string dirs[5] = {
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\1",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\2",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\3",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\4",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\5"
	};
	
	AuxiliaryIndex ai_one(10);

	walkdirs(dirs[0], ai_one);
	walkdirs(dirs[1], ai_one);
	walkdirs(dirs[2], ai_one);
	walkdirs(dirs[3], ai_one);
	walkdirs(dirs[4], ai_one);
	
	std::cout << ai_one.Read("windows11hplaptop") << std::endl;
	for (size_t i = 0; i < 10; ++i)
		std::cout << i << " " << ai_one.SegmentSize(i) << std::endl;

	
	AuxiliaryIndex ai_many(10);
	std::thread writers[5];
	
	for (int i = 0; i < 5; ++i)
	{
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	}
	
	
	//walkdirs("C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\1", ai);
	
	
	
	for (int i = 0; i < 5; ++i)
	{
		writers[i].join();
	}
	
	std::cout << ai_many.Read("windows11hplaptop") << std::endl;
	for (size_t i = 0; i < 10; ++i)
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;











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
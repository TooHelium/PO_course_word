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

#include <set>

class AuxiliaryIndex
{
private:
	using TermType = std::string;
	using DocIdType = uint32_t;
	using PosType = uint32_t;
	using FreqType = size_t;
	
	using FreqDocIdPair = std::pair<FreqType, DocIdType>;
	using DecrFreqStat = std::vector<FreqDocIdPair>;
	
	using TermInfo = std::pair< 
								DecrFreqStat, 
								std::unordered_map<DocIdType, std::vector<PosType>>
							  >;
	
	using TermsTable = std::unordered_map<TermType, TermInfo>;
	
	std::vector<TermsTable> table_;
				 
				 
	size_t num_segments_;
	std::vector<std::unique_ptr<std::shared_mutex>> segments_;
	
	std::string merge_path_ = "/home/dima/Desktop/БІС/test IR/Новая папка/merged index";
	
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

	size_t GetSegmentIndex(const TermType& term) 
	{
		size_t hash_value = std::hash<TermType> {}(term);
		return hash_value % num_segments_;
	}

	DocIdType Read(const TermType& term) 
	{
		size_t i = GetSegmentIndex(term);
		
		std::shared_lock<std::shared_mutex> _(*segments_[i]);
		
		auto it = table_[i].find(term);
		if ( it == table_[i].end() ) //maybe go out mutex
			return 0;
		
		return table_[i][term].first[0].second;
	}
	
	void Write(const TermType& term, DocIdType doc_id, PosType term_position)
	{
		size_t i = GetSegmentIndex(term);
		
		std::unique_lock<std::shared_mutex> _(*segments_[i]);
		
		auto& positions = table_[i][term].second[doc_id]; //auto can be changed !!!!!!!!!!!!! i know type
		positions.push_back(term_position);
		
		auto& stat = table_[i][term].first;
		//UpdateDecrFreqStat
		if (stat.size() < num_top_doc_ids_ 
		    || positions.size() > stat.back().first)
		{
			auto it = std::lower_bound(stat.begin(), stat.end(), 
									   FreqDocIdPair{positions.size(), doc_id}, 
									   [](const FreqDocIdPair& l, const FreqDocIdPair& r){
										   return l.first > r.first;
									   });
									  
			stat.insert(it, {positions.size(), doc_id});
			
			if (stat.size() > num_top_doc_ids_)
				stat.pop_back();
		}
	}
	
	/*size_t SegmentSize(size_t i)
	{
		if (i < table_.size())
			return table_[i].size();
		
		return 0;
	}*/
	
	/*void WriteToDisk()
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
				std::vector<TermType> keys;
				keys.reserve( table_[i].size() );
				
				for (const auto& pair : table_[i])
				{
					keys.push_back(pair.first);
				}
				
				std::sort(keys.begin(), keys.end());
				
				//now write to merge file
				//term1:doc_id1=freaq,pos1,pos2,pos3,...,posn;doc_id2=...;
				for (const auto& term : keys)
				{
					file << term << ":" << "["; //i think can be combined
					
					for (const auto& pair : table_[i][term].first)
						file << std::to_string(pair.second) << ","; //the last coma is inaviTable
					file << "]";
					
					
					std::vector<DocIdType> doc_ids;
					doc_ids.reserve( table_[i][term].second.size() );
					for (const auto& pair : table_[i][term].second)
					{
						doc_ids.push_back(pair.first);
					}
					std::sort(doc_ids.begin(), doc_ids.end());
						
					for (const auto& doc_id : doc_ids)
					{
						file << std::to_string(doc_id) << "=";
						
						for (const auto& pos : table_[i][term].second[doc_id])
							file << std::to_string(pos) << ",";
						
						file << ";";
					}
					
					file << std::endl;
				}
			}
			
			file.close();
		}
	}*/
	
/*	void ReadFromDiskIndex(const std::string& term) //TODO
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
*/
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


int main()
{
	std::string dirs[10] = {
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/1",
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/2"
	};
	
	AuxiliaryIndex ai_many(10);
	
	int t = 2;
	std::thread writers[t];
	for (int i = 0; i < t; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < t; ++i)
		writers[i].join();
	
	size_t total = 0;
	for (size_t i = 0; i < 10; ++i){
		//std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		//total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;
	std::cout << "Writing to disk..." << std::endl;
	
	//ai_many.WriteToDisk();
	
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

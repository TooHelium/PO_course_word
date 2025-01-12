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

//#include <queue>

#include <set>

#include <atomic>

class AuxiliaryIndex
{
private:
	using TermType = std::string;
	using DocIdType = uint32_t;
	using PosType = uint32_t;
	using FreqType = size_t;
	
	struct DocFreqEntry
	{
		DocIdType doc_id = 0;
		FreqType freq = 0;
	};
	//using FreqDocIdPair = std::pair<FreqType, DocIdType>;
	using DescFreqRanking = std::vector<DocFreqEntry>;
	
	struct TermInfo
	{
		DescFreqRanking desc_freq_ranking; //decr_freq_ranking
		std::unordered_map<DocIdType, std::vector<PosType>> doc_pos_map; //maybe add some initialization
	};
	//using TermInfo = std::pair< 
	//							DecrFreqStat, 
	//							std::unordered_map<DocIdType, std::vector<PosType>>
	//						  >;
	
	using TermsTable = std::unordered_map<TermType, TermInfo>;
	
	std::vector<TermsTable> table_;
				 

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
		
		return table_[i][term].desc_freq_ranking[0].doc_id;
	}
	
	void Write(const TermType& term, const DocIdType& doc_id, const PosType& term_position)
	{
		size_t i = GetSegmentIndex(term);
		
		std::unique_lock<std::shared_mutex> _(*segments_[i]);
		
		std::vector<PosType>& positions = table_[i][term].doc_pos_map[doc_id];
		positions.push_back(term_position);
		
		std::vector<DocFreqEntry>& ranking = table_[i][term].desc_freq_ranking;
		//UpdateDecrFreqStat NOW
		if (ranking.size() < num_top_doc_ids_ 
		    || positions.size() > ranking.back().freq) //when we delete we need this algorithm too !!!!!!!
		{
			DocFreqEntry curr;//auto curr = DocFreqEntry{doc_id, positions.size()};
			curr.doc_id = doc_id;
			curr.freq = positions.size();

			auto it = std::find_if(ranking.begin(), ranking.end(), 
								   [&curr](const DocFreqEntry& entry) { return entry.doc_id == curr.doc_id; });

			if (it != ranking.end()) 
				it->freq = curr.freq;
			else 
				ranking.push_back(curr);

			std::sort(ranking.begin(), ranking.end(), 
							 [](const DocFreqEntry& l, const DocFreqEntry& r) {
								 return l.freq > r.freq;
							 });
							 
			if (ranking.size() > num_top_doc_ids_) //[10,164,18,71,67,]
				ranking.pop_back(); 
		}
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
				std::vector<TermType> terms;
				terms.reserve( table_[i].size() );
				
				for (const auto& pair : table_[i])
				{
					terms.push_back(pair.first);
				}
				
				std::sort(terms.begin(), terms.end());
				
				//now write to merge file
				//term1:doc_id1=freaq,pos1,pos2,pos3,...,posn;doc_id2=...;
				for (const TermType& term : terms)
				{
					file << term << ":" << "["; //i think can be combined
					
					for (const DocFreqEntry& entry : table_[i][term].desc_freq_ranking)
						file << std::to_string(entry.doc_id) << ","; //the last coma is inaviTable
					file << "]";
					
					
					std::vector<DocIdType> doc_ids;
					doc_ids.reserve( table_[i][term].doc_pos_map.size() );
					for (const auto& pair : table_[i][term].doc_pos_map)
					{
						doc_ids.push_back(pair.first);
					}
					std::sort(doc_ids.begin(), doc_ids.end());
						
					for (const DocIdType& doc_id : doc_ids)
					{
						file << std::to_string(doc_id) << "=";
						
						for (const PosType& pos : table_[i][term].doc_pos_map[doc_id])
							file << std::to_string(pos) << ",";
						
						file << ";";
					}
					
					file << std::endl;
				}
			}
			
			file.close();
		}
	}
	
	DocIdType ReadFromDiskIndex(const std::string& term) //TODO
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
			std::regex term_regex("^" + term + ":\\[([0-9,]+)\\]");
			std::smatch match;
			
			while (std::getline(file, line))
			{
				if (std::regex_search(line, match, term_regex)) 
				{
					std::string nums = match[1].str();
					std::regex num_regex("[0-9]+");
		
					std::vector<DocIdType> res_doc_ids;
					res_doc_ids.reserve(num_top_doc_ids_); //maybe use separate variable for this?

					auto first_num = std::sregex_iterator(nums.begin(), nums.end(), num_regex);
					auto last_num = std::sregex_iterator();

					for (std::sregex_iterator ri = first_num; ri != last_num; ++ri)
						res_doc_ids.push_back(static_cast<DocIdType>( std::stoul(ri->str())));

					return res_doc_ids[0]; //!!!! NOW I ONLY RETURN TOP 1 element
				}
			}
		}
		
		file.close(); //maybe delete?
		
		return 0; //of DocIdType
	}
};

//std::atomic<size_t> D{0};

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
		}
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
	size_t num_of_segments = 10;          
	
	AuxiliaryIndex ai_many(num_of_segments);
	
	
	std::cout << ai_many.ReadFromDiskIndex("and") << std::endl; //10 4
	std::cout << ai_many.ReadFromDiskIndex("south") << std::endl; //238 4
	std::cout << ai_many.ReadFromDiskIndex("text") << std::endl; //94 8
	
	
	/*
	std::string dirs[4] = {
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\1", //2 3 4 1
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\2",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\3",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\4"
	};
	
	walkdirs(dirs[0], ai_many);
	walkdirs(dirs[1], ai_many);
	walkdirs(dirs[2], ai_many);
	walkdirs(dirs[3], ai_many);
	*/
	
	/*
	int t = 4;
	std::thread writers[t];
	for (int i = 0; i < t; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < t; ++i)
		writers[i].join();
	*/
	
	/*
	size_t total = 0;
	std::cout << ai_many.Read("soft-core") << std::endl;
	for (size_t i = 0; i < num_of_segments; ++i){
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;
	*/
	
	//std::cout << "Writing to disk..." << std::endl;
	
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
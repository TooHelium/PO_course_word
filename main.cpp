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
	using DescFreqRanking = std::vector<DocFreqEntry>;
	
	struct TermInfo
	{
		DescFreqRanking desc_freq_ranking;
		std::map<DocIdType, std::vector<PosType>> doc_pos_map; //maybe add some initialization
		
		std::string RankingToString()
		{
			std::ostringstream oss;
			
			oss << "[";
			
			for (const DocFreqEntry& entry : desc_freq_ranking)
				oss << entry.doc_id << ",";
			
			std::string res = oss.str();
			res.back() = ']'; //remove the last comma
			
			return res;
		}
		
		std::string MapToString()
		{
			std::ostringstream oss;
			
			for (const auto& doc_pos_pair : doc_pos_map)
			{
				oss << doc_pos_pair.first << "=" << doc_pos_pair.second.size();
				
				for (auto it = doc_pos_pair.second.begin(); it != doc_pos_pair.second.end(); ++it)
					oss << "," << *it;
				
				oss << ";"; //remove the last comma and ins 
			}
			
			return oss.str();
		}
		
		std::string MapEntryToString(const size_t& doc_id)
		{
			std::ostringstream oss;
			
			oss << doc_id << "=" << doc_pos_map[doc_id].size();
			
			for (auto it = doc_pos_map[doc_id].begin(); it != doc_pos_map[doc_id].end(); ++it)
				oss << "," << *it;
			
			oss << ";";
			
			return oss.str();
		}
		
		void UpdateRanking(const DocFreqEntry& new_entry, size_t num_top)
		{
			if (desc_freq_ranking.size() < num_top 
				|| new_entry.freq > desc_freq_ranking.back().freq) //when we delete we need this algorithm too !!!!!!!
			{
				auto it = std::find_if(desc_freq_ranking.begin(), desc_freq_ranking.end(), 
									   [&new_entry](const DocFreqEntry& entry) 
									   { 
											return entry.doc_id == new_entry.doc_id; 
									   });

				if (it != desc_freq_ranking.end()) 
					it->freq = new_entry.freq;
				else 
					desc_freq_ranking.push_back(new_entry);

				std::sort(desc_freq_ranking.begin(), desc_freq_ranking.end(), 
								 [](const DocFreqEntry& l, const DocFreqEntry& r) {
									 return l.freq > r.freq;
								 });
								 
				if (desc_freq_ranking.size() > num_top) //[10,164,18,71,67,]
					desc_freq_ranking.pop_back(); 
			}
		}
	};
	
	using TermsTable = std::unordered_map<TermType, TermInfo>;
	
	std::vector<TermsTable> table_;
				 
	size_t num_segments_;
	std::vector<std::unique_ptr<std::shared_mutex>> segments_;
	
	std::string main_index_path_ = "C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\main index";
	std::string merge_index_path_ = "C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\merged index";
	
	size_t num_top_doc_ids_ = 5;
	
	std::vector<DocIdType> deleted_files_list_;
	
	struct Phrase
	{
		std::vector<TermType> words;
		std::vector<TermInfo*> terms; //we can not have vectors of reference in simple way RENAME
		//std::vector<std::vector<PosType>*> pos_vectors;
		size_t distance;
		
		bool FindIn(DocIdType doc_id) //somehow we need to make them wait for each other
		{
			std::vector<std::vector<PosType>*> pos_vectors;
			
			for (TermInfo* term : terms)
			{
				auto it = term->doc_pos_map.find(doc_id);
				if (it == term->doc_pos_map.end())
					return false;
				pos_vectors.push_back(&(it->second)); //get address of positions pos_vector
			}			
			
			//use score function?
			size_t num_terms = terms.size();
			std::vector<size_t> indexes(num_terms, 0);
			std::vector<long long int> sliding_window(num_terms); //should be of type PosType, but then i need to use static_case in std::abs
			
			while (1)
			{
				for (size_t i = 0; i < num_terms; ++i)
					sliding_window[i] = (*pos_vectors[i])[indexes[i]];
				
				bool is_chain = true;
				for (size_t i = 1; i < num_terms; ++i)
				{
					if ( std::abs(sliding_window[i] - sliding_window[i-1]) > distance )
					{
						is_chain = false;
						break;
					}
				}
				
				if (is_chain)
				{
					//std::cout << "doc_id!!! " << doc_id << std::endl;//d
					return true;
				}
				
				size_t min_index = 0;
				PosType min_value = sliding_window[0];
				for (size_t i = 1; i < num_terms; ++i) 
				{
					if (sliding_window[i] < min_value) 
					{
						min_value = sliding_window[i];
						min_index = i;
					}
				}
				
				indexes[min_index] += 1;
				if (indexes[min_index] >= pos_vectors[min_index]->size()) 
					return false;
			}
		}
	};
	// 'cat' 'tasky cookies'
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

	void SplitIntoPhrases(const std::string& query, std::vector<Phrase>& phrases)
	{
		std::string word = "\\w+(['-]\\w+)*";
		
		std::regex word_regex(word);
		std::regex distance_regex("\\)(\\d+)");
		std::regex phrase_regex("\\((" + word + "(\\s+" + word + ")*)\\)\\d*");	
		
		auto phrases_begin = std::sregex_iterator(query.begin(), query.end(), phrase_regex);
		auto phrases_end = std::sregex_iterator();
		
		std::smatch match;
		
		for (auto it = phrases_begin; it != phrases_end; ++it) 
		{
			match = *it;
			
			Phrase phrase;
		
			auto words_begin = std::sregex_iterator(match[1].first, match[1].second, word_regex);
			auto words_end = std::sregex_iterator();

			for (auto word_it = words_begin; word_it != words_end; ++word_it) 
				phrase.words.push_back(word_it->str());
			
			std::string tmp = match[0].str();
			if (std::regex_search(tmp, match, distance_regex))	
				phrase.distance = std::stoull( match[1].str() );
			else
				phrase.distance = 1; //default distance. maybe  in separate class variable
			
			phrases.push_back(phrase);
		}
	}

	DocIdType ReadPhrase(const std::string& query)
	{
		std::vector<Phrase> phrases;
		SplitIntoPhrases(query, phrases);
		
		std::vector<std::shared_lock<std::shared_mutex>> locks;
		
		//std::unordered_set<size_t> acquired_segments;
		for (Phrase& phrase : phrases)
		{
			for (const TermType& word : phrase.words)
			{
				size_t i = GetSegmentIndex(word);
				locks.emplace_back(*segments_[i]);
				//acquired_segments.insert(i);
				auto it = table_[i].find(word);
				if (it != table_[i].end())
					phrase.terms.push_back( &(it->second) ); //RENAME terms in its struct
				else
					return 0;//attention
			}
		}
		
		DocIdType doc_id;
		for (const auto& pair : phrases[0].terms[0]->doc_pos_map)
		{	
			doc_id = pair.first;
			
			for (Phrase& phrase : phrases)
			{
				if (!phrase.FindIn(doc_id))
					goto continue_outter_loop;
			}
			
			return doc_id;
			
		continue_outter_loop:
		}
		
		return 0;
		
		//release the locks
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
		
		std::unique_lock<std::shared_mutex> _(*segments_[i]); //!!!!!maybe star * to go out the scope
		
		std::vector<PosType>& positions = table_[i][term].doc_pos_map[doc_id];
		positions.push_back(term_position);
		
		table_[i][term].UpdateRanking( DocFreqEntry{doc_id, positions.size()}, num_top_doc_ids_ ); //maybe make it static for CLASS, eh?
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
			
			//std::string merge_filename = merge_index_path_ + "\\me" + std::to_string(i) + ".txt"; //VAR NAME --
			std::string merge_filename = main_index_path_ + "\\ma" + std::to_string(i) + ".txt";
			
			std::ofstream file(merge_filename);
			
			if (!file)
			{
				std::cerr << "Error opening (writetodisk)" << merge_filename << std::endl;
				//return; // VERY DANGEROUS
			}
			else
			{
				std::vector<TermType> terms;
				terms.reserve( table_[i].size() );
				
				for (const auto& pair : table_[i])
					terms.push_back(pair.first);
				
				std::sort(terms.begin(), terms.end());
				
				for (const TermType& term : terms)
				{
					file << term << ":" << table_[i][term].RankingToString() << table_[i][term].MapToString() << std::endl;
				}
			}
			file.close();
		}
	}
	
	DocIdType ReadFromDiskIndex(const std::string& term) //TODO
	{
		size_t i = GetSegmentIndex(term);
		
		std::string index_filename = merge_index_path_ + "\\me" + std::to_string(i) + ".txt"; //change path to main index
			
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
	
	void MergeAiWithDisk() //TODO
	{
		//size_t D = 0;
		for (size_t i = 0; i < num_segments_; ++i)
		{
			std::unique_lock<std::shared_mutex> _(*segments_[i]); //maybe block readers?
			
			std::string index_filename = main_index_path_ + "\\ma" + std::to_string(i) + ".txt"; 
			std::string merge_filename = merge_index_path_ + "\\me" + std::to_string(i) + ".txt"; 
			
			std::ifstream ma_file(index_filename);
			std::ofstream me_file(merge_filename);
			
			if (!ma_file && !me_file)
			{
				std::cerr << "Error opening files (ma, me)" << std::endl;
			}
			else
			{
				std::vector<TermType> terms;
				terms.reserve( table_[i].size() );
				
				for (const auto& pair : table_[i])
					terms.push_back(pair.first);
				
				std::sort(terms.begin(), terms.end()); //terms are sorted
				auto terms_it = terms.begin();
				auto terms_end = terms.end();
				
				
				std::string line;
				std::regex term_regex("^(.+):"); //"\\w+(['-]\\w+)*" maybe to use this?
				std::regex term_info_regex("\\d+=\\d+,[^;]+;"); //be cautious, my friend
				std::regex doc_freq_regex("(\\d+)=(\\d+)");
				std::smatch match;
				
				while (std::getline(ma_file, line))
				{
					//maybe add regex check on correct line?
					if (terms_it == terms_end)
					{
						me_file << line << std::endl;
						continue;
					}
					
					if (std::regex_search(line, match, term_regex))
					{
						while (terms_it != terms_end && *terms_it < match[1].str())
						{
							me_file << *terms_it << ":" 
									<< table_[i][*terms_it].RankingToString() 
									<< table_[i][*terms_it].MapToString() << std::endl;
							++terms_it;
						}
						if (terms_it == terms_end || *terms_it > match[1].str())
						{
							me_file << line << std::endl;
							continue;
						}
						else //if they equal
						{
							//++D; //
							
							me_file << *terms_it << ":";
							
							std::ostringstream oss;
							std::ostringstream oss_freq;
							
							TermInfo& merged_term = table_[i][*terms_it];
							auto m_it = merged_term.doc_pos_map.begin();
							auto m_end = merged_term.doc_pos_map.end();
							 //using for sorting rankings only. maybe to use existed?
							
							auto begin = std::sregex_token_iterator(line.begin(), line.end(), term_info_regex);
							auto end = std::sregex_token_iterator();
							std::smatch df_match_d;
							
							for (auto it = begin; it != end; ++it)
							{	
								std::string tmp = it->str();
								if (std::regex_search(tmp, df_match_d, doc_freq_regex))
								{
									while (m_it != m_end)
									{
										if (static_cast<DocIdType>( std::stoul(df_match_d[1].str()) ) >= m_it->first)
										{
											oss << merged_term.MapEntryToString(m_it->first);
											++m_it;
										}
										else
											break;
									}

									oss << tmp; //duplicate
									
									merged_term.UpdateRanking
									( 
										DocFreqEntry{
														static_cast<DocIdType>( std::stoul(df_match_d[1].str()) ), 
														static_cast<FreqType>( std::stoul(df_match_d[2].str()) )
													}, 
										num_top_doc_ids_ 
									);
								}
							}
							while (m_it != m_end)
							{
								oss << merged_term.MapEntryToString(m_it->first);
								++m_it;
							}
							
							oss_freq << merged_term.RankingToString();
							
							me_file << oss_freq.str();
							me_file << oss.str();
							
							me_file << std::endl;
							
							++terms_it;
						} //if they equal
							
					}

				}
				
				while (terms_it != terms_end)
				{
					me_file << *terms_it << ":" 
							<< table_[i][*terms_it].RankingToString() 
							<< table_[i][*terms_it].MapToString() << std::endl;
					++terms_it;
				}
				
			}
			
			ma_file.close();
			me_file.close();
		}
		//std::cout << "D: " << D << std::endl;
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
	size_t num_of_segments = 1;          
	AuxiliaryIndex ai_many(num_of_segments);
	
	std::string dirs[4] = {
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\1", //2 3 4 1
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\2",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\3",
		"C:\\Users\\rudva\\OneDrive\\Desktop\\Test IR\\data\\4"
	};
	
	//walkdirs(dirs[0], ai_many);
	//walkdirs(dirs[1], ai_many);
	//walkdirs(dirs[2], ai_many);
	walkdirs(dirs[3], ai_many);
	
	//std::cout << "Writing to disk..." << std::endl;

	//ai_many.WriteToDisk(); //importang !!!!!
	
	std::cout << "Phrase in " << ai_many.ReadPhrase(" (this)4 ") << std::endl;
	
	ai_many.MergeAiWithDisk();
	
	//std::cout << "Reading from disk..." << std::endl;
	
	//std::cout << ai_many.ReadFromDiskIndex("and") << std::endl; //10 4
	//std::cout << ai_many.ReadFromDiskIndex("south") << std::endl; //238 4
	//std::cout << ai_many.ReadFromDiskIndex("text") << std::endl; //94 8
	//std::cout << ai_many.Read("soft-core") << std::endl;
	
	
	/*
	int t = 4;
	std::thread writers[t];
	for (int i = 0; i < t; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < t; ++i)
		writers[i].join();
	*/
	
	
	size_t total = 0;
	for (size_t i = 0; i < num_of_segments; ++i){
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;
	
	return 0;
}
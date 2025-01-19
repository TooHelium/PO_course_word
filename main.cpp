#include <iostream>
#include <regex>
#include <fstream>
#include <algorithm> 
#include <cctype>
#include <utility> //for swap()
#include <thread>

#include <filesystem>

#include <unordered_map>

#include <cstdint> //for uint...

#include <shared_mutex> //for class

#include <functional> // For std::hash
#include <stdexcept>
#include <mutex>
#include <chrono> //for thread to sleep

#include <queue>

#include <set>

#include <unordered_set> //for Sheduler

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
	
	size_t num_top_doc_ids_ = 5; //can be set in constuctor
	
	size_t max_segment_size_ = 1000;
	
	struct IndexPath
	{
	private:
		std::string main;
		std::string merge;
		std::unique_ptr<std::shared_mutex> mtx_ptr;
	public:
		IndexPath(const std::string& ma, const std::string& me, std::unique_ptr<std::shared_mutex> mp)
		{
			main = ma;
			merge = me;
			mtx_ptr = std::move(mp);
		}
		
		void UpdateMainIndexPath()
		{
			std::unique_lock<std::shared_mutex> _(*mtx_ptr);
			std::swap(main, merge);
		}
		
		std::string GetMainIndexPath()
		{
			std::shared_lock<std::shared_mutex> _(*mtx_ptr);
			return main;
		}
		
		std::string GetMergeIndexPath()
		{
			std::shared_lock<std::shared_mutex> _(*mtx_ptr);
			return merge;
		}
	};
	
	std::vector<IndexPath> indexes_paths_;
	
	struct Phrase
	{
		std::vector<TermType> words;
		std::vector<TermInfo*> terms; //we can not have vectors of reference in simple way RENAME
		size_t distance;
		
		size_t FindIn(DocIdType doc_id) //somehow we need to make them wait for each other
		{
			std::vector<std::vector<PosType>*> pos_vectors;
			
			if (terms.size() == 1) //TODO MAYBE MERGE WITH NEXT CODE
			{
				auto it = terms[0]->doc_pos_map.find(doc_id);
				if (it == terms[0]->doc_pos_map.end())
					return 0;
				return it->second.size();
			}

			for (TermInfo* term : terms)
			{
				auto it = term->doc_pos_map.find(doc_id);
				if (it == term->doc_pos_map.end())
					return 0;//false; YOU HAVE AN IDEA HERE TO NOT RETURN (search will return for any phare then)
				pos_vectors.push_back(&(it->second)); //get address of positions pos_vector
			}			
			
			//use score function?
			size_t num_terms = terms.size();
			std::vector<size_t> indexes(num_terms, 0);
			std::vector<long long int> sliding_window(num_terms); //should be of type PosType, but then i need to use static_case in std::abs
			
			size_t curr_score = 0;
			size_t max_score = curr_score;
			
			while (1)
			{
				for (size_t i = 0; i < num_terms; ++i)
					sliding_window[i] = (*pos_vectors[i])[indexes[i]];
				
				bool is_chain = true;
				curr_score = 0;
				for (size_t i = 1; i < num_terms; ++i)
				{
					if ( std::abs(sliding_window[i] - sliding_window[i-1]) > distance )
					{
						is_chain = false;
						continue;//break;
					}
					++curr_score;
				}
				
				max_score = (curr_score > max_score) ? curr_score : max_score;
				
				if (is_chain)
				{
					return max_score; //i have an idea to add bonus score for each whole finded phrase. so return will be in the end
				}
				
				size_t min_index = 0;
				PosType min_value = sliding_window[0];
				for (size_t i = 1; i < num_terms; ++i) 
				{
					if (sliding_window[i] <= min_value) 
					{
						min_value = sliding_window[i];
						min_index = i;
					}
				}
				
				indexes[min_index] += 1;
				if (indexes[min_index] >= pos_vectors[min_index]->size()) 
					return max_score; //maybe continue with others?
			}
		}
	};

public:
	AuxiliaryIndex(size_t s, const std::string& ma, const std::string& me)
	{	
		num_segments_ = s ? s : 1; //to make sure s is always > 0
		
		table_.resize(num_segments_); // +++
		
		segments_.reserve(num_segments_); //add some try catch
		for (size_t i = 0; i < num_segments_; ++i)
			segments_.emplace_back(std::make_unique<std::shared_mutex>());
		
		for (size_t i = 0; i < num_segments_; ++i) //creating initial empty main index
		{
			std::ofstream file(ma + "i" + std::to_string(i) + ".txt");
			if (!file.is_open())
				std::cout << "Error creating initial index file " << i << std::endl;
			file.close();
		}		
		
		for (size_t i = 0; i < num_segments_; ++i)
			indexes_paths_.emplace_back(ma, me, std::make_unique<std::shared_mutex>());
			
	}

	size_t GetSegmentIndex(const TermType& term) 
	{
		size_t hash_value = std::hash<TermType> {}(term);
		return hash_value % num_segments_;
	}

	void SplitIntoPhrases(std::string query, std::vector<Phrase>& phrases)
	{
		std::transform(query.begin(), query.end(), query.begin(), 
						[](unsigned char c) { return std::tolower(c); });
		
		std::string word = "\\w+(['-]\\w+)*";
		
		std::regex word_regex(word);
		std::regex distance_regex("\\)(\\d+)");
		std::regex phrase_regex("\\([^\\w]*((" + word + ")([^\\w]*" + word + ")*)[^\\w]*\\)(\\d*)");	
		
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

		if (phrases.empty())
			return DocIdType(0);
		
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
					return DocIdType(0);//attention WILL WE RELEASE LOCKS HERE !!!!!!!!!!!!!!!
			}
		}

		DocIdType curr_doc_id;
		DocIdType best_doc_id = 0;
		size_t curr_score = 0;
		size_t max_score = curr_score;
		for (const auto& pair : phrases[0].terms[0]->doc_pos_map)
		{	
			curr_doc_id = pair.first;
			
			curr_score = 0;
			for (Phrase& phrase : phrases)
				curr_score += phrase.FindIn(curr_doc_id);
			
			if (curr_score > max_score)
			{
				max_score = curr_score;
				best_doc_id = curr_doc_id;
			}
			//std::cout << best_doc_id << std::endl; //DELETE TODO
		}
		
		return best_doc_id;
		
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
		
		if ( table_[i].size() > max_segment_size_ )
		{
			std::cout << "START MERGING " << i << " segment";
			MergeAiWithDisk(i);
			std::cout << "END MERGED " << i << " segment";
		}
		
	}
	
	size_t SegmentSize(size_t i) //TODO syncronization
	{
		if (i < table_.size())
			return table_[i].size();
		
		return 0;
	}
	
	void WriteToDisk() //change file name (old was 'me') TODO
	{
		for (size_t i = 0; i < num_segments_; ++i)
		{
			std::unique_lock<std::shared_mutex> _(*segments_[i]); //maybe block readers?
			
			//std::string merge_filename = merge_index_path_ + "\\me" + std::to_string(i) + ".txt"; //VAR NAME --
			std::string merge_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt";
			
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
	
	DocIdType ReadFromDiskIndexLog(const std::string& term) //TODO
	{
		size_t i = GetSegmentIndex(term);
		
		std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt"; //change path to main index
			
		std::ifstream file(index_filename);

		int counter = 0; //DELETE TODO

		if (!file)
		{
			std::cout << "Error opening file (index) " << index_filename << std::endl;
		}
		else
		{
			std::regex term_regex("^([^ ]+):\\[([0-9,]+)\\]");
			std::smatch match;
			std::string line;
			std::string tmp;

			uint64_t left = 0;
			file.seekg(0, std::ios::end);
			uint64_t right = static_cast<uint64_t>( file.tellg() );
			uint64_t mid;

			while(left <= right)
			{
				mid = (left + right) / 2;
				file.seekg(mid);
				
				while (file.tellg() > 0 && file.peek() != '\n')
					file.seekg(file.tellg() - static_cast<std::streamoff>(1));
		
				if (file.peek() == '\n')
					file.seekg(file.tellg() + static_cast<std::streamoff>(1));
				
			
				auto start = file.tellg(); //or mid
				std::getline(file, line);
				if (std::regex_search(line, match, term_regex)){
					tmp = match[1].str();
					std::cout << tmp << std::endl;}
					
				if (term < tmp)
					right = mid - 1;
				else if (term > tmp)
					left = mid + 1;
				else
				{
					file.close();
					return static_cast<DocIdType>( std::stoul(match[2].str()) ); //returns the top 1 only
				}
			}
			file.close();
			
			return DocIdType(0);
		}

		return DocIdType(0);
	}

	
	DocIdType ReadFromDiskIndexLin(const std::string& term) //TODO
	{
		size_t i = GetSegmentIndex(term);
		
		std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt"; //change path to main index
			
		std::ifstream file(index_filename);

		int counter = 0; //DELETE TODO

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
				++counter; //DELETE TODO
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
					std::cout << "Counter " << counter << std::endl;//delete TOOD
					return res_doc_ids[0]; //!!!! NOW I ONLY RETURN TOP 1 element
				}
			}
		}
		std::cout << "Counter " << counter << std::endl; //DELETE TODO
		file.close(); //maybe delete?
		
		return 0; //of DocIdType
	}
	
	
	void MergeAiWithDisk(size_t i) //TODO
	{
		//for (size_t i = 0; i < num_segments_; ++i)
		{
			//std::unique_lock<std::shared_mutex> _(*segments_[i]); //maybe block readers?
			
			std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt"; 
			std::string merge_filename = indexes_paths_[i].GetMergeIndexPath() + "i" + std::to_string(i) + ".txt"; 
			
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
			
			table_[i].clear();
			
			indexes_paths_[i].UpdateMainIndexPath();
		}
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
	//filename.erase(filename.size() - 2); // erasing '_i' part !!!!!!!!!!!!!!!!!!!!!!!!!
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

class Sheduler //TODO i think we need delete code that remove _number part from data file's path
{
private:
	std::string data_path_;
	//AuxiliaryIndex& ai_;
	std::chrono::duration<size_t> duration_;
	std::unordered_set<std::string> monitored_dirs_; //directories a Sheduler know about. they are unique
	
	std::string ready_dir_marker_ = "___"; // 3 underscores
	
	std::vector<std::string> tasks_pool_;
	
public:
	Sheduler(const std::string dp, /*AuxiliaryIndex& ai,*/ size_t sleep_duration)
	{
		if ( !(fs::exists(dp) && fs::is_directory(dp)) )
			throw std::invalid_argument("Data path does not exist or is not a directory");
		
		data_path_ = dp;
		//ai_ = ai;
		duration_ = std::chrono::seconds( sleep_duration );
	}
	
	void MonitorData(AuxiliaryIndex& ai)
	{
		std::string curr_dir;
		
		while (1)
		{
			std::this_thread::sleep_for(duration_);
			
			for (const auto& entry : fs::directory_iterator(data_path_))
			{
				if (fs::is_directory(entry.path()) && DirIsReady(entry.path()))
				{		
					curr_dir = entry.path().string();
				
					if (monitored_dirs_.find(curr_dir) == monitored_dirs_.end())
					{
						std::cout << "New directory " << curr_dir << std::endl;
						monitored_dirs_.insert(curr_dir);
						InspectDir(curr_dir, ai);
					}
				}
			}
		}
	}
	
	std::string GetPathByDocId(const uint32_t& id)
	{
		std::regex id_range_regex("(\\d+)-(\\d+)");
		std::smatch match;
		
		for (const auto& entry : fs::directory_iterator(data_path_))
		{
			if (fs::is_directory(entry.path()) && DirIsReady(entry.path()))
			{
				std::string tmp = entry.path().stem().string();
				if (std::regex_search(tmp, match, id_range_regex))
				{
					uint32_t min_id = static_cast<uint32_t>( std::stoul(match[1].str()) );
					uint32_t max_id = static_cast<uint32_t>( std::stoul(match[2].str()) );
					
					if (min_id <= id && id <= max_id)
					{
						return entry.path().string() + "/" + std::to_string(id) + ".txt";
					}
				}					
			}
		}
		
		return "none";//path_to_no_file_;
	}
	
	bool DirIsReady(const fs::path& path)
	{
		std::string path_stem = path.stem().string();
		
		if ( path_stem.substr(path_stem.length() - 3) == ready_dir_marker_ )
			return true;
		
		return false;
	}
	
	void InspectDir(const std::string& directory_path, AuxiliaryIndex& ai)
	{
		for (const auto& entry : fs::directory_iterator(directory_path))
		{
			if (entry.path().extension() == ".txt")
			{
				tasks_pool_.push_back(entry.path().string());
				split(entry.path(), ai);
			}
		}
	}
	
	void PrintTasks()
	{
		for (auto v : tasks_pool_)
			std::cout << v << std::endl;
	}
};

/*
void run(AuxiliaryIndex& ai_many)
{
	size_t num_of_segments = 10;          
	//std::string ma = "/home/dima/Desktop/БІС/test IR/Новая папка/main index/";
	//std::string me = "/home/dima/Desktop/БІС/test IR/Новая папка/merged index/";
	//AuxiliaryIndex ai_many(num_of_segments, ma, me);

	std::string dirs[4] = {
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/1", //2 3 4 1
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/2",
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/3",
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/4"
	};

	int t = 4;
	std::thread writers[t];
	for (int i = 0; i < t; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < t; ++i)
		writers[i].join();

	size_t total = 0;
	for (size_t i = 0; i < num_of_segments; ++i){
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;

	while (1) { };
}
*/
int main()
{
	/*
	size_t num_of_segments = 10;          
	std::string ma = "/home/dima/Desktop/БІС/test IR/Новая папка/main index/";
	std::string me = "/home/dima/Desktop/БІС/test IR/Новая папка/merged index/";
	AuxiliaryIndex ai_many(num_of_segments, ma, me);
	
	
		Sheduler s("/home/dima/Desktop/БІС/test IR/Новая папка/testdata/", 1);
		std::thread t(&Sheduler::MonitorData, &s, std::ref(ai_many));
		
		
		std::this_thread::sleep_for(std::chrono::duration<int>(30));
	
	std::cout << "Searching phrase..." << std::endl;
	uint32_t id = ai_many.ReadPhrase(" (fitness enjoy)4 ");
	std::cout << "Phrase in id " << id << " path: " << s.GetPathByDocId(id) << std::endl;
	
	size_t total = 0;
	for (size_t i = 0; i < num_of_segments; ++i){
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;
	
	t.detach();
	*/
	
	
	size_t num_of_segments = 10;          
	std::string ma = "/home/dima/Desktop/БІС/test IR/Новая папка/main index/";
	std::string me = "/home/dima/Desktop/БІС/test IR/Новая папка/merged index/";
	AuxiliaryIndex ai_many(num_of_segments, ma, me);
	
	std::string dirs[4] = {
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/1", //2 3 4 1
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/2",
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/3",
		"/home/dima/Desktop/БІС/test IR/Новая папка/data/4"
	}; 
	
	//walkdirs(dirs[0], ai_many);
	//walkdirs(dirs[1], ai_many);
	//walkdirs(dirs[2], ai_many);
	//walkdirs(dirs[3], ai_many);
	
	int t = 4;
	std::thread writers[t];
	for (int i = 0; i < t; ++i)
		writers[i] = std::thread(walkdirs, dirs[i], std::ref(ai_many));
	
	for (int i = 0; i < t; ++i)
		writers[i].join();
	
	std::cout << "Searching phrase..." << std::endl;
	
	std::cout << "Phrase in " << ai_many.ReadPhrase(" (home-made style) (visual) (effects, awkward dialogue,) ") << std::endl;
	
	ai_many.MergeAiWithDisk(4);

	size_t total = 0;
	for (size_t i = 0; i < num_of_segments; ++i){
		std::cout << i << " " << ai_many.SegmentSize(i) << std::endl;
		total += ai_many.SegmentSize(i);
	}
	std::cout << "Total :" << total << std::endl;

	//std::cout << ai_many.ReadFromDiskIndexLin("and") << std::endl;

	std::cout << ai_many.ReadFromDiskIndexLog("anytime") << std::endl;
	std::cout << ai_many.ReadFromDiskIndexLog("amelioration") << std::endl;
	std::cout << ai_many.ReadFromDiskIndexLog("9") << std::endl;
	
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
	
	

	
	return 0;
}
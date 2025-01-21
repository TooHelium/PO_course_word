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

#include "auxiliary_index.hpp"


std::string AuxiliaryIndex::TermInfo::RankingToString()
{
    std::ostringstream oss;
    
    oss << "[";
    
    for (const DocFreqEntry& entry : desc_freq_ranking)
        oss << entry.doc_id << ",";
    
    std::string res = oss.str();
    res.back() = ']'; //remove the last comma
    
    return res;
}
std::string AuxiliaryIndex::TermInfo::MapToString()
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
std::string AuxiliaryIndex::TermInfo::MapEntryToString(const size_t& doc_id)
{
    std::ostringstream oss;
    
    oss << doc_id << "=" << doc_pos_map[doc_id].size();
    
    for (auto it = doc_pos_map[doc_id].begin(); it != doc_pos_map[doc_id].end(); ++it)
        oss << "," << *it;
    
    oss << ";";
    
    return oss.str();
}
std::string AuxiliaryIndex::TermInfo::MapEntryWithoutIdToString(const size_t& doc_id)
{
    std::ostringstream oss;
    
    oss << doc_pos_map[doc_id].size();
    
    for (auto it = doc_pos_map[doc_id].begin(); it != doc_pos_map[doc_id].end(); ++it)
        oss << "," << *it;
    
    oss << ";";
    
    return oss.str();
}
void AuxiliaryIndex::TermInfo::UpdateRanking(const DocFreqEntry& new_entry, size_t num_top)
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


AuxiliaryIndex::IndexPath::IndexPath(const std::string& ma, const std::string& me, std::unique_ptr<std::shared_mutex> mp)
{
    main = ma;
    merge = me;
    mtx_ptr = std::move(mp);
}
void AuxiliaryIndex::IndexPath::UpdateMainIndexPath()
{
    std::unique_lock<std::shared_mutex> _(*mtx_ptr);
    std::swap(main, merge);
}
std::string AuxiliaryIndex::IndexPath::GetMainIndexPath()
{
    std::shared_lock<std::shared_mutex> _(*mtx_ptr);
    return main;
}
std::string AuxiliaryIndex::IndexPath::GetMergeIndexPath()
		{
			std::shared_lock<std::shared_mutex> _(*mtx_ptr);
			return merge;
		}


size_t AuxiliaryIndex::Phrase::FindIn(DocIdType doc_id, std::vector<TermInfo*>& terms, size_t distance) //somehow we need to make them wait for each other
{
    std::vector<std::vector<PosType>*> pos_vectors;
    
    if (terms.size() == 1) //TODO MAYBE MERGE WITH NEXT CODE
    {
        auto it = terms[0]->doc_pos_map.find(doc_id);
        if (it == terms[0]->doc_pos_map.end())
            return 0;
        return 1;//return it->second.size();
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
	

AuxiliaryIndex::AuxiliaryIndex(size_t s, const std::string& ma, const std::string& me)
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

inline size_t AuxiliaryIndex::GetSegmentIndex(const TermType& term) 
{
    return std::hash<TermType>{}(term) % num_segments_;
}

void AuxiliaryIndex::SplitIntoPhrases(std::string query, std::vector<Phrase>& phrases)
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
        
        phrase.ai_words = phrase.words;
        phrase.disk_words = phrase.words;

        std::string tmp = match[0].str();
        if (std::regex_search(tmp, match, distance_regex))	
            phrase.words_distance = std::stoull( match[1].str() );
        else
            phrase.words_distance = 1; //default distance. maybe  in separate class variable

        phrase.ai_distance = phrase.words_distance;
        phrase.disk_distance = phrase.words_distance;
        
        phrases.push_back(phrase);
    }
}

AuxiliaryIndex::DocIdType AuxiliaryIndex::ReadPhrase(const std::string& query)
{
    std::vector<Phrase> phrases;
    TermsTable phrases_disk_table;

    SplitIntoPhrases(query, phrases);

    if (phrases.empty())
        return DocIdType(0);
    
    std::vector<std::shared_lock<std::shared_mutex>> locks;
    
    //std::unordered_set<size_t> acquired_segments;
    /*
    for (Phrase& phrase : phrases) //old versioin
    {
        for (const TermType& word : phrase.words)
        {
            size_t i = GetSegmentIndex(word);
            locks.emplace_back(*segments_[i]);
            //acquired_segments.insert(i);
            auto it = table_[i].find(word);
            if (it != table_[i].end())
                phrase.ai_terms.push_back( &(it->second) ); //RENAME terms in its struct
            else
                return DocIdType(0);//attention WILL WE RELEASE LOCKS HERE !!!!!!!!!!!!!!!

            if ( !ReadTermInfoFromDiskLog(word, phrases_disk_table) )
                return DocIdType(0);

            it = phrases_disk_table.find(word);
            if (it != phrases_disk_table.end())
                phrase.disk_terms.push_back( &(it->second) );
            else
                return DocIdType(0); 
        }
    }//old version
    */

    for (Phrase& phrase : phrases) //new versioin
    {
        for (const TermType& word : phrase.words)
        {
            size_t i = GetSegmentIndex(word);
            locks.emplace_back(*segments_[i]);
            //acquired_segments.insert(i);
            auto it = table_[i].find(word);
            if (it != table_[i].end())
                phrase.ai_terms.push_back( &(it->second) ); //RENAME terms in its struct
            else
            {
                phrase.ai_words.erase( std::find(phrase.ai_words.begin(), phrase.ai_words.end(), word) );
                //phrase.words.erase(word);
                phrase.ai_distance += 1;
                //return DocIdType(0);//attention WILL WE RELEASE LOCKS HERE !!!!!!!!!!!!!!!
            }

            //if ( !ReadTermInfoFromDiskLog(word, phrases_disk_table) )
            //    return DocIdType(0);

            ReadTermInfoFromDiskLog(word, phrases_disk_table);

            it = phrases_disk_table.find(word);
            if (it != phrases_disk_table.end())
                phrase.disk_terms.push_back( &(it->second) );
            else
            {
                phrase.disk_words.erase( std::find(phrase.disk_words.begin(), phrase.disk_words.end(), word) );
                //return DocIdType(0);
                phrase.disk_distance += 1;
            }
                 
        }
    }//new version

    //std::cout << phrases.

    //AI best score
    DocIdType curr_doc_id;
    DocIdType ai_best_doc_id = 0;
    size_t curr_score = 0;
    size_t ai_max_score = curr_score;

    if ( !phrases[0].ai_terms.empty() )
    {
        for (const auto& pair : phrases[0].ai_terms[0]->doc_pos_map)
        {	
            curr_doc_id = pair.first;
            
            curr_score = 0;
            for (Phrase& phrase : phrases)
                curr_score += phrase.FindIn(curr_doc_id, phrase.ai_terms, phrase.ai_distance);
            
            if (curr_score > ai_max_score)
            {
                ai_max_score = curr_score;
                ai_best_doc_id = curr_doc_id;
            }
        }
    }

    if ( phrases[0].disk_terms.empty() )
        return ai_best_doc_id;

    DocIdType disk_best_doc_id = 0;
    curr_score = 0;
    size_t disk_max_score = curr_score;
    for (const auto& pair : phrases[0].disk_terms[0]->doc_pos_map)
    {	
        curr_doc_id = pair.first;
        
        curr_score = 0;
        for (Phrase& phrase : phrases)
            curr_score += phrase.FindIn(curr_doc_id, phrase.disk_terms, phrase.disk_distance);
        
        if (curr_score > disk_max_score)
        {
            disk_max_score = curr_score;
            disk_best_doc_id = curr_doc_id;
        }
    }

    std::cout << "ai_max_score " << ai_max_score << std::endl;
    std::cout << "ai_doc " << ai_best_doc_id << std::endl;

    std::cout << "disk_max_score " << disk_max_score << std::endl; 
    std::cout << "disk_doc " << disk_best_doc_id << std::endl;

    return ai_max_score > disk_max_score ? ai_best_doc_id : disk_best_doc_id;
    

    //now go to disk until segments are locked

    //release the locks
}

/*
bool AuxiliaryIndex::ReadTermInfoFromDiskLog(const std::string& target_term, TermsTable& phrases_disk_table) //NEW
{
    if ( phrases_disk_table.find(target_term) != phrases_disk_table.end() )
        return false;

    std::smatch matched_line;

    ReadFromDiskLogGeneral(target_term, matched_line);

    if (matched_line.empty())
        return false;

    std::string curr = matched_line[3].str();

    std::regex doc_pos_regex("([\\d]+)=\\d+(,\\d+)+;");
    std::regex num_regex("[0-9]+");
    
    std::string nums;
    DocIdType doc_id;
    std::vector<PosType> *positions;

    for (std::sregex_iterator it(curr.begin(), curr.end(), doc_pos_regex), last; it != last; ++it)
    {
        nums = it->str();
    
        std::sregex_iterator ri(nums.begin(), nums.end(), num_regex), last_num;
        doc_id = static_cast<DocIdType>( std::stoul(ri->str()) );
        ++ri; //pass frequency

        positions = &( phrases_disk_table[target_term].doc_pos_map[doc_id] );

        while ( ++ri != last_num )
            positions->push_back( static_cast<PosType>( std::stoul(ri->str()) ) );
    }
    
    return true;

   /* nums = it->str();
    
    std::sregex_iterator ri(nums.begin(), nums.end(), num_regex), last_num;
    doc_id = static_cast<DocIdType>( std::stoul(ri->str()) );
    ++++ri; //pass frequency
    pos = static_cast<PosType>( std::stoul(ri->str()) );

    positions = &( phrases_disk_table[target_term].doc_pos_map[doc_id] );
    positions->push_back(pos);

    while ( ++ri != last_num )
        positions->push_back( static_cast<PosType>( std::stoul(ri->str()) ) );*/
//}

/*
void AuxiliaryIndex::ReadFromDiskLogGeneral(const std::string& target_term, std::smatch& matched_line)
{
    size_t i = GetSegmentIndex(target_term);
    
    std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt";
        
    std::ifstream file(index_filename);

    if (!file)
    {
        std::cerr << "Error opening file for reading: " << index_filename << '\n';
        return;
    }

    std::regex term_regex("^([^ ]+):\\[([0-9,]+)\\]([^\\s\\n]+)$");
    std::string line;
    std::string curr_term;

    uint64_t left = 0;
    file.seekg(0, std::ios::end);
    uint64_t right = static_cast<uint64_t>( file.tellg() );
    uint64_t mid;
    std::streamoff one_offset = static_cast<std::streamoff>(1);

    while(left <= right)
    {
        mid = (left + right) / 2;
        file.seekg(mid);
        
        while (file.tellg() > 0 && file.peek() != '\n')
            file.seekg(file.tellg() - one_offset);

        if (file.peek() == '\n')
            file.seekg(file.tellg() + one_offset);
        
        auto start = file.tellg();
        std::getline(file, line);
        if (std::regex_search(line, matched_line, term_regex))
            curr_term = matched_line[1].str();
            
        if (target_term < curr_term)
            right = mid - 1;
        else if (target_term > curr_term)
            left = mid + 1;
        else
            return;
    }
}
*/

AuxiliaryIndex::DocIdType AuxiliaryIndex::Read(const TermType& term) //REFACTORED TODO
{
    size_t i = GetSegmentIndex(term);
    
    std::shared_lock<std::shared_mutex> _(*segments_[i]);
    
    auto it = table_[i].find(term);
    if ( it == table_[i].end() )
        return DocIdType(0);
    
    return table_[i][term].desc_freq_ranking[0].doc_id;
}


void AuxiliaryIndex::Write(const TermType& term, const DocIdType& doc_id, const PosType& term_position) //REFACTORED TODO
{
    size_t i = GetSegmentIndex(term);
    
    std::unique_lock<std::shared_mutex> _(*segments_[i]);
    
    std::vector<PosType>& positions = table_[i][term].doc_pos_map[doc_id];
    positions.push_back(term_position);
    
    table_[i][term].UpdateRanking( DocFreqEntry{doc_id, positions.size()}, num_top_doc_ids_ ); //maybe make it static for CLASS, eh?
    
    if ( table_[i].size() > max_segment_size_ )
        MergeAiWithDisk(i);
}


size_t AuxiliaryIndex::SegmentSize(size_t i) //REFACTORED TODO
{
    if (i < table_.size())
    {
        std::shared_lock<std::shared_mutex> _(*segments_[i]);
        return table_[i].size();
    }
        
    return 0;
}

/*
AuxiliaryIndex::DocIdType AuxiliaryIndex::ReadFromDiskIndexLog(const std::string& target_term) //NEW
{
    std::smatch matched_line;

    ReadFromDiskLogGeneral(target_term, matched_line);

    if (matched_line.empty())
        return DocIdType(0);

    return static_cast<DocIdType>( std::stoul(matched_line[2].str()) ); //returns the top 1 only
}
*/

size_t MergeTermInfos(std::string& s1, std::string& s2)
{
	std::regex num_regex(",(\\d+)");
	
	auto s1_begin = std::sregex_iterator(s1.begin(), s1.end(), num_regex);
	auto s1_end = std::sregex_iterator();
	
	auto s2_begin = std::sregex_iterator(s2.begin(), s2.end(), num_regex);
	auto s2_end = std::sregex_iterator();
	
	std::string res = "";
	std::string prev = res;
	std::string curr;
	
	auto s1_it = s1_begin;
	
	int s1_num, s2_num;

    size_t size = 0;
	
	auto res_append = [&res, &curr, &prev, &size](auto& s_it) 
	{	
		curr = (*s_it)[0].str();
		
		if (curr != prev)
		{
        	res += curr;
            ++size;
        }
		
		prev = curr;
	};
	
	for (auto s2_it = s2_begin; s2_it != s2_end; ++s2_it)
	{
		if (s1_it == s1_end)
		{
			res_append(s2_it);
			continue;
		}
		
		s2_num = std::stoi( (*s2_it)[1].str() );
		
		while (s1_it != s1_end)
		{
			s1_num = std::stoi( (*s1_it)[1].str() );
			if (s1_num < s2_num)
			{
				res_append(s1_it);
				++s1_it;
			}
			else if (s1_num > s2_num)
			{
				res_append(s2_it);
				break;
			}	
			else
			{
				res_append(s1_it);
				++s1_it;
				break;
			}
		}
	}
	while (s1_it != s1_end)
	{
		res_append(s1_it);
		++s1_it;
	}
	
	s1 = res;
	
    return size;
}


void AuxiliaryIndex::MergeAiWithDisk(size_t i) //REFACTORED TODO
{
    std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt"; 
    std::string merge_filename = indexes_paths_[i].GetMergeIndexPath() + "i" + std::to_string(i) + ".txt"; 
    
    std::ifstream ma_file(index_filename);
    std::ofstream me_file(merge_filename);
    
    if (!ma_file || !me_file)
    {
        std::cerr << "ERROR merging files:\n";

        if (!ma_file) std::cerr << "ma: " << index_filename << '\n';
        if (!ma_file) std::cerr << "me: " << merge_filename << '\n';

        table_[i].clear(); //DOTO NOT SURE ABOUT THIS

        return;
    }
    
    std::vector<TermType> terms;
    terms.reserve( table_[i].size() );
    
    for (const auto& pair : table_[i])
        terms.push_back(pair.first);
    
    std::sort(terms.begin(), terms.end()); //terms are sorted
    auto terms_it = terms.begin();
    auto terms_end = terms.end();
    
    std::string line;
    std::regex term_regex("^(.+):"); //"\\w+(['-]\\w+)*" maybe to use this?
    std::regex term_info_regex("\\d+=(\\d+,[^;]+);"); //be cautious, my friend
    std::regex doc_freq_regex("(\\d+)=(\\d+)");
    std::smatch match;
    
    while (std::getline(ma_file, line))
    {
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
                
                auto begin = std::sregex_iterator(line.begin(), line.end(), term_info_regex);
                auto end = std::sregex_iterator();
                std::smatch df_match_d;
                
                std::string tmp;
                DocIdType line_doc_id;
                bool skip_line_insert;
                for (auto it = begin; it != end; ++it)
                {	
                    tmp = (*it)[0].str();
                    skip_line_insert = false;

                    if (std::regex_search(tmp, df_match_d, doc_freq_regex))
                    {
                        line_doc_id = static_cast<DocIdType>( std::stoul(df_match_d[1].str()) );
                        while (m_it != m_end)
                        {
                            if (line_doc_id > m_it->first)
                            {
                                oss << merged_term.MapEntryToString(m_it->first);
                                ++m_it;
                            }
                            else if (line_doc_id == m_it->first)
                            {
                                std::string s1 = merged_term.MapEntryWithoutIdToString(m_it->first);
                                std::string s2 = (*it)[1].str();
                                FreqType f = static_cast<FreqType>( MergeTermInfos(s1, s2) );

                                oss << std::to_string(line_doc_id);
                                oss << "=";
                                oss << std::to_string(f);
                                oss << s1;
                                oss << ";";

                                merged_term.UpdateRanking
                                (
                                    DocFreqEntry{
                                                    line_doc_id,
                                                    f
                                                },
                                    num_top_doc_ids_
                                );

                                ++m_it;
                                skip_line_insert = true;
                                break;
                            }
                            else
                                break;
                        }

                        if ( !skip_line_insert )
                        {
                            oss << tmp;
                            
                            merged_term.UpdateRanking
                            ( 
                                DocFreqEntry{
                                                line_doc_id, 
                                                static_cast<FreqType>( std::stoul(df_match_d[2].str()) )
                                            }, 
                                num_top_doc_ids_ 
                            );
                        }
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
            }
                
        }

    }
    
    while (terms_it != terms_end)
    {
        me_file << *terms_it << ":" 
                << table_[i][*terms_it].RankingToString() 
                << table_[i][*terms_it].MapToString() << std::endl;
        ++terms_it;
    }
    
    table_[i].clear();
    
    indexes_paths_[i].UpdateMainIndexPath();
}


AuxiliaryIndex::DocIdType AuxiliaryIndex::ReadFromDiskIndexLog(const std::string& target_term) //REFACTORED TODO
{
    size_t i = GetSegmentIndex(target_term);
    
    std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt";
        
    std::ifstream file(index_filename);

    if (!file)
    {
        std::cerr << "Error opening file for reading: " << index_filename << '\n';
        return DocIdType(0);
    }

    std::regex term_regex("^([^ ]+):\\[([0-9,]+)\\]");
    std::smatch match;
    std::string line;
    std::string curr_term;

    uint64_t left = 0;
    file.seekg(0, std::ios::end);
    uint64_t right = static_cast<uint64_t>( file.tellg() );
    uint64_t mid;
    std::streamoff one_offset = static_cast<std::streamoff>(1);

    while(left <= right)
    {
        mid = (left + right) / 2;
        file.seekg(mid);
        
        while (file.tellg() > 0 && file.peek() != '\n')
            file.seekg(file.tellg() - one_offset);

        if (file.peek() == '\n')
            file.seekg(file.tellg() + one_offset);
        
        auto start = file.tellg();
        std::getline(file, line);
        if (std::regex_search(line, match, term_regex))
            curr_term = match[1].str();
            
        if (target_term < curr_term)
            right = mid - 1;
        else if (target_term > curr_term)
            left = mid + 1;
        else
            return static_cast<DocIdType>( std::stoul(match[2].str()) ); //returns the top 1 only
    }

    return DocIdType(0);
}






bool AuxiliaryIndex::ReadTermInfoFromDiskLog(const std::string& term, TermsTable& phrases_disk_table) //TODO
{
    if ( phrases_disk_table.find(term) != phrases_disk_table.end() )
        return false;
    
    size_t i = GetSegmentIndex(term);
    
    std::string index_filename = indexes_paths_[i].GetMainIndexPath() + "i" + std::to_string(i) + ".txt";
        
    std::ifstream file(index_filename);

    if (!file)
    {
        std::cerr << "Error opening file for reading: " << index_filename << '\n';
        return false;
    }
    
    std::regex term_regex("^([^ ]+):\\[([0-9,]+)\\]([^\\s\\n]+)$");
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
        if (std::regex_search(line, match, term_regex))
            tmp = match[1].str();
            
        if (term < tmp)
            right = mid - 1;
        else if (term > tmp)
            left = mid + 1;
        else
        {
            std::string curr = match[3].str();

            std::regex doc_pos_regex("([\\d]+)=\\d+(,\\d+)+;");
            std::regex num_regex("[0-9]+");
            
            std::string nums;
            DocIdType doc_id;
            PosType pos;

            for (std::sregex_iterator it(curr.begin(), curr.end(), doc_pos_regex), last; it != last; ++it)
            {
                nums = it->str();
            
                std::sregex_iterator ri(nums.begin(), nums.end(), num_regex), last_num;
                doc_id = static_cast<DocIdType>( std::stoul(ri->str()) );
                ++++ri; //pass frequency
                pos = static_cast<PosType>( std::stoul(ri->str()) );

                std::vector<PosType>& positions = phrases_disk_table[term].doc_pos_map[doc_id];
                positions.push_back(pos);

                while ( ++ri != last_num )
                    positions.push_back( static_cast<PosType>( std::stoul(ri->str()) ) );
            }
            
            return true;
        }
    }
    
    return false;
}


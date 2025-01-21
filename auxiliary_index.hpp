#ifndef AUXILIARY_INDEX_H
#define AUXILIARY_INDEX_H


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

#include <set>

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
    public:
		DescFreqRanking desc_freq_ranking;
		std::map<DocIdType, std::vector<PosType>> doc_pos_map;
		
    public:
		std::string RankingToString();
		std::string MapToString();
		std::string MapEntryToString(const size_t& doc_id);
		std::string MapEntryWithoutIdToString(const size_t& doc_id); //TYPES
		void UpdateRanking(const DocFreqEntry& new_entry, size_t num_top);
	};
	using TermsTable = std::unordered_map<TermType, TermInfo>;
	
	std::vector<TermsTable> table_;
				 
	size_t num_segments_;
	std::vector<std::unique_ptr<std::shared_mutex>> segments_;
	
	size_t num_top_doc_ids_ = 5; //can be set in constuctor
	
	size_t max_segment_size_ = 100;
	
	struct IndexPath
	{
	private:
		std::string main;
		std::string merge;
		std::unique_ptr<std::shared_mutex> mtx_ptr;
	public:
		IndexPath(const std::string& ma, const std::string& me, std::unique_ptr<std::shared_mutex> mp);
		
		void UpdateMainIndexPath();
		std::string GetMainIndexPath();
		std::string GetMergeIndexPath();
	};
	std::vector<IndexPath> indexes_paths_;
	
	struct Phrase
	{
    public:
		std::vector<TermType> words;
        std::vector<TermType> ai_words;
        std::vector<TermType> disk_words;
		std::vector<TermInfo*> ai_terms; //we can not have vectors of reference in simple way RENAME
		std::vector<TermInfo*> disk_terms;
		size_t words_distance;
        size_t ai_distance;
        size_t disk_distance;
		
    public:
		size_t FindIn(DocIdType doc_id, std::vector<TermInfo*>& terms, size_t distance); //somehow we need to make them wait for each other
	};

    //void MergeAiWithDisk(size_t i); //TODO
    size_t GetSegmentIndex(const TermType& term);
    bool ReadTermInfoFromDiskLog(const std::string& term, TermsTable& phrases_disk_table); //TODO
    DocIdType ReadFromDiskIndexLog(const std::string& term); //TODO
    void SplitIntoPhrases(std::string query, std::vector<Phrase>& phrases);

public:
	AuxiliaryIndex(size_t s, const std::string& ma, const std::string& me);

    DocIdType Read(const TermType& term);
	void Write(const TermType& term, const DocIdType& doc_id, const PosType& term_position);
    DocIdType ReadPhrase(const std::string& query);

	size_t SegmentSize(size_t i); //TODO syncronization

    void MergeAiWithDisk(size_t i); //TODO

	void ReadFromDiskLogGeneral(const std::string& target_term, std::smatch& matched_line);
};

#endif //AUXILIARY_INDEX_H
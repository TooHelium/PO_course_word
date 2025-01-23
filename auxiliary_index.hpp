#ifndef AUXILIARY_INDEX_H
#define AUXILIARY_INDEX_H

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
		std::string MapEntryToString(const DocIdType& doc_id);
		std::string MapEntryWithoutIdToString(const DocIdType& doc_id);
		void UpdateRanking(const DocFreqEntry& new_entry, size_t num_top);
	};
	using TermsTable = std::unordered_map<TermType, TermInfo>;
	
	std::vector<TermsTable> table_;
				 
	size_t num_segments_;
	std::vector<std::unique_ptr<std::shared_mutex>> segments_;
	
	size_t num_top_doc_ids_;
	
	size_t max_segment_size_;
	
	struct IndexPath
	{
	private:
		std::string main;
		std::string merge;
		std::unique_ptr<std::shared_mutex> mtx_ptr;
	public:
		IndexPath(const std::string& main_index_path, const std::string& merge_index_path, std::unique_ptr<std::shared_mutex> mp);
		
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
		std::vector<TermInfo*> ai_terms;
		std::vector<TermInfo*> disk_terms;
		size_t words_distance;
        size_t ai_distance;
        size_t disk_distance;
		
    public:
		size_t FindIn(DocIdType doc_id, std::vector<TermInfo*>& terms, size_t distance); //
	};

private:
    inline size_t GetSegmentIndex(const TermType& term);//
    void ReadTermInfoFromDisk(const TermType& target_term, TermsTable& phrases_disk_table);//
    DocFreqEntry ReadDocFreqEntryFromDisk(const TermType& target_term);//
    void SplitIntoPhrases(std::string query, std::vector<Phrase>& phrases); //
	void MergeAiWithDisk(size_t i);//
	DocIdType ReadOneWord(const TermType& term);//

public:
	AuxiliaryIndex(const std::string& main_index_path, const std::string& merge_index_path, 
				   size_t num_of_segments, size_t max_segment_size, size_t num_top_doc_ids);//

	void Write(const TermType& term, const DocIdType& doc_id, const PosType& term_position);//
    DocIdType ReadPhrase(const std::string& query);//
	size_t SegmentSize(size_t i);//
};

#endif //AUXILIARY_INDEX_H
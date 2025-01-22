#ifndef SHEDULER_H
#define SHEDULER_H

#include <string>
#include <regex>
#include <unordered_set>
#include <filesystem>
#include <memory>
#include <shared_mutex> 

#include "BS_thread_pool.hpp" 

#include "auxiliary_index.hpp"

class Sheduler 
{
public:
    AuxiliaryIndex* ai;
	BS::priority_thread_pool* pool;
	std::string data_path_;
	std::chrono::duration<size_t> duration_;
	std::unordered_set<std::filesystem::path> monitored_dirs_;
	std::unique_ptr<std::shared_mutex> mtx_ptr;
	std::regex id_range_regex;
	std::regex word_regex;
	const std::string ready_dir_marker_ = "___"; // 3 underscores
	
public:
	Sheduler(const std::string data_path, AuxiliaryIndex* aux_idx, BS::priority_thread_pool* thread_pool, size_t sleep_duration);

	void MonitorData();
	
	std::string GetPathByDocId(const uint32_t id);
	
	bool DirIsReady(const std::filesystem::path& path);
	
	void InspectDir(const std::string directory_path);

    void SplitFileInWords(const std::filesystem::path& file_path);
};

#endif //SHEDULER_H
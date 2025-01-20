#ifndef SHEDULER_H
#define SHEDULER_H

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "auxiliary_index.hpp"

//class AuxiliaryIndex;

class Sheduler 
{
public:
    AuxiliaryIndex* ai;
	std::string data_path_;
	std::chrono::duration<size_t> duration_;
	std::unordered_set<std::string> monitored_dirs_;
	const std::string ready_dir_marker_ = "___"; // 3 underscores
	
public:
	Sheduler(const std::string dp, AuxiliaryIndex* ai_many, size_t sleep_duration);

	void MonitorData();
	
	std::string GetPathByDocId(const uint32_t id);
	
	bool DirIsReady(const std::filesystem::path& path);
	
	void InspectDir(const std::string& directory_path);

    void Split(const std::filesystem::path& file_path);
};

#endif //SHEDULER_H
#include "sheduler.hpp"

#include <chrono> //for thread to sleep
#include <iostream>
#include <thread>
#include <regex>
#include <string>
#include <filesystem>
#include <fstream>

Sheduler::Sheduler(const std::string dp, AuxiliaryIndex* ai_many, size_t sleep_duration)
{
    if ( !(std::filesystem::exists(dp) && std::filesystem::is_directory(dp)) )
        throw std::invalid_argument("Data path does not exist or is not a directory");
    
    data_path_ = dp;
    ai = ai_many;
    duration_ = std::chrono::seconds( sleep_duration );
}

void Sheduler::MonitorData()
{
    std::string curr_dir;
    
    while (1)
    {
        std::this_thread::sleep_for(duration_);
        
        for (const auto& entry : std::filesystem::directory_iterator(data_path_))
        {
            if (std::filesystem::is_directory(entry.path()) && DirIsReady(entry.path()))
            {		
                curr_dir = entry.path().string();
            
                if (monitored_dirs_.find(curr_dir) == monitored_dirs_.end())
                {
                    std::cout << "New directory " << curr_dir << std::endl;
                    monitored_dirs_.insert(curr_dir);
                    InspectDir(curr_dir);
                }
            }
        }
    }
}

std::string Sheduler::GetPathByDocId(const uint32_t id)
{
    if (id == 0)
        return "none";

    std::regex id_range_regex("(\\d+)-(\\d+)");
    std::smatch match;

    for (const auto& entry : std::filesystem::directory_iterator(data_path_))
    {
        if (std::filesystem::is_directory(entry.path()) && DirIsReady(entry.path()))
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

bool Sheduler::DirIsReady(const std::filesystem::path& path)
{
    std::string path_stem = path.stem().string();
    
    if ( path_stem.substr(path_stem.length() - 3) == ready_dir_marker_ )
        return true;
    
    return false;
}

void Sheduler::InspectDir(const std::string& directory_path)
{
    for (const auto& entry : std::filesystem::directory_iterator(directory_path))
    {
        if (entry.path().extension() == ".txt")
        {
            //tasks_pool_.push_back(entry.path().string());
            Split(entry.path());
        }
    }
}

void Sheduler::Split(const std::filesystem::path& file_path)
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
			
			ai->Write(match_str, doc_id, word_position++);
		}
	}
	
	file.close();
}
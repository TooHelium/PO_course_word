#include "sheduler.hpp"
//#include "BS_thread_pool.hpp"

#include <chrono> //for thread to sleep
#include <iostream>
#include <thread>
#include <regex>
#include <string>
#include <filesystem>
#include <fstream>

Sheduler::Sheduler(const std::string dp, AuxiliaryIndex* ai_many, BS::thread_pool<4>* thread_pool, size_t sleep_duration)
{
    if ( !(std::filesystem::exists(dp) && std::filesystem::is_directory(dp)) )
        throw std::invalid_argument("Data path does not exist or is not a directory");
    
    data_path_ = dp;
    ai = ai_many;
    pool = thread_pool;
    duration_ = std::chrono::seconds( sleep_duration );
    mtx_ptr = std::make_unique<std::shared_mutex>();
    
    id_range_regex = std::regex("(\\d+)-(\\d+)");
    word_regex = std::regex("\\w+(['-]\\w+)*");
}

void Sheduler::MonitorData()
{
    std::filesystem::path curr_dir;

    while (1)
    {
        std::this_thread::sleep_for(duration_);
        
        for (const auto& entry : std::filesystem::directory_iterator(data_path_))
        {
            if (std::filesystem::is_directory(entry.path()) && DirIsReady(entry.path()))
            {		
                curr_dir = entry.path();

                std::unique_lock<std::shared_mutex> _(*mtx_ptr);

                if ( !monitored_dirs_.count(curr_dir) )
                {
                    //std::cout << "New directory " << curr_dir.string() << std::endl;

                    monitored_dirs_.insert(curr_dir);

                    InspectDir(curr_dir.string());
                }
            }
        }
    }
}

std::string Sheduler::GetPathByDocId(const uint32_t id)
{
    if (id == 0)
        return "none";

    std::smatch match;
    std::string tmp;
 
    std::shared_lock<std::shared_mutex> _(*mtx_ptr);

    for (const std::filesystem::path& dir_path : monitored_dirs_)
    {
        tmp = dir_path.stem().string();
        if (std::regex_search(tmp, match, id_range_regex))
        {
            uint32_t min_id = static_cast<uint32_t>( std::stoul(match[1].str()) );
            uint32_t max_id = static_cast<uint32_t>( std::stoul(match[2].str()) );
            
            if (min_id <= id && id <= max_id)
                return dir_path.string() + "/" + std::to_string(id) + ".txt";
        }
    }

    return "none";
}

bool Sheduler::DirIsReady(const std::filesystem::path& path)
{
    std::string path_stem = path.stem().string();
    
    if ( path_stem.substr(path_stem.length() - 3) == ready_dir_marker_ )
        return true;
    
    return false;
}

void Sheduler::InspectDir(const std::string directory_path)
{
    for (const auto& entry : std::filesystem::directory_iterator(directory_path))
    {
        if (entry.path().extension() == ".txt")
        {
            (void) pool->submit_task( [this, entry] {
                SplitFileInWords(entry.path());
            }, BS::pr::normal);
        }
    }
}

void Sheduler::SplitFileInWords(const std::filesystem::path& file_path)
{
	std::ifstream file(file_path.string());

	if (!file)
	{
		std::cerr << "Error opening file to split: " << file_path.string() << '\n';
		return;
	}
	
	std::string filename = file_path.stem().string();
	uint32_t doc_id = static_cast<uint32_t>( std::stoul(filename) );
		
	uint32_t word_position = 1;
	
	std::string line;

	while ( std::getline(file, line) )
	{
		auto first_word = std::sregex_iterator(line.begin(), line.end(), word_regex);
		auto last_word = std::sregex_iterator();
		
		for (std::sregex_iterator it = first_word; it != last_word; ++it)
		{
			std::smatch match = *it;
			std::string match_str = match.str();
			
			std::transform(match_str.begin(), match_str.end(), match_str.begin(), 
						   [](unsigned char c) { return std::tolower(c); }); 
			
			ai->Write(match_str, doc_id, word_position++);
		}
	}
}
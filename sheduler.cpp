#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm> 

#include "BS_thread_pool.hpp"

#include "sheduler.hpp"
#include "auxiliary_index.hpp"

extern std::mutex print_mutex;

Scheduler::Scheduler(const std::string data_path, AuxiliaryIndex* aux_idx, BS::priority_thread_pool* thread_pool, size_t sleep_duration)
{
    data_path_ = data_path;
    ai_ptr_ = aux_idx;
    pool_ptr_ = thread_pool;
    duration_ = std::chrono::seconds( sleep_duration );
    mtx_ptr_ = std::make_unique<std::shared_mutex>();
    
    id_range_regex_ = std::regex("(\\d+)-(\\d+)");
    word_regex_ = std::regex("\\w+(['-]\\w+)*");
}

void Scheduler::MonitorData()
{
    std::filesystem::path curr_dir;

    while (1)
    {
        std::this_thread::sleep_for(duration_);
        for (int i = 0; i < 10; ++i)
            std::cout << ai_ptr_->SegmentSize(i) << std::endl;
        
        for (const auto& entry : std::filesystem::directory_iterator(data_path_))
        {
            if (std::filesystem::is_directory(entry.path()) && DirIsReady(entry.path()))
            {		
                curr_dir = entry.path();

                std::unique_lock<std::shared_mutex> _(*mtx_ptr_);

                if ( !monitored_dirs_.count(curr_dir) )
                {
                    {
                        std::lock_guard<std::mutex> _(print_mutex);
                        std::cout << "New directory " << curr_dir.string() << std::endl;
                    }
                    monitored_dirs_.insert(curr_dir);

                    InspectDir(curr_dir.string());
                }
            }
        }
    }
}

std::string Scheduler::GetPathByDocId(const uint32_t id)
{
    if (id == 0)
        return "none";

    std::smatch match;
    std::string tmp;
 
    std::shared_lock<std::shared_mutex> _(*mtx_ptr_);

    for (const std::filesystem::path& dir_path : monitored_dirs_)
    {
        tmp = dir_path.stem().string();
        if (std::regex_search(tmp, match, id_range_regex_))
        {
            uint32_t min_id = static_cast<uint32_t>( std::stoul(match[1].str()) );
            uint32_t max_id = static_cast<uint32_t>( std::stoul(match[2].str()) );
            
            if (min_id <= id && id <= max_id)
                return dir_path.string() + "/" + std::to_string(id) + ".txt";
        }
    }

    return "none";
}

bool Scheduler::DirIsReady(const std::filesystem::path& path)
{
    std::string path_stem = path.stem().string();
    
    if ( path_stem.substr(path_stem.length() - 3) == ready_dir_marker_ )
        return true;
    
    return false;
}

void Scheduler::InspectDir(const std::string directory_path)
{
    for (const auto& entry : std::filesystem::directory_iterator(directory_path))
    {
        if (entry.path().extension() == ".txt")
        {
            (void) pool_ptr_->submit_task( [this, entry] {
                WordParser(entry.path());
            }, BS::pr::normal);
        }
    }
}

void Scheduler::WordParser(const std::filesystem::path& file_path)
{
	std::ifstream file(file_path.string());

	if (!file)
	{
        std::lock_guard<std::mutex> _(print_mutex);
		std::cerr << "Error opening file to split: " << file_path.string() << '\n';
		return;
	}
	
	std::string filename = file_path.stem().string();
	uint32_t doc_id = static_cast<uint32_t>( std::stoul(filename) );
		
	uint32_t word_position = 1;
	
	std::string line;

	while ( std::getline(file, line) )
	{
		auto first_word = std::sregex_iterator(line.begin(), line.end(), word_regex_);
		auto last_word = std::sregex_iterator();
		
		for (std::sregex_iterator it = first_word; it != last_word; ++it)
		{
			std::smatch match = *it;
			std::string match_str = match.str();
			
			std::transform(match_str.begin(), match_str.end(), match_str.begin(), 
						   [](unsigned char c) { return std::tolower(c); }); 
			
			ai_ptr_->Write(match_str, doc_id, word_position++);
		}
	}
}
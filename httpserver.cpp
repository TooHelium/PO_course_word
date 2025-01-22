#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <regex>
#include <thread>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "BS_thread_pool.hpp"

#include "sheduler.cpp"
#include "auxiliary_index.cpp"

#define PORT 8080
#define MY_ADDR "192.168.0.100"
#define LOCAL_HOST "127.0.0.1"

#define HTML_ROOT "./index.html"

#define STATUS_404 ("HTTP/1.1 404 Not Found\r\n" \
                    "Content-Type: text/html; charset=UTF-8\r\n" \
                    "Content-Length: 158\r\n" \
                    "\r\n" \
                    "<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>")

#define STATUS_200(content) ("HTTP/1.1 200 OK\r\n" \
                             "Content-Type: text/html; charset=UTF-8\r\n" \
                             "Content-Length: " + std::to_string(content.size()) + "\r\n" \
                             "\r\n" \
                             + content)

#define STATUS_500 ("HTTP/1.1 500 Internal Server Error\r\n" \
                    "Content-Type: text/html; charset=UTF-8\r\n" \
                    "Content-Length: 158\r\n" \
                    "\r\n" \
                    "<!DOCTYPE html><html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>")


std::string content_root;

std::mutex print_mutex;

std::regex path_root("^GET / HTTP/1.1");
std::regex query_regex("^GET /search\\?query=([^ ]+) HTTP/1.1");

using DecodeRule = std::pair<std::regex, std::string>;

std::vector<DecodeRule> url_decoding_map = {
     {std::regex("%20|\\+"), " "},
     {std::regex("%22"), "\""}, 
     {std::regex("%27"), "'"}, 
     {std::regex("%28"), "("}, 
     {std::regex("%29"), ")"}, 
     {std::regex("%2C"), ","}, 
     {std::regex("%2E"), "."} 
};

inline void DecodeUrl(std::string& enc_url)
{
    for (const DecodeRule& pair : url_decoding_map)
        enc_url = std::regex_replace(enc_url, pair.first, pair.second);
}

void HandleRequest(int& client_socket, AuxiliaryIndex& ai, Sheduler& sheduler) 
{
    const size_t kMaxBufferSize = 512;
    char request[kMaxBufferSize];

    int recved = recv(client_socket, request, sizeof(request) - 1, 0);
    
    if (recved < 0)
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Error receiving request from client" << std::endl;
        close(client_socket);
        return;
    }
    else
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Error client closed connection" << std::endl;
        close(client_socket);
        return;
    }

    request[recved] = '\0';
    std::string request_str(request); 

    std::string http_response;
    std::smatch match;

    if (std::regex_search(request_str, path_root))
        http_response = STATUS_200(content_root);
    else if (std::regex_search(request_str, match, query_regex))
    {
        std::string query = match[1].str();
        DecodeUrl(query);

        std::string path = sheduler.GetPathByDocId( ai.ReadPhrase(query) );

        if (path == "none")
            http_response = STATUS_404;
        else
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                http_response = STATUS_500;
            else
            {
                std::ostringstream ss;
                ss << file.rdbuf();
                http_response = STATUS_200( ss.str() );
            }
        }
    }
    else
        http_response = STATUS_404;

    send(client_socket, http_response.c_str(), http_response.size(), 0); 

    close(client_socket);
}

bool CreateDirectory(const std::string& path)
{
    std::filesystem::path dir(path);

    if ( !std::filesystem::exists(dir) )
    {
        if (std::filesystem::create_directories(dir))
            std::cout << "Directory created: " << path << '\n';
        else
        {
            std::cerr << "Fail to create directory: " << path << '\n';
            return false;
        }
    }
    else
        std::cout << "Warning: directory already exists: " << path << '\n';
    
    return true;
}

int main() 
{
    std::cout << "------Index creation------\n";

    std::string main_index_path;
    std::string merge_index_path;
    size_t num_of_segments;
    size_t max_segment_size;
    size_t num_top_doc_ids;

repeat_main:
    std::cout << "Enter path to the main index: "; 
    std::getline(std::cin, main_index_path);
    if ( !CreateDirectory(main_index_path) )
        goto repeat_main;

repeat_merge:
    std::cout << "Enter path to the merge index: "; 
    std::getline(std::cin, merge_index_path); 
    if ( !CreateDirectory(merge_index_path) )
        goto repeat_merge;

    std::cout << "Enter numbers of index's segments: "; std::cin >> num_of_segments;
    std::cout << "Enter max segment's size: "; std::cin >> max_segment_size;
    std::cout << "Enter number of monitored top documents: "; std::cin >> num_top_doc_ids;

    AuxiliaryIndex ai(main_index_path, merge_index_path, num_of_segments, max_segment_size, num_top_doc_ids);

    std::cout << "------Thread pool creation------\n";

    size_t num_threads;
    std::cout << "Enter number of threads in pool: "; std::cin >> num_threads;

    BS::priority_thread_pool pool(num_threads); 

    std::cout << "------Sheduler creation------\n";

    std::string data_path;
    std::size_t seconds_to_sleep;

repeat_data:
    std::cout << "Enter path to data: "; 
    std::cin.ignore();
    std::getline(std::cin, data_path);
    if ( !CreateDirectory(data_path) )
        goto repeat_data;

    std::cout << "Enter time period (in seconds) to inspect new directories: "; std::cin >> seconds_to_sleep;

    Sheduler sheduler(data_path, &ai, &pool, seconds_to_sleep);

    std::thread t_s(&Sheduler::MonitorData, &sheduler);

    std::ifstream file(HTML_ROOT);
    if (file) 
    {
        content_root = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    else
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Cannot load HTML content (root)\n";
        return 1;
    }
    
    //////////////////////

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_socket) 
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Server socket creation failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    server_addr.sin_port = htons(PORT);

    if (-1 == bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) 
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Bind failed\n";
        close(server_socket);
        return 1;
    }

    if (-1 == listen(server_socket, 120)) 
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cerr << "Listen failed\n";
        close(server_socket);
        return 1;
    }
    
    {
        std::lock_guard<std::mutex> _(print_mutex);
        std::cout << "Server (" << LOCAL_HOST << ") listening on port " << PORT << "...\n";
    }

    while (true) 
    {
        int client_socket = accept(server_socket, NULL, NULL);
        if (-1 == client_socket) 
        {
            std::lock_guard<std::mutex> _(print_mutex);
            std::cerr << "Accept failed\n";
            continue;
        }


        (void) pool.submit_task([&client_socket, &ai, &sheduler] {
            HandleRequest(client_socket, std::ref(ai), std::ref(sheduler));
        }, BS::pr::high);
    }

    close(server_socket);
    
    return 0;
}
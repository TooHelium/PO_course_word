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

#include "main.cpp"
#include "sheduler.cpp"

#define PORT 8080
#define MY_ADDR "192.168.0.100"
#define LOCAL_HOST "127.0.0.1"

#define HTML_ROOT "index.html"

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

void DecodeUrl(std::string& enc_url)
{
    for (const DecodeRule& pair : url_decoding_map)
        enc_url = std::regex_replace(enc_url, pair.first, pair.second);
}

void HandleRequest(int& client_socket, AuxiliaryIndex& ai_many, Sheduler& sheduler) 
{
    const size_t kMaxBufferSize = 512;
    char request[kMaxBufferSize];

    int recved = recv(client_socket, request, sizeof(request) - 1, 0);
    
    if (recved < 1)
    {
        close(client_socket); //std::cout << "ERROR receiving request from client" << std::endl;
        return;
    }
    else if (0 == recved)
    {
        close(client_socket); //std::cout << "ERROR client closed connection" << std::endl;
        return;
    }

    request[recved] = '\0';
    std::string request_str(request); 

    std::string http_response;
    std::regex path_root("^GET / HTTP/1.1");
    std::regex query_regex("^GET /search\\?query=([^ ]+) HTTP/1.1");
    std::smatch match;

    if (std::regex_search(request_str, path_root))
        http_response = STATUS_200(content_root);
    else if (std::regex_search(request_str, match, query_regex))
    {
        std::string query = match[1].str();
        DecodeUrl(query);

        std::string path = sheduler.GetPathByDocId( ai_many.ReadPhrase(query) );

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

    //sleep(1);
    close(client_socket);
}


int main() 
{
    size_t num_of_segments = 10;          
	std::string ma = "/home/dima/Desktop/БІС/test IR/Новая папка (copy)/main index/";
	std::string me = "/home/dima/Desktop/БІС/test IR/Новая папка (copy)/merged index/";
	AuxiliaryIndex ai_many(num_of_segments, ma, me);

    std::thread t_ai(run, std::ref(ai_many));
    t_ai.detach();
    ////////////////////////////

    std::cout << "Creating thread pool...\n"; 
    BS::thread_pool<4> pool(4);

    //////////////////////////

    std::cout << "Creating Sheduler..." << std::endl;
    Sheduler s("/home/dima/Desktop/БІС/test IR/Новая папка (copy)/data/", &ai_many, &pool, 5);
	std::thread t_s(&Sheduler::MonitorData, &s);
    t_s.detach();

    ///////////////////


    std::ifstream file(HTML_ROOT, std::ios::in);
    if (file.is_open()) 
    {
        content_root = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    else
    {
        std::cerr << "Cannot load HTML content (root)\n";
        return 1;
    }
    
    //////////////////////

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_socket) 
    {
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
        std::cerr << "Bind failed\n";
        close(server_socket);
        return 1;
    }

    if (-1 == listen(server_socket, 120)) 
    {
        std::cerr << "Listen failed\n";
        close(server_socket);
        return 1;
    }
    
    std::cout << "Server (" << LOCAL_HOST << ") listening on port " << PORT << "...\n";

    while (true) 
    {
        int client_socket = accept(server_socket, NULL, NULL);
        if (-1 == client_socket) 
            continue; //std::cerr << "Accept failed\n";


        (void) pool.submit_task([&client_socket, &ai_many, &s] {
            HandleRequest(client_socket, std::ref(ai_many), std::ref(s));
        }, BS::pr::high);
    }

    close(server_socket);
    
    return 0;
}
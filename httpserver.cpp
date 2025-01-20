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

#include "main.cpp"

#define PORT 8080
#define MY_ADDR "192.168.0.100"
#define LOCAL_HOST "127.0.0.1"

#define HTML_ROOT "index.html"
#define HTML_SECOND "second.html"

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

std::string content_root;
std::string content_second;

std::regex path_root("^GET / HTTP/1.1");
std::regex path_second("^GET /second.html HTTP/1.1");

using DecodeRule = std::pair<std::regex, std::string>;

std::vector<DecodeRule> url_decoding_map = { //TODO RENAME DUE TO GOOGLE CONVENTIONS
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

void HandleRequest(int client_socket, AuxiliaryIndex& ai_many) 
{
    const size_t kMaxBufferSize = 512;
    char request[kMaxBufferSize];

    int recved = recv(client_socket, request, sizeof(request) - 1, 0);
    
    if (recved < 1)
    {
        std::cout << "ERROR receiving request from client" << std::endl;
        close(client_socket);
        return;
    }
    else if (0 == recved)
    {
        std::cout << "ERROR client closed connection" << std::endl;
        close(client_socket);
        return;
    }

    request[recved] = '\0';
    std::string request_str(request); 

    std::string http_response;
    std::regex query_regex("^GET /search\\?query=([^ ]+) HTTP/1.1");
    std::smatch match;

    if (std::regex_search(request_str, path_root))
        http_response = STATUS_200(content_root);
    else if (std::regex_search(request_str, match, query_regex))
    {
        std::string enc_url = match[1].str();
        DecodeUrl(enc_url);
        std::string doc_id = std::to_string( ai_many.ReadPhrase(enc_url) );
        
        http_response = STATUS_200(doc_id);
    }
    else
        http_response = STATUS_404;

    send(client_socket, http_response.c_str(), http_response.size(), 0); 

    sleep(1);
    close(client_socket);
}

int main() 
{
    size_t num_of_segments = 10;          
	std::string ma = "/home/dima/Desktop/БІС/test IR/Новая папка/main index/";
	std::string me = "/home/dima/Desktop/БІС/test IR/Новая папка/merged index/";
	AuxiliaryIndex ai_many(num_of_segments, ma, me);

    std::thread t(run, std::ref(ai_many));
    t.detach();

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

    /*file = std::ifstream(HTML_SECOND, std::ios::in);
    if (file.is_open())
    {
        content_second = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    else
    {
        std::cerr << "Cannot load HTML content (second)\n";
        return 1;
    }*/
    

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

    if (-1 == listen(server_socket, 5)) 
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
        {
            std::cerr << "Accept failed\n";
            continue;
        }

        HandleRequest(client_socket, std::ref(ai_many));
        //here must be thread pool 
    }

    close(server_socket);
    
    return 0;
}






/*
void SendResponse(int client_socket, const std::string& response) 
{
    send(client_socket, response.c_str(), response.size(), 0);
}
*/
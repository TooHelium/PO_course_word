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

#define PORT 8080
#define MY_ADDR "192.168.0.100"

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

std::regex path_root("^GET / HTTP/1.1"),
           path_second("^GET /second.html HTTP/1.1");

void SendResponse(int client_socket, const std::string& response) 
{
    send(client_socket, response.c_str(), response.size(), 0);
}

void HandleRequest(int client_socket, bool* thread_is_finished) 
{
    char request[256];
    int recved = recv(client_socket, request, sizeof(request), 0);
    if (-1 == recved)
    {
        std::cout << "ERROR receiving request from client" << std::endl;
        close(client_socket);
        return;
    }

    std::string httpResponse;

    if (std::regex_search(request, path_root))
    {
        httpResponse = STATUS_200(content_root);
    }
    else if (std::regex_search(request, path_second))
    {
        httpResponse = STATUS_200(content_second);
    }
    else
    {
        httpResponse = STATUS_404;
    }

    SendResponse(client_socket, httpResponse); 

    sleep(1); //in order to let the client to get all the packets
    close(client_socket);

    *thread_is_finished = true;
}

int main() 
{
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

    file = std::ifstream(HTML_SECOND, std::ios::in);
    if (file.is_open())
    {
        content_second = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    else
    {
        std::cerr << "Cannot load HTML content (second)\n";
        return 1;
    }
    

    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_socket) 
    {
        std::cerr << "Server socket creation failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(MY_ADDR);
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
    
    std::cout << "Server (" << MY_ADDR << ") listening on port " << PORT << "...\n";

    while (true) 
    {
        int client_socket = accept(server_socket, NULL, NULL);
        if (-1 == client_socket) 
        {
            std::cerr << "Accept failed\n";
            continue;
        }

        //here must be thread pool 
    }

    close(server_socket);
    
    return 0;
}




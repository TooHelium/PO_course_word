#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h> 
#include <thread>
#include <vector>
#include <iostream>
#include <cstdint>
#include <regex>

#define PORT 1234
#define LOCAL_HOST "127.0.0.1"

void error(const char* msg)
{
    perror(msg); 
    exit(1);
}

void workWithClient(const int sockfd, bool* thread_is_finished)
{
    int sended, recved;
    char msg[50] = "Connection established";
    sended = send(sockfd, msg, sizeof(msg), 0);
    if (-1 == sended)
    {
       perror("ERROR sending connection established");
    }

    bool is_configured = false, 
         is_processing = false,
         task_is_done = false,
         data_is_send = false;

    int* matrix = nullptr;
    int matrix_size, 
        thread_count;
    
    std::regex config("\\s*config\\s+size=(\\d+)\\s+threads=(\\d+)\\s*"),
               start_processing("\\s*start processing\\s*"),
               get_status("\\s*get status\\s*"),
               get_result("\\s*get result\\s*"),
               send_data("\\s*send data\\s*");
    std::cmatch match;

    std::string response;
    std::thread task_thread;


    while (true)
    {
        char request[100];
        recved = recv(sockfd, request, sizeof(request), 0);
        if (-1 == recved)
        {
            perror("ERROR receiving message");
        }
        else
        {
            if (std::regex_match(request, match, config))
            {
                if (is_processing)
                {
                    response = "Processing... Can not change configurations";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending can not change configurations");
                    }
                }
                else
                {
                    if (match.size() == 3)
                    {
                        int msize = std::stoi(match[1].str());  
                        int mthreads = std::stoi(match[2].str());

                        if ( !(msize >= 1 && msize <= 15000 && mthreads >= 1 && mthreads <= 8) )
                        {
                            goto invalid_arguments;
                        }
                        std::cout << "Configs: " << msize << " " << mthreads << std::endl;
                        is_configured = true;
                        matrix_size = msize;
                        thread_count = mthreads;

                        response = "Configured size = " + match[1].str() + " threads = " + match[2].str();
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending invalid arguments");
                        }
                    }
                    else
                    {
                        invalid_arguments:;
                        response = "Invalid arguments (must be size <= 15000, threads <= 8)";
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending invalid arguments");
                        }
                    }
                }
            }
            else if (std::regex_match(request, send_data))
            {
                if (is_processing)
                {
                    response = "Processing... Can not send data";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending can not send data");
                    }
                }
                else
                {
                    if (is_configured)
                    {
                        std::string ready = "ready to take data";
                        sended = send(sockfd, ready.c_str(), ready.length() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending ready to take data");
                            continue;
                        }


                        if (matrix != nullptr)
                        {
                            delete[] matrix;
                            matrix = nullptr;
                        }

                        /////////////////////////////////////
                        int recved_bytes = matrix_size * matrix_size * sizeof(int);
                        int packet_no = 0;
                        const int max_packet_size = 65500; 
                        matrix = new int[matrix_size * matrix_size];
                        int* start = matrix;
                        int step_addr = 0;

                        int all_recved = 0;

                        while (recved_bytes > 0)
                        {
                            recved = recv(sockfd, start, std::min(max_packet_size, recved_bytes), 0); 

                            if (-1 == recved)
                            {
                                printf("ERROR recv %d packet", packet_no);
                                error("ERROR");
                            }
                            //else
                            //{
                            //    printf("Received - %d\n", recved);
                            //}

                            all_recved += recved / sizeof(int);

                            if (recved % sizeof(int) == 0)
                            {
                                recved_bytes -= recved;
                                ++packet_no;
                                step_addr = (recved / sizeof(int)); 
                                
                                start += step_addr;
                            }
                            else
                            {
                                int q = recved % sizeof(int);

                                recved_bytes -= recved;
                                ++packet_no;
                                step_addr = (recved / sizeof(int));

                                start += step_addr;
                                char* temp = (char*)start;
                                temp += q;
                                start = (int*)temp;
                            }

                        }

                        data_is_send = true;

                        response = "All data is send. Waiting to start processing";
                        std::cout << response << std::endl;
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending all data is send");
                        }

                        //std::cout << "All recved = " << all_recved << std::endl;
                        /////////////////////////////////////
                    }
                    else
                    {
                        response = "Must to configurate first 2";
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending must to configurate 2");
                        }
                    }
                }
            }
            else if (std::regex_match(request, start_processing))
            {
                if (is_processing)
                {
                    response = "Already processing";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending already processing");
                    }
                }
                else
                {
                    if (is_configured && data_is_send)
                    {
                        task_thread = std::thread(startProcessing, matrix, matrix_size, thread_count, &task_is_done); //DO NOT FORGET TO .JOIN
                        is_processing = true;
                        
                        response = "Start processing...";
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending start processing");
                        }
                    }
                    else
                    {
                        response = "Must to configurate first or send data";
                        sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                        if (-1 == sended)
                        {
                            perror("ERROR sending configurate first");
                        }
                    }
                }
                
            }
            else if (std::regex_match(request, get_status))
            {
                if (task_is_done)
                {
                    response = "Task is done";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending task is done");
                    }
                }
                else
                {
                    response = "Task is not done yet or even not processing";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending task is not done yet");
                    }
                }
            }
            else if (std::regex_match(request, get_result))
            {
                if (task_is_done)
                {
                    task_thread.join();

                    response = "ready to send result";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending ready to send result");
                    }

                    sleep(0.5);

                    sended = send(sockfd, matrix, matrix_size*matrix_size*sizeof(int), 0);
                    if (-1 == sended)
                        perror("ERROR sending matrix");

                    break;
                }
                else
                {
                    response = "Task is not done or even not processing";
                    sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                    if (-1 == sended)
                    {
                        perror("ERROR sending task is not done or even not processing");
                    }
                }
            }
            else
            {
                response = "Unknown command";
                sended = send(sockfd, response.c_str(), response.size() + 1, 0);
                if (-1 == sended)
                {
                    perror("ERROR sending unknown command");
                }
            }
        }
    }

    if (close(sockfd) == -1)
        perror("ERROR closing socket");

    if (matrix != nullptr)
        delete[] matrix;

    std::cout << "Ended with client" << std::endl;

    *thread_is_finished = true;
}

int main(int argc, char *argv[])
{ 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
        error("ERROR opening socket");

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);

    if (bind(sockfd, (struct sockaddr*) &my_addr, sizeof(my_addr)) == -1)
        error("ERROR binding socket");

    if (listen(sockfd, 3) == -1)
        error("ERROR listening");

    std::vector<std::pair<bool*, std::thread>> clients;

    while (1)
    {   
        int newsockfd = accept(sockfd, NULL, NULL);

        if (-1 == newsockfd)
        {
            perror("ERROR accepting peer");
            continue;
        }

        bool* thread_is_finished = new bool(false);
        std::thread t(workWithClient, newsockfd, thread_is_finished);
        clients.emplace_back(thread_is_finished, std::move(t));

        for (auto client = clients.begin(); client != clients.end();)
        {
            if (*(client->first))
            {
                if (client->second.joinable())
                {
                    client->second.join();
                    delete client->first;
                    client = clients.erase(client);
                    continue;
                }
            }
            
            ++client;
        }

        std::cout << "Size " << clients.size() << std::endl;
    }
    
    if (close(sockfd) == -1)
        error("ERROR closing socket");

    printf("END\n");

    return 0; 
}
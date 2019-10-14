//
// Simple chat client for TSAM-409
//
// Command line: ./_client 127.0.0.1 4000 
//
// Author: Gabríel Sighvatsson (gabriels17@ru.is) & Karl Bachmann (karl19@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <thread>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <sys/time.h>
#include <fstream>

// Threaded function for handling responss from server

void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));
       std::cout << "----- frá server ---- " << std::endl;

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          exit(0);
       }
       else if(nread > 0)
       {
          printf("%s\n", buffer);
       }
       printf("here\n");
    }
}

int main(int argc, char* argv[])
{
   struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   int nwrite;                               // No. bytes written to server
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt

   if(argc != 3)
   {
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        exit(0);
   }

   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

   if(getaddrinfo(argv[1], argv[2], &hints, &svr) != 0)
   {
       perror("getaddrinfo failed: ");
       exit(0);
   }

   struct hostent *server;
   server = gethostbyname(argv[1]);

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   bcopy((char *)server->h_addr,
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);
   serv_addr.sin_port = htons(atoi(argv[2]));

   serverSocket = socket(AF_INET, SOCK_STREAM, 0);

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
       printf("Failed to set SO_REUSEADDR for port %s\n", argv[2]);
       perror("setsockopt failed: ");
   }

   
   if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
   {
       printf("Failed to open socket to server: %s\n", argv[1]);
       perror("Connect failed: ");
       exit(0);
   }

    // Listen and print replies from server
   std::thread serverThread(listenServer, serverSocket);

   finished = false;
   while(!finished)
   {
       std::cout << ">";
       bzero(buffer, sizeof(buffer));

       fgets(buffer, sizeof(buffer), stdin);

       nwrite = send(serverSocket, buffer, strlen(buffer),0);
       // timestamp on send
       struct timeval sendTime;
       gettimeofday(&sendTime, NULL);
       std::ofstream myfile;
       myfile.open ("log.txt", std::ios::app);
       myfile << "Message sent from client timestamp: " << buffer << ctime(&sendTime.tv_sec);
       myfile.close();
       
       std::cout << "Message sent Timestamp: " << ctime(&sendTime.tv_sec);
       
       //clear the buffer        
       
       memset(&buffer, 0, sizeof(buffer));
       
       //nread = recv(serverSocket, (char*)&buffer, sizeof(buffer), 0);
               
       // timestamp on receive
       //gettimeofday(&recvTime, NULL);
       //std::cout << "Message received Timestamp: " << ctime(&recvTime.tv_sec);

       if(nwrite  == -1)
       {
           perror("send() to server failed: ");
           finished = true;
       }
       
   }
}

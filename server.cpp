//
// Not so simple botnet server for TSAM-409
//
// Command line: ./tsamvgroup6 <client-port> <server-port>
//
// Authors: Gabríel Sighvatsson (gabriels17@ru.is) & Karl Bachmann (karl19@ru.is)
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

#include <ifaddrs.h>
#include <net/if.h>

#include <algorithm>
#include <map>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace std;

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
    public:
        int sock;              // socket of client connection
        string ip;
        int port;           // Limit length of name of client's user

        Client(int socket, string ip, int port)
        {
            sock = socket;
            this->ip = ip;
            this->port = port;
        } 

        ~Client(){}            // Virtual destructor defined for base class
};

class Server
{
    public:
        int sock;              // socket of client connection
        string name;
        string ip;
        int port;
        time_t keepalive_t;

        Server(int socket, string ip, int port)
        {
            sock = socket;
            this->ip = ip;
            this->port = port;
            keepalive_t = time(&keepalive_t);
        } 

        Server(int socket, string name, string ip, int port)
        {
            sock = socket;
            this->name = name;
            this->ip = ip;
            this->port = port;
            keepalive_t = time(&keepalive_t);
        } 

        ~Server(){}            // Virtual destructor defined for base class
};

class Message {
    public:
        string group;
        string fromGroup;
        string message;

        Message(string group, string from_group, string message)
        {
            this->group = group;
            this->fromGroup = fromGroup;
            this->message = message;
        }

        ~Message(){}
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

map<int, Client*> clients; // Lookup table for per Client information
map<int, Server*> servers; // Lookup table for per Client information
vector<Message*> msgs; // Vector for messages

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

    // Create socket for connection. Set to be non-blocking, so recv will
    // return immediately if there isn't anything waiting to be read.
    #ifdef __APPLE__     
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket");
        return(-1);
    }
    #else
    if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
    {
        perror("Failed to open socket");
        return(-1);
    }
    #endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
    // program exit.

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }
    set = 1;
    #ifdef __APPLE__     
    if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SOCK_NOBBLOCK");
    }
    #endif
    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family      = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons(portno);

    // Bind to socket to listen for connections from clients

    if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return(-1);
    }
    else
    {
        return(sock);
    }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
    // Remove client from the clients list
    clients.erase(clientSocket);

    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if(*maxfds == clientSocket)
    {
        for(auto const& p : clients)
        {
            *maxfds = max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.

    FD_CLR(clientSocket, openSockets);
}

void closeServer(int serverSocket, fd_set *openSockets, int *maxfds)
{
    // Remove server from the servers list
    servers.erase(serverSocket);

    // If this server's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if(*maxfds == serverSocket)
    {
        for(auto const& p : servers)
        {
            *maxfds = max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.

    FD_CLR(serverSocket, openSockets);
}

string get_my_ip()
{
    struct ifaddrs *myaddrs, *ifa;
    void *in_addr;
    char buf[64];
    string output = "BATMAN!";

    // get every ip address and put them in a linked list
    if(getifaddrs(&myaddrs) != 0)
    {
        perror("getifaddrs");
        exit(1);
    }

    // for every ip address in the linked list
    for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        // safety checks
        if (ifa->ifa_addr == NULL)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        switch (ifa->ifa_addr->sa_family)
        {
            // if its IPv4 then we look further
            case AF_INET:
            {
                struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
                in_addr = &s4->sin_addr;
                break;
            }
            // we are not interested in IPv6 (sorry...)
            case AF_INET6:
            {
                continue;
            }
            default:
                continue;
        }

        // we put our IPv4 into char buffer for output
        if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf)))
        {
            printf("%s: inet_ntop failed!\n", ifa->ifa_name);
        }
        // make sure we aren't looking at localhost
        else if (string(buf).compare("127.0.0.1") != 0)
        {
            // this assumes there are at most two ip addresses and that one of them is localhost
            output = string(buf);
            break;
        }
    }

    freeifaddrs(myaddrs);
    return output;
}

string addTokens(string tokens) {
    string tokenString = "";
    string str = tokens;
    string startToken = "\x01";
    string endToken = "\x04";

    tokenString = startToken + str + endToken;

    return tokenString;
}

string removeTokens(string tokens) {
    string str = tokens;
    if(str.find("\x01") == 0 && str.compare(str.size() - 1, 1, "\x04") == 0) {
        str.erase(0, 1);
        str.erase(str.size() - 1, str.size());
    }
    return str;
}

vector<string> parseTokens(string msg, char delim)
{
    vector<string> tokens;
    string token;

    // Split command from client into tokens for parsing
    stringstream stream(msg);

    size_t found = msg.find(delim);

    // If the specified delimiter is not found, the found variable will be equal to the const npos or -1
    // Then parse by whitespace
    if(found == string::npos) {
        while(stream >> token) {
            tokens.push_back(token);
        }
    }
    // If a delimeter was found parse by that delimeter
    else {
        while(getline(stream, token, delim)) {
            tokens.push_back(token);
        }
    }

    return tokens;
}


// Process command from client on the server
void clientCommand(int sock, fd_set *openSockets, int *maxfds, char *buffer, int myPort) 
{
    vector<string> tokens = parseTokens(buffer, ',');

    if((tokens[0].compare("\x01LISTSERVERS") == 0))
    {
        cout << "in listservers" << endl;
        string msg = "\x01SERVERS,P3_GROUP_6," + get_my_ip() + "," + to_string(myPort) + ";";

        for (auto const& server : servers)
        {
            if (server.second->name.length() != 0)
            {
                msg += server.second->name + "," + server.second->ip + "," + to_string(server.second->port) + ";";
            }
        }
        msg += "\x04";
        send(sock, msg.c_str(), msg.length(), 0);
    }

    else if(tokens[0].compare("SERVERS") == 0) {
        
        vector<string> groups = parseTokens(buffer, ';');
        vector<string> groupInfo = parseTokens(groups[0], ',');

        servers[sock]->name = groupInfo[1];
        servers[sock]->ip = groupInfo[2];
        servers[sock]->port = stoi(groupInfo[3]);
    }

    else if(tokens[0].compare("LISTSERVERS") == 0)
    {
        string msg = "\x01LISTSERVERS,P3_GROUP_6\x04";

        for(auto const& pair : servers)
        {
            Server *server = pair.second;
            //printf("New connection, socket fd is %d , ip is : %s , port : %d\n" , server->sock, server->myip.c_str(), server->myport);

            send(server->sock, msg.c_str(), msg.length() + 1, 0);
            recv(server->sock, buffer, sizeof(buffer), 0);
            send(sock, buffer, sizeof(buffer), 0);
        }

        //cout << buffer << endl;
    }
    else if((tokens[0].compare("CONNECT") == 0) && (tokens.size() == 3))
    {
        int newsock;
        if((newsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("Failed to open socket");
        }

        struct sockaddr_in new_addr;
        new_addr.sin_family      = AF_INET;
        new_addr.sin_addr.s_addr = inet_addr(tokens[1].c_str());
        new_addr.sin_port        = htons(stoi(tokens[2]));
        
        if (connect(newsock, (sockaddr*)&new_addr, sizeof(new_addr)) < 0) {
            perror("Failed to connect");
        }

        // Great success
        string name = "P3_GROUP_6";
        servers[newsock] = new Server(newsock, name, tokens[1], stoi(tokens[2]));
    }
    else if(tokens[0].compare("LEAVE") == 0 && tokens.size() == 3)
    {
        string ip = tokens[1];
        int port = stoi(tokens[2]);
        int sock;
        
        for (auto const& server : servers) {
            if (server.second->ip == ip && server.second->port == port)
            {
                cout << "Found a server hosted on: " << ip << ":" << port << endl;
                sock = server.second->sock;
            }
        }

        // Close the socket, and leave the socket handling
        // code to deal with tidying up clients etc. when
        // select() detects the OS has torn down the connection.
        
        closeServer(sock, openSockets, maxfds);
    }
    else if(tokens[0].compare("WHO") == 0)
    {
        cout << "Who is logged on" << endl;
        string msg;

        for(auto const& names : servers)
        {
            msg += names.second->name + ",";
        }

        // Reducing the msg length by 1 loses the excess "," - which
        // granted is totally cheating.
        send(sock, msg.c_str(), msg.length() - 1, 0);

    }
    // This is slightly fragile, since it's relying on the order
    // of evaluation of the if statement.
    else if((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0))
    {
        string msg;
        for(auto i = tokens.begin()+2;i != tokens.end();i++) 
        {
            msg += *i + " ";
        }

        for(auto const& pair : clients)
        {
            send(pair.second->sock, msg.c_str(), msg.length(),0);
        }
    }
    else if(tokens[0].compare("MSG") == 0)
    {
        for(auto const& pair : servers)
        {
            if(pair.second->name.compare(tokens[1]) == 0)
            {
                string msg;
                for(auto i = tokens.begin()+2;i != tokens.end();i++) 
                {
                    msg += *i + " ";
                }
                send(pair.second->sock, msg.c_str(), msg.length(),0);
            }
        }
    }
    else if(tokens[0].compare("KEEPALIVE") == 0) {
        time_t t;
        time(&t);
        servers[sock]->keepalive_t = t;
        if(!tokens[1].compare("0") == 0) {
            string str = addTokens("GET_MSG,P3_GROUP_6");
            send(sock, str.c_str(), str.length(), 0);
        }
    }
    else if(tokens[0].compare("GET_MSG") == 0)
    {
        int counter = 0;
        for(auto const& msg : msgs)
        {
            if(msg->group == tokens[1])
            {
                string str = "SEND_MSG," + msg->fromGroup + "," + msg->group + "," + msg->message;
                str = addTokens(str);
                send(sock, str.c_str(), str.length(), 0);
                msgs.erase(msgs.begin() + counter);
                counter++;
            }
        }
    }
    else if(tokens[0].compare("GETMSG") == 0) {
        for(auto const& msg : msgs)
        {
            if(msg->group == tokens[1]) {
                string str = msg->message;
                str = addTokens(str);
                send(sock, str.c_str(), str.length(), 0);
            }
        }

        string str = addTokens(buffer);
        send(sock, str.c_str(), str.length(), 0);
    }
    else if(tokens[0].compare("SEND_MSG") == 0){
        
        bool msg_sent = false;
        for(auto const& pair : servers) {
            if(pair.second->name.compare(tokens[2]) == 0) {
                send(pair.second->sock, buffer, sizeof(buffer), 0);
                msg_sent = true;
            }
        }
        if(!msg_sent) {
            string message;
            message = tokens[3];
            if(tokens.size() > 4) {
                for(unsigned int i = 4; i < tokens.size(); i++) {
                    message += "," + tokens[i];
                }
                
            strcpy(buffer, message.c_str());
            }
            else 
            {
                Message* currentMsg = new Message(tokens[1], tokens[2], message);
                msgs.push_back(currentMsg);
            }
        }
    }
    else if(tokens[0].compare("SENDMSG") == 0) {
        bool msg_sent = false;
        for(auto const& pair : servers) {
            if(pair.second->name.compare(tokens[1]) == 0) {
                string str = addTokens("SEND_MSG,P3_GROUP_6," + tokens[1] + "," + tokens[2]);
                send(sock, str.c_str(), str.length(), 0);
                msg_sent = true;
            }
        }
        if(!msg_sent) {
            Message* currentMsg = new Message("P3_GROUP_6", tokens[1], tokens[2]);
            msgs.push_back(currentMsg);
        }
    }
    else
    {
        cout << "Unknown command from client:" << buffer << endl;
    }
}

int main(int argc, char* argv[])
{
    bool finished;
    int serverSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    int connectionSock;             // Socket for new connections
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    struct sockaddr_in server;
    socklen_t serverLen;
    char buffer[1025];              // Buffer for reading from clients            

    if(argc != 3)
    {
        printf("Usage: ./tsamvgroup6 <server port i am listening to> <client port i am listening to>\n");
        exit(0);
    }
    cout << get_my_ip() << endl;

    // Setup sockets for server to listen on
    serverSock = open_socket(atoi(argv[1]));
    clientSock = open_socket(atoi(argv[2]));
    printf("Listening for servers on port %d and clients on port %d\n", atoi(argv[1]), atoi(argv[2]));

    if(listen(serverSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        close(serverSock);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(serverSock, &openSockets);
        maxfds = serverSock;
    }
    
    if(listen(clientSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        close(clientSock);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {
        FD_SET(clientSock, &openSockets);
        maxfds = max(maxfds, clientSock);
    }

    finished = false;

    while(!finished)
    {
        time_t t;
        time(&t);
        for(auto const& pair : servers) {
            Server* server = pair.second;

            if(t - server->keepalive_t > 240) {
                cout << "no KEEPALIVE sent for 4 minutes. Disconnecting server" << endl;
                closeClient(server->sock, &openSockets, &maxfds);
            }
        }
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new server connections to the server on the listening socket
            if(FD_ISSET(serverSock, &readSockets))
            {
                connectionSock = accept(serverSock, (struct sockaddr *)&server, &serverLen);
                // We need to know who connected to us so we immediately send LISTSERVERS
                string msg = "\x01LISTSERVERS,P3_GROUP_6\x04";
                if (send(connectionSock, msg.c_str(), msg.length(), 0) < 0)
                {
                    perror("send failed");
                }
                
                if (recv(connectionSock, buffer, sizeof(buffer), 0) < 0)
                {
                    perror("recv failed");
                }

                cout << buffer << endl;

                vector<string> tokens = parseTokens(buffer, ';');
                vector<string> newTokens = parseTokens(tokens[0], ',');
                string name = newTokens[1];
                string ip_str = newTokens[2];
                int server_port = stoi(tokens[3]);
                
                printf("accept***\n");
                // Add new server to the list of open sockets
                FD_SET(connectionSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = max(maxfds, connectionSock);

                // create a new server to store information.
                servers[connectionSock] = new Server(connectionSock, name, ip_str, server_port);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Server connected from %s:%d\n", ip_str.c_str(), server_port);
            }

            // Second, accept any new client connections to the server on the listening socket
            if(FD_ISSET(clientSock, &readSockets))
            {
                connectionSock = accept(clientSock, (struct sockaddr *)&client, &clientLen);
                // cout << "connectionSock before: " << connectionSock << endl;;
                
                struct in_addr client_addr = client.sin_addr;
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr, ip_str, INET_ADDRSTRLEN);
                int client_port = client.sin_port;
                
                // cout << "connectionSock after: " << connectionSock << endl;

                printf("accept***\n");
                // Add new client to the list of open sockets
                FD_SET(connectionSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = max(maxfds, connectionSock);

                // create a new client to store information.
                clients[connectionSock] = new Client(connectionSock, ip_str, client_port);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected from %s:%d\n", ip_str, client_port);
            }
            
            // Now check for commands from clients & servers
            while(n-- > 0)
            {
                for(auto const& pair : clients)
                {
                    Client *client = pair.second;
                    if(FD_ISSET(client->sock, &readSockets))
                    {
                        // recv() == 0 means client has closed connection
                        if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            printf("Client closed connection: %d\n", client->sock);
                            close(client->sock);
                            closeClient(client->sock, &openSockets, &maxfds);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            cout << buffer << endl;
                            clientCommand(client->sock, &openSockets, &maxfds, buffer, atoi(argv[1]));
                        }
                    }
                }

                for(auto const& pair : servers)
                {
                    Server *server = pair.second;
                    if(FD_ISSET(server->sock, &readSockets))
                    {
                        // recv() == 0 means server has closed connection
                        if(recv(server->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            printf("Server closed connection: %d\n", server->sock);
                            close(server->sock);
                            closeServer(server->sock, &openSockets, &maxfds);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            cout << buffer << endl;
                            clientCommand(server->sock, &openSockets, &maxfds, buffer, atoi(argv[1]));
                        }
                    }
                }
            }
        }
    }
}
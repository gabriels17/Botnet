all: server client
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o server -lpthread
client: client.cpp
	g++ -Wall -std=c++11 client.cpp -o client -lpthread
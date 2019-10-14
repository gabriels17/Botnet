all: server client
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o tsamvgroup6
client: client.cpp
	g++ -Wall -std=c++11 client.cpp -o client -lpthread
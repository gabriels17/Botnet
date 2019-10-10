all: myserver myclient server
myserver: myserver.cpp
	g++ -Wall -std=c++11 myserver.cpp -o tsamvgroup6
myclient: myclient.cpp
	g++ -Wall -std=c++11 myclient.cpp -o client -lpthread

# client: client.cpp
# 	g++ -Wall -std=c++11 client.cpp -o client -lpthread
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o server
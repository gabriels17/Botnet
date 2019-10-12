all: myserver myclient ip testserver
myserver: myserver.cpp
	g++ -Wall -std=c++11 myserver.cpp -o tsamvgroup6
testserver: server1.cpp
	g++ -Wall -std=c++11 server1.cpp -o testserver
myclient: myclient.cpp
	g++ -Wall -std=c++11 myclient.cpp -o client -lpthread
ip: ip.cpp
	g++ -Wall -std=c++11 ip.cpp -o ip
# client: client.cpp
# 	g++ -Wall -std=c++11 client.cpp -o client -lpthread
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o server
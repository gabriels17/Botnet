Our code was primarily compiled on a Windows machine in WSL the linux subsystem.

To compile:

    make

To run:

    ./tsamvgroup6 <server-port> <client-port>
    ./client <ipv4-of-server> <client-port-of-server>


Client Commands:

- CONNECT,<ip><port> : Connect to server at <ip> on port <port>
- WHO : Lists connections

Server Commands:

- LISTSERVERS,<from-group-name> : Requests a list of servers connected
- KEEPALIVE,<no-of-messages> : ???
- GETMSG : ???
- SEND_MSG,<from-name>,<to-name>,<message> : Sends a message
- LEAVE,<server-ip>,<server-port> : Disconnects from specified server

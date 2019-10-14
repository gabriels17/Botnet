Our code was primarily compiled on a Windows machine in WSL the linux subsystem.

To compile:

    make

To run:

    ./tsamvgroup6 <server-port> <client-port>
    ./client <ipv4-of-server> <client-port-of-server>


Client Commands:

- CONNECT,<name> : Connect to server as <name>
- WHO : Lists connections
- MSG,<group-name>,<message> : Send message to group
- MSG,ALL,<message> : Send message to all connected

Server Commands:

- LISTSERVERS,<from-group-name> : Requests a list of servers connected
- KEEPALIVE,<no-of-messages-waiting> : Messages a connected server, indicating still alive and the no. of messages waiting for the server to be "get-ed".
- GETMSG : Asks a group for the messages it has sent to us
- SEND_MSG,<from-name>,<to-name>,<message> : Sends a message
- LEAVE,<server-ip>,<server-port> : Disconnects from specified server

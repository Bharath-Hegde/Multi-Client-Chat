# Multi Client Chat Application

## Overview
The code contains two programs: server and client. The server program listens for incoming client connections and tranfsers data between the clients. The client program can connect to the server, request list of other clients, connect to any free client and exchange messages. Sockets are used for communication, pthreads for handling multiple clients simultaneously and file operations for storing chat history.

## Usage
Compile the server and client codes.

`gcc server.c -o server`

`gcc client.c -o client`

Run the executables with the desired port number and server IP address.

`./server <ip_address> <port>`

`./client <ip_address> <port>`

As a client use
* LIST to obtain a list of active clients from the server
* CONN <client_name> to send a connection request to the server
* EXIT to discontinue chatting

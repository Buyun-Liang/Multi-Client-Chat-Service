## Multi-client chat service
The main goal of this chat service is to achieve inter-process communication (between server and clients) through FIFOs. This project includes two parts:

Server
server starts the bl_server to manage the chatroom. The server is non-iterative.

Client
User who wants to chat could run the bl_client which takes input typed on the keyboard and sends it to the server. The server broadcasts the input to all other clients who can respond by typing their own input.

The key parts of this project includes:

1. Multiple communicating processes: clients to servers Communication through FIFOs
2. Signal handling for graceful server shutdown
3. Alarm signals for periodic behavior
4. Input multiplexing with poll()
5. Multiple threads in the client to handle typed input versus info from the server

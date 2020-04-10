#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <map>
#include "mensaje.pb.h"

using namespace std;
using namespace chat;

#ifndef MESSAGE_SIZE
#define MESSAGE_SIZE 8192
#endif

#ifndef Client
class Client {
    public:
        int _sock;
        struct sockaddr_in _serv_addr;
        int _user_id;
        char * _username;
        Client( char * username );
        int connect_server(char *server_address, int server_port);
        int log_in();
        int send_request(ClientMessage req);
        ServerMessage read_message();
        ClientMessage get_connected_request();
};
#endif


#ifndef client_info
struct client_info {
    int id;
    string name;
    string ip;
    int socket_fd;
    string status;
};
#endif

#ifndef Server
class Server {
    public:
        struct sockaddr_in _serv_addr;
        int _sock;
        int _user_count;
        int _port;
        map<std::string, client_info> _user_list;
        Server( int port );
        int initiate();
        void start();
        int listen_messages( char *buffer );
        int send_response( ServerMessage res, int fd );
        ServerMessage process_request( char *req, int fd );
        ServerMessage register_user( MyInfoSynchronize req, int fd );
        ServerMessage get_connected_users( connectedUserRequest req );
        ServerMessage error_response( string msg );
};
#endif
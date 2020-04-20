#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <map>
#include <queue> 
#include <iostream>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "mensaje.pb.h"

using namespace std;
using namespace chat;

#ifndef MESSAGE_SIZE
#define MESSAGE_SIZE 8192
#endif

#ifndef MAX_QUEUE
#define MAX_QUEUE 20
#endif

#ifndef gettid
#define gettid() syscall(SYS_gettid)
#endif

#ifndef connected_user
struct connected_user {
    int id;
    string name;
    string status;
    string ip;
};
#endif

#ifndef message_type
enum message_type {
    BROADCAST,
    DIRECT
};
#endif

#ifndef server_message_options
enum server_message_options {
    BROADCASTS = 1,
    MESSAGE = 2,
    ERROR = 3,
    MYINFORESPONSE = 4,
    CONNECTEDUSERRESPONSE = 5,
    CHANGESTATUSRESPONSE = 6,
    BROADCASTRESPONSE = 7,
    DIRECTMESSAGERESPONSE = 8
};
#endif


#ifndef client_message_options
enum client_message_options {
    SYNCHRONIZED = 1,
    CONNECTEDUSER = 2,
    CHANGESTATUS = 3,
    BROADCASTC = 4,
    DIRECTMESSAGE = 5,
    ACKNOWLEDGE = 6
};
#endif

#ifndef message_received
struct message_received {
    int from_id;
    string from_username;
    message_type type;
    string message;
};
#endif

#ifndef Client
class Client {
    public:
        int _sock;
        struct sockaddr_in _serv_addr;
        int _user_id;
        char * _username;
        Client( char * username, FILE *log_level = stdout );
        int connect_server(char *server_address, int server_port);
        int log_in();
        int send_request(ClientMessage req);
        int read_message( void * buff );
        ServerMessage parse_response( char *res );
        int get_connected_request();
        int change_status( string n_st );
        int broadcast_message( string msg );
        int direct_message( string msg, int dest_id = -1, string dest_nm = "" );
        int process_response( ServerMessage res );
        string get_last_error();
        void start_session();
        void stop_session();
        void handle_error( ErrorResponse err );
        static void * bg_listener( void * context );
    private:
        FILE *_log_level;
        pthread_mutex_t _noti_queue_mutex;
        queue <message_received> _dm_queue;
        queue <message_received> _br_queue;
        pthread_mutex_t _connected_users_mutex;
        map <string, connected_user> _connected_users;
        map <string, connected_user> get_connected_users();
        void set_connected_users( map <string, connected_user> cn_u );
        connected_user get_connected_user( string name );
        connected_user get_connected_user( int id );
        pthread_mutex_t _error_queue_mutex;
        queue <ErrorResponse> _error_queue;
        void add_error( ErrorResponse err );
        int pop_error_message( string * buf );
        int _close_issued;
        pthread_mutex_t _stop_mutex;
        int get_stopped_status();
        void send_stop();
        message_received pop_res( message_type mtype );
        int pop_to_buffer( message_type mtype, message_received * buf );
        void push_res( ServerMessage el );
        map <string, connected_user> parse_connected_users( ConnectedUserResponse c_usr );
};
#endif


#ifndef client_info
struct client_info {
    int id;
    struct sockaddr_in socket_info;
    int socket_fd;
    int req_fd;
    string name;
    string ip;
    string status;
};
#endif

#ifndef Server
class Server {
    public:
        struct sockaddr_in _serv_addr, _cl_addr;
        int _sock;
        int _user_count;
        int _port;
        Server( int port, FILE *log_level = stdout );
        int initiate();
        void start();
        int listen_connections();
        int read_request( int fd, void *buf );
        int send_response( int sock_fd, struct sockaddr_in *dest, ServerMessage res );
        ServerMessage process_request( ClientMessage cl_msg, client_info cl = {} );
        ServerMessage broadcast_message( BroadcastRequest req, client_info sender );
        ServerMessage send_to_all( string msg );
        ServerMessage direct_message( DirectMessageRequest req, client_info sender );
        ServerMessage error_response( string msg );
        string register_user( MyInfoSynchronize req, client_info cl );
        ServerMessage get_connected_users();
        ServerMessage change_user_status( ChangeStatusRequest req, string name );
        ClientMessage parse_request( char *req );
        void send_all( ServerMessage res, string sender );
        static void * new_conn_h( void * context );
    private:
        pthread_mutex_t _req_queue_mutex;
        queue <client_info> _req_queue;
        pthread_mutex_t _user_list_mutex;
        map<std::string, client_info> _user_list;
        FILE *_log_level;
        client_info req_pop();
        void req_push( client_info el );
        client_info get_user( string key );
        client_info get_user( int id );
        void add_user( client_info el );
        void delete_user( string key );
        map<std::string, client_info> get_all_users();
};
#endif

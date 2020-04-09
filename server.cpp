#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <map>
#include <sstream>
#include "mensaje.pb.h"

#define MESSAGE_SIZE 8192

using namespace chat;

struct client
{
    int id;
    std::string name;
    std::string ip;
    int socket_fd;
};


int sock, sock_opt = 1;
struct sockaddr_in serv_addr;
char buffer[MESSAGE_SIZE] = {0};
int user_count = 0;
std::map<std::string, client> user_list;

/*
* Start server to listen for messages on port
* returns 0 on success -1 on error
*/
int start_server(int port) {
    /* Create socket for server */
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) <= 0 ) {
        perror("Socket creation error!");
        return -1;
    }

    /* Attach server to port */
    if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sock_opt, sizeof(sock_opt)) < 0 ) {
        perror("Socket options failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_port = htons( port ); // Convert to host byte order

    if( bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) { // Bind server address to socket
        perror("Error binding address to socket");
        return -1;
    }

    /* Set socket to listen for connections */
    if( listen(sock, 10) < 0 ) { // queue 10 requests before refusing
        perror("Unable to listen for messages");
        return -1;
    }

    return 0;

}

/*
* Listen for messages and read its value to buffer
* returns 0 on succes -1 on error
*/
int listen_messages() {
    /* Get request info */
    int req_fd;
    int addr_size = sizeof(serv_addr);
    if( ( req_fd = accept(sock, (struct sockaddr *)&serv_addr, (socklen_t*)&addr_size) ) < 0 ) { // Get request file descriptor
        perror("Unable to accept request");
        return -1;
    }

    /* Read request info */
    if( read(req_fd, buffer, MESSAGE_SIZE)  < 0 ) {
        perror("Error reading request message");
    }

    return req_fd;
}

/*
* Send erroe message msg to socket fd
*/
void send_error_message(std::string msg, int fd) {

    ErrorResponse * err_msg(new ErrorResponse);
    err_msg->set_errormessage(msg);

    ServerMessage res;
    res.set_option(3);
    res.set_allocated_error(err_msg);

    std::string dsrl_res;
    res.SerializeToString(&dsrl_res);

    char c_str[ dsrl_res.size() + 1 ];
    strcpy( c_str, dsrl_res.c_str() );

    send(fd, c_str, sizeof(c_str), 0);
}

/*
* Saved user info and asigns user id
* return 0 on succes -1 on error
*/
int register_user(ClientMessage msg, int fd) {
    /* Step 1: Register user and assign user id */
    MyInfoSynchronize sync_info = msg.synchronize();

    std::stringstream key;
    key << sync_info.username() << "-" << sync_info.ip() << std::endl;

    // Check if user name is registered
    if( user_list.find( key.str() ) != user_list.end() ) {
        send_error_message("Username or ip already in use\n", fd);
        return -1;
    }
    
    struct client client_info;
    client_info.socket_fd = fd;
    client_info.id = user_count;
    client_info.name = sync_info.username();
    client_info.ip = sync_info.ip();

    user_list[ key.str() ] = client_info;
    user_count++;

    /* Step 2: Return userid to client */
    MyInfoResponse * myinfo_res(new MyInfoResponse);
    myinfo_res->set_userid(client_info.id);

    ServerMessage res;
    res.set_option(4);
    res.set_allocated_myinforesponse(myinfo_res);

    std::string dsrl_res;
    res.SerializeToString(&dsrl_res);

    char c_str[ dsrl_res.size() + 1 ];
    strcpy( c_str, dsrl_res.c_str() );

    if( send(fd, c_str, sizeof(c_str), 0) < 0 ) {
        perror("Unable to handshake with client");
        return -1;
    }

    /* Step 3: Wait for client ACK */
    char buffer_ack[MESSAGE_SIZE] = {0};
    if( read(fd, buffer_ack, MESSAGE_SIZE) < 0 ) {
        perror("ack response from client failed");
        return -1;
    }

    return 0;
}

/*
* Process an incoming client message
* returns 0 on succes -1 on error
*/
int process_message(int fd) {

    /* Deserealize message */
    ClientMessage cl_msg;
    cl_msg.ParseFromString(buffer);

    /* Process message depending on option */
    int res_val;
    switch (cl_msg.option()) {
        /* Register user */
        case 1:
            printf("Registering user\n");
            res_val = register_user(cl_msg, fd);        
        default:
            break;
    }
    return res_val;
}

int main(int argc, char *argv[]) {

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if( argc < 2) {
        perror("No port was passed!");
        return -1;
    }

    /* Start server */
    int port = atoi(argv[1]);

    if( start_server(port) < 0 ) {
        perror("Unable to start server");
        return -1;
    }

    printf("Server initiated on port %d\n", port);

    /* Listen for messages */
    printf("Listening for messages...\n");

    while(1){
        int req_fd;
        if( ( req_fd = listen_messages() ) < 0 ) {
            printf("Error reading messages");
        }
        
        /* Deserialize and process new message */
        if(  process_message(req_fd) < 0 ) {
            printf("Unable to process message!\n");
        }
    }
}
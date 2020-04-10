#include "Chat.h"

using namespace chat;

Server::Server( int port ) {
    _port = port;
}

/*
* Start server to listen for messages on port
* returns 0 on success -1 on error
*/
int Server::initiate() {
    /* Create socket for server */
    if( ( _sock = socket(AF_INET, SOCK_STREAM, 0) ) <= 0 ) {
        perror("Socket creation error!");
        return -1;
    }

    /* Attach server to port */
    int sock_opt = 1;
    if( setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sock_opt, sizeof(sock_opt)) < 0 ) {
        perror("Socket options failed");
        return -1;
    }

    _serv_addr.sin_family = AF_INET; 
    _serv_addr.sin_addr.s_addr = INADDR_ANY; 
    _serv_addr.sin_port = htons( _port ); // Convert to host byte order

    if( bind(_sock, (struct sockaddr *)&_serv_addr, sizeof(_serv_addr)) < 0 ) { // Bind server address to socket
        perror("Error binding address to socket");
        return -1;
    }

    /* Set socket to listen for connections */
    if( listen(_sock, 10) < 0 ) { // queue 10 requests before refusing
        perror("Unable to listen for messages");
        return -1;
    }

    return 0;
}

/*
* Listen for messages, read request's value to buffer and return request file descriptor
* returns 0 on succes -1 on error
*/
int Server::listen_messages( char *buffer ) {
    /* Get request info */
    int req_fd;
    int addr_size = sizeof(_serv_addr);
    if( ( req_fd = accept(_sock, (struct sockaddr *)&_serv_addr, (socklen_t*)&addr_size) ) < 0 ) { // Get request file descriptor
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
* Send response res to file desscriptor fd
* returns 0 on succes -1 on error
*/
int Server::send_response( ServerMessage res, int fd ) {
    printf("Serealizing response\n");
    string dsrl_res;
    res.SerializeToString(&dsrl_res);

    printf("Setting serialize response to c string\n");
    char c_str[ dsrl_res.size() + 1 ];
    strcpy( c_str, dsrl_res.c_str() );

    printf("Sending response to client\n");
    return send(fd, c_str, sizeof(c_str), 0);
}

/*
* Form error response with message msg
*/
ServerMessage Server::error_response( string msg ) {
    ErrorResponse * err_msg(new ErrorResponse);
    err_msg->set_errormessage(msg);

    ServerMessage res;
    res.set_option(3);
    res.set_allocated_error(err_msg);
    return res;
}

/*
* Saved user info and asigns user id
* return Server Response
*/
ServerMessage Server::register_user( MyInfoSynchronize req, int fd ) {
    /* Step 1: Register user and assign user id */

    // Check if user name or ip is registered
    printf("Checking if username is in used..\n");
    if( _user_list.find( req.username() ) != _user_list.end() ) {
        return error_response("Username already in use");
    } else {
        printf("Checking if ip adddress is already connected to server\n");
        map<std::string, client_info>::iterator it;
        for( it = _user_list.begin(); it != _user_list.end(); it++ ) {
            if( req.ip() == it->second.ip ) {
                return error_response("Ip already in use");
            }
        }
    }
    
    printf("Saving new user to connected users db\n");
    struct client_info new_client;
    new_client.socket_fd = fd;
    new_client.id = _user_count;
    new_client.name = req.username();
    new_client.ip = req.ip();

    _user_list[ req.username() ] = new_client;
    _user_count++;

    printf("User saved correctly onm db, building response...\n");
    /* Step 2: Return userid to client */
    MyInfoResponse * myinfo_res(new MyInfoResponse);
    myinfo_res->set_userid(new_client.id);

    ServerMessage res;
    res.set_option(4);
    res.set_allocated_myinforesponse(myinfo_res);

    printf("Response was build correctly\n");
    send_response( res, fd );

    /* Waiting for client ack */
    printf("Waiting for client ACK\n");
    char client_response[MESSAGE_SIZE];
    int req_fd = listen_messages( client_response );
    printf("Client ACK was received\n");

    return res;
}

/*
* Get all connected users, returns server response
*/
ServerMessage Server::get_connected_users( connectedUserRequest req ) {
    /* Verify if there are connected users */
    if( _user_list.empty() ) {
        return error_response("No connected users");
    }

    /* Get connected users */
    ConnectedUserResponse * users;
    map<std::string, client_info>::iterator it;
    for( it = _user_list.begin(); it != _user_list.end(); it++ ) {
        ConnectedUser * c_user(new ConnectedUser);
        c_user->set_username(it->first);
        c_user->set_status(it->second.status);
        c_user->set_userid(it->second.id);
        c_user->set_ip(it->second.ip);
        users->mutable_connectedusers()->AddAllocated(c_user);
    }

    /* Form response */
    ServerMessage res;
    res.set_allocated_connecteduserresponse(users);

    std::string dsrl_res;
    res.SerializeToString(&dsrl_res);

    return res;
}

/*
* Process incoming message and convert to appropiate Server response
*/
ServerMessage Server::process_request( char *req, int fd ) {
    printf("Parsing request\n");
    ClientMessage cl_msg;
    cl_msg.ParseFromString(req);
    int option = cl_msg.option();
    printf("Request was parsed with option: %d\n", option);
    switch (option)
    {
    case 1:
        printf("Registering user...\n");
        return register_user( cl_msg.synchronize(), fd );
    case 2:
        printf("Getting all connected users\n");
        return get_connected_users( cl_msg.connectedusers() );
    default:
        return error_response("Invalid option\n");
    }
}

/*
* Start to listen for messages
*/
void Server::start() {
    // Server infinite loop
    printf("Server initiated\n");
    while(1) {
        printf("listening for messages in port %d\n", _port);
        char request[MESSAGE_SIZE];
        int req_fd = listen_messages( request );
        printf("Request with fd: %d was received\n", req_fd);
        ServerMessage res = process_request( request, req_fd );
        int response_code = res.option();
        printf("Response option %d was returned, sending response back to client...\n", response_code);
        int send_code = send_response( res, req_fd );
        printf("Response sent with %d characters\n", send_code);
    }
}
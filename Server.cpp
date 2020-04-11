#include "Chat.h"

using namespace chat;

/*
* Start server instance
*/
Server::Server( int port, FILE *log_level ) {
    _port = port;
    _log_level = log_level;
}

/*
* Initiate server on port
* returns 0 on succes -1 on error
*/
int Server::initiate() {
    /* Create socket for server */
    fprintf(_log_level, "DEBUG: Creating new server socket\n");
    if( ( _sock = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        fprintf(_log_level, "ERROR: Socket creation error\n");
        return -1;
    }

    fprintf(_log_level, "DEBUG: Socket created correctly fd: %d\n", _sock);

    /* Bind server to port */
    fprintf(_log_level, "DEBUG: Binding socket %d to port %d\n", _sock, _port);
    _serv_addr.sin_family = AF_INET; 
    _serv_addr.sin_addr.s_addr = INADDR_ANY; 
    _serv_addr.sin_port = htons( _port ); // Convert to host byte order

    if( bind(_sock, (struct sockaddr *)&_serv_addr, sizeof(_serv_addr)) < 0 ) { // Bind server address to socket
        fprintf(_log_level, "ERROR: Error binding address to socket\n");
        return -1;
    }

    /* Set socket to listen for connections */
    fprintf(_log_level, "DEBUG: Setting socket to listen for connections...\n");
    if( listen( _sock, MAX_QUEUE ) < 0 ) { // queue MAX_QUEUE requests before refusing
        fprintf(_log_level, "ERROR: Unable to listen for messages\n");
        return -1;
    }

    return 0;
}

/*
* Listen for messages
* returns 0 on succes -1 on error
*/
int Server::listen_messages() {
    /* Accept incoming request */
    int addr_size = sizeof(_cl_addr);
    int req_fd = accept( _sock, (struct sockaddr *)&_cl_addr, (socklen_t*)&addr_size );
    if(req_fd < 0) {
        fprintf(_log_level, "ERROR: Error on connection with cliet\n");
        return -1;
    }

    fprintf( _log_level, "LOG: Accepted request with fd: %d\n", req_fd );

    /* Save request info to queue */
    struct client_info new_cl;
    new_cl.id = _user_count;
    new_cl.socket_info = _cl_addr;
    new_cl.req_fd = req_fd;
    _req_queue.push( new_cl );
    _user_count++;

    fprintf(_log_level, "DEBUG: Request added to queue id: %d\n", _user_count);

    return 0;
}

/*
* Read message on socket fd and writes it to buf
* Returns 0 on succes -1 on error 
*/
int Server::read_request( int fd, void *buf ) {
    /* Read request info on socket */
    fprintf(_log_level, "DEBUG: Receiving request to buffer \n");
    if( recvfrom( fd, buf, MESSAGE_SIZE, 0, NULL, NULL ) < 0 ) {
        fprintf(_log_level, "ERROR: Error reading request");
        return -1;
    }
    return 0;
}

/*
* Send response res on socket sock_fd to dest
* returns 0 on succes -1 on error
*/
int Server::send_response( int sock_fd, struct sockaddr_in *dest, ServerMessage res ) {
    /* Serealizing response */
    fprintf(_log_level, "DEBUG: Serealizing response\n");
    string dsrl_res;
    res.SerializeToString(&dsrl_res);

    // Converting to c string
    fprintf(_log_level, "DEBUG: Converting to c string\n");
    char c_str[ dsrl_res.size() + 1 ];
    strcpy( c_str, dsrl_res.c_str() );

    /* Sending response */
    fprintf(_log_level, "LOG: Sending response...\n");
    if( sendto( sock_fd, c_str, strlen(c_str), 0, (struct sockaddr *) &dest, sizeof( &dest ) ) < 0 ) {
        fprintf(_log_level, "ERROR: Error sending response\n");
        return -1;
    }
    return 0;
}

/*
* Process incoming message and convert to appropiate Server response
*/
ServerMessage Server::process_request( char *req, client_info cl ) {
    /* deserealizing request */
    fprintf(_log_level, "DEBUG: Deserealizing request\n");
    ClientMessage cl_msg;
    cl_msg.ParseFromString(req);
    int option = cl_msg.option();

    /* Process acording to option */
    fprintf(_log_level, "DEBUG: Processing request option: %d\n", option);
    switch (option)
    {
    case 1:
        return register_user( cl_msg.synchronize(), cl );
    /*case 2:
        return get_connected_users( cl_msg.connectedusers() );
    case 3:
        return change_user_status( cl_msg.changestatus() );
    case 4:
        return broadcast_message( cl_msg.broadcast() );
    case 5:
        return direct_message( cl_msg.directmessage() );*/
    default:
        return error_response("Invalid option\n");
    }
}

/*
* Register new user on connected users
* TODO: COMPLETE HANDSHAKE
*/
ServerMessage Server::register_user( MyInfoSynchronize req, client_info cl ) {
    fprintf(_log_level, "LOG: Registering new user\n");

    /* Step 1: Register user and assign user id */

    // Check if user name or ip is registered
    fprintf(_log_level, "DEBUG: Checking if username is in used..\n");
    if( _user_list.find( req.username() ) != _user_list.end() ) {
        return error_response("Username already in use");
    } else {
        fprintf(_log_level, "DEBUG: Checking if ip adddress is already connected to server\n");
        map<std::string, client_info>::iterator it;
        for( it = _user_list.begin(); it != _user_list.end(); it++ ) {
            if( req.ip() == it->second.ip ) {
                return error_response("Ip already in use");
            }
        }
    }

    // Adding mising data to client info
    cl.name = req.username();
    cl.ip = req.ip();
    // Adding to db
    fprintf(_log_level, "LOG: Save new user: %s\n", req.username().c_str());
    _user_list[ req.username() ] = cl;

    /* Step 2: Return userid to client */
    MyInfoResponse * myinfo_res(new MyInfoResponse);
    myinfo_res->set_userid( _user_list[ req.username() ].id );

    ServerMessage res;
    res.set_option(4);
    res.set_allocated_myinforesponse(myinfo_res);

    /*
    fprintf(_log_level, "DEBUG: Sending ACK to client..\n");
    send_response( _user_list[ req.username() ].req_fd, &_user_list[ req.username() ].socket_info, res );
    */
    /* Waiting for client ack */
    /*
    fprintf(_log_level, "DEBUG: Waiting for client ACK..\n");
    if( listen_messages() < 0 ) {
        return error_response("Error receiving client ACK\n");
    }
    */

    /* Reading client ACK */
    /*
    fprintf(_log_level, "DEBUG: Reading client ACK..\n");
    char ack[ MESSAGE_SIZE ];
    struct client_info req_ds = _req_queue.front();
    _req_queue.pop();
    read_request( req_ds.req_fd, ack );

    fprintf(_log_level, "DEBUG: Client ACK was process correctly.\n");
    */
    return res;
}

/*
* Form error response with message msg
*/
ServerMessage Server::error_response( string msg ) {
    /* Building response */
    fprintf(_log_level, "LOG: Bulding error response with message: %s\n", msg);
    ErrorResponse * err_msg(new ErrorResponse);
    err_msg->set_errormessage(msg);

    ServerMessage res;
    res.set_option(3);
    res.set_allocated_error(err_msg);
    return res;
}

/*
* Start the server infinite listening loop
*/
void Server::start() {
    /* Server infinite loop */
    pthread_t thread;
    fprintf(_log_level, "LOG: Listening for messages on port %d...\n", _port);
    while(1) {
        int client_id = listen_messages();
        fprintf(_log_level, "DEBUG: Reading request on new thread...\n");
        pthread_create( &thread, NULL, &*mt_handler, this );
    }
}

/*
* Function used on threads to handle requests
* TODO: ADD ERROR HANDLER
*/
void * Server::mt_handler( void * context ) {

    /* Get server context */
    Server * s = ( ( Server * )context );

    /* Gettin thread ID*/
    pid_t tid = gettid();
    fprintf( s->_log_level, "DEBUG: Running on thread ID: %d\n", ( int )tid);

    /* Start processing request */
    struct client_info req_ds = s->_req_queue.front();
    s->_req_queue.pop();
    char req[ MESSAGE_SIZE ];
    s->read_request( req_ds.req_fd, req );
    fprintf(stdout, "LOG: Processing request...\n");
    ServerMessage res = s->process_request( req, req_ds );
    s->send_response( req_ds.req_fd, &req_ds.socket_info, res );
    pthread_exit( NULL );
}

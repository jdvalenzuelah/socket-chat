#include "Chat.h"

using namespace chat;

/*
* Start server instance. Saves port where the server will be running and defines the log level
*/
Server::Server( int port, FILE *log_level ) {
    _port = port;
    _log_level = log_level;
    pthread_mutex_init( &_user_list_mutex, NULL );
    pthread_mutex_init( &_req_queue_mutex, NULL );
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
* Listen for new connections and adds them to the request queue to be processed
* returns 0 on succes -1 on error
*/
int Server::listen_connections() {
    /* Accept incoming request */
    int addr_size = sizeof(_cl_addr);
    int req_fd = accept( _sock, (struct sockaddr *)&_cl_addr, (socklen_t*)&addr_size );
    if(req_fd < 0) {
        fprintf(_log_level, "ERROR: Error on connection with cliet\n");
        return -1;
    }

    fprintf( _log_level, "INFO: Accepted request with fd: %d\n", req_fd );

    /* Get the incomming request ip */
    char ipstr[ INET6_ADDRSTRLEN ];
    struct sockaddr_in *tmp_s = (struct sockaddr_in *)&_cl_addr;
    inet_ntop( AF_INET, &tmp_s->sin_addr, ipstr, sizeof( ipstr ) );
    fprintf(_log_level, "INFO: Incomming request ip %s\n", ipstr);

    /* Save request info to queue */
    struct client_info new_cl;
    new_cl.id = _user_count;
    new_cl.socket_info = _cl_addr;
    new_cl.req_fd = req_fd;
    new_cl.ip = ipstr;
    req_push( new_cl );
    _user_count++;

    fprintf(_log_level, "DEBUG: Request added to queue id: %d\n", _user_count);

    return 0;
}

/*
* Read message on socket fd and writes it to buf. This is just a wrapper on recvfrom
* to make code easier to understand.
* Returns the number of bytes received, or -1 if an error occurred.
*/
int Server::read_request( int fd, void *buf ) {
    /* Read request info on socket */
    fprintf(_log_level, "DEBUG: Waiting for request of fd: %d \n", fd);
    int read_size;
    if( ( read_size = recvfrom( fd, buf, MESSAGE_SIZE, 0, NULL, NULL ) ) < 0 ) {
        fprintf(_log_level, "ERROR: Error reading request");
        return -1;
    }
    return read_size;
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
    fprintf(_log_level, "INFO: Sending response %d bytes to fd %d...\n", sizeof( c_str ), sock_fd);
    if( sendto( sock_fd, c_str, sizeof(c_str), 0, (struct sockaddr *) &dest, sizeof( &dest ) ) < 0 ) {
        fprintf(_log_level, "ERROR: Error sending response\n");
        return -1;
    }
    return 0;
}

/*
* Parse an incomming request, returns the ClientMessage that was received
*/
ClientMessage Server::parse_request( char *req ) {
    /* deserealizing request */
    fprintf(_log_level, "DEBUG: Deserealizing request\n");
    ClientMessage cl_msg;
    cl_msg.ParseFromString(req);
    return cl_msg;
}

/*
* Process incoming message and convert to appropiate Server response
*/
ServerMessage Server::process_request( ClientMessage cl_msg, client_info cl ) {
    /* Get option */
    int option = cl_msg.option();

    /* Process acording to option */
    fprintf(_log_level, "DEBUG: Processing request option: %d\n", option);
    switch (option) {
        case CONNECTEDUSER:
            return get_connected_users();
        case CHANGESTATUS:
            return change_user_status( cl_msg.changestatus(), cl.name );
        case BROADCASTC:
            return broadcast_message( cl_msg.broadcast(), cl );
        case DIRECTMESSAGE:
            return direct_message( cl_msg.directmessage(), cl );
        default:
            return error_response("Invalid option\n");
    }
}

/*
* Register new user on connected users
* returns username or empty string if unable to register user.
* Unlike more other functions this one handles the responses to server and process client responses.
*/
string Server::register_user( MyInfoSynchronize req, client_info cl ) {
    fprintf(_log_level, "INFO: Registering new user\n");
    map<std::string, client_info> all_users = get_all_users();
    /* Step 1: Register user and assign user id */

    // Check if user name or ip is registered
    fprintf(_log_level, "DEBUG: Checking if username is in used..\n");
    if( all_users.find( req.username() ) != all_users.end() ) {
        send_response( cl.req_fd, &cl.socket_info, error_response("Username already in use") );
        return "";
    } else {
        fprintf(_log_level, "DEBUG: Checking if ip adddress is already connected to server\n");
        map<std::string, client_info>::iterator it;
        for( it = all_users.begin(); it != all_users.end(); it++ ) {
            if( cl.ip == it->second.ip ) {
                send_response( cl.req_fd, &cl.socket_info, error_response("Ip already in use") );
                return "";
            }
        }
    }

    // Adding mising data to client info
    cl.name = req.username();

    // Adding to db
    fprintf(_log_level, "INFO: Save new user: %s conn fd: %d\n", req.username().c_str(), cl.req_fd);
    add_user( cl );

    /* Step 2: Return userid to client */
    client_info conn_user = get_user(  req.username() );
    MyInfoResponse * myinfo_res(new MyInfoResponse);
    myinfo_res->set_userid( conn_user.id );

    ServerMessage res;
    res.set_option( MYINFORESPONSE );
    res.set_allocated_myinforesponse(myinfo_res);

    
    fprintf(_log_level, "DEBUG: Sending ACK to client..\n");
    send_response( conn_user.req_fd, &conn_user.socket_info, res );

    /* Reading client ACK */
    
    fprintf(_log_level, "DEBUG: Reading client ACK..\n");
    char ack[ MESSAGE_SIZE ];
    read_request( conn_user.req_fd, ack );

    fprintf(_log_level, "DEBUG: Client ACK was process correctly.\n");
    
    return conn_user.name;
}

/*
* Get all the connected users on the server
* returns the server response with all the connected users
*/
ServerMessage Server::get_connected_users() {
    /* Verify if there are connected users */
    map<std::string, client_info> all_users = get_all_users();
    if( all_users.empty() ) {
        return error_response("No connected users");
    }

    /* Get connected users */

    map<std::string, client_info>::iterator it;
    fprintf(_log_level, "DEBUG: Mapping connected users to response\n");
    ConnectedUserResponse * users( new ConnectedUserResponse );
    
    for( it = all_users.begin(); it != all_users.end(); it++ ) {
        ConnectedUser * c_user(new ConnectedUser);
        c_user->set_username(it->first);
        ConnectedUser * c_user_l = users->add_connectedusers();
        c_user_l = c_user;
        fprintf(_log_level, "DEBUG: Saving connected user to response\n");
    }

    /* Form response */
    ServerMessage res;
    res.set_option( CONNECTEDUSERRESPONSE );
    fprintf(_log_level, "DEBUG: Saving connected users on response\n");
    res.set_allocated_connecteduserresponse( users );

    return res;
}

/*
* Change the user status. Won't notify other users
*/
ServerMessage Server::change_user_status( ChangeStatusRequest req, string name ) {
    string new_st = req.status();
    int id;
    pthread_mutex_lock( &_user_list_mutex );
    _user_list[ name ].status = new_st;
    id = _user_list[ name ].id;
    pthread_mutex_unlock( &_user_list_mutex );

    ChangeStatusResponse * ctr(new ChangeStatusResponse);
    ctr->set_userid( id );
    ctr->set_status( new_st );

    ServerMessage res;
    res.set_option( CHANGESTATUSRESPONSE );
    res.set_allocated_changestatusresponse( ctr );
    return res;
}

/*
* Send response to all connected users
*/
void Server::send_all( ServerMessage res, string sender ) {
    fprintf(_log_level, "INFO: Sending request to all connected clients\n");

    /* Iterate trough all connected users and send message */
    map<std::string, client_info> all_users = get_all_users();
    map<std::string, client_info>::iterator it;
    for( it = all_users.begin(); it != all_users.end(); it++ ) {
        if( it->first != sender ) {
            client_info user = it->second;
            send_response( user.req_fd, &user.socket_info, res );
        }
    }
}

/*
* Broadcast message to all users
*/
ServerMessage Server::broadcast_message( BroadcastRequest req, client_info sender ) {
    string msg = req.message();
    /* Notify all users of message */
    BroadcastMessage * br_msg( new BroadcastMessage );
    br_msg->set_message( msg );
    br_msg->set_userid( sender.id );
    ServerMessage all_res;
    all_res.set_option( BROADCASTS );
    all_res.set_allocated_broadcast( br_msg );
    send_all( all_res, sender.name );
    /* Return message status */
    BroadcastResponse * res( new BroadcastResponse );
    res->set_messagestatus( "sent" );
    ServerMessage res_r;
    res_r.set_option( BROADCASTRESPONSE );
    res_r.set_allocated_broadcastresponse( res );
    return res_r;
}

/*
* Send dm
*/
ServerMessage Server::direct_message( DirectMessageRequest req, client_info sender ) {
    /* Verify that username or id was sent */
    client_info rec;
    if( req.has_username() ) {
        rec = get_user( req.username() );
    } else if( req.has_userid() ) {
        rec = get_user( req.userid() );
    } else {
        return error_response( "You have to include either userid or username" );
    }

    /* Send dm to rec */
    DirectMessage * dm_msg( new DirectMessage );
    dm_msg->set_userid( sender.id );
    dm_msg->set_message( req.message() );

    ServerMessage dm_res;
    dm_res.set_option( MESSAGE );
    dm_res.set_allocated_message( dm_msg );

    send_response( rec.req_fd, &rec.socket_info, dm_res );

    /* Response to sender */
    DirectMessageResponse * res_msg( new DirectMessageResponse );
    res_msg->set_messagestatus( "sent" );

    ServerMessage res;
    res.set_option( DIRECTMESSAGERESPONSE );
    res.set_allocated_directmessageresponse( res_msg );

    return res;

}

/*
* Form error response with message msg
* returns server response with error details
*/
ServerMessage Server::error_response( string msg ) {
    /* Building response */
    fprintf(_log_level, "INFO: Bulding error response with message: %s\n", msg.c_str());
    ErrorResponse * err_msg(new ErrorResponse);
    err_msg->set_errormessage(msg);

    ServerMessage res;
    res.set_option( ERROR );
    res.set_allocated_error(err_msg);
    return res;
}

/*
* Start the server infinite listening loop
* Listens for new connections, creates a new thread for new every connection.
*/
void Server::start() {
    /* Server infinite loop */
    pthread_t thread;
    fprintf(_log_level, "INFO: Listening for new connections on port %d...\n", _port);
    while(1) {
        int client_id = listen_connections();
        fprintf(_log_level, "DEBUG: Processing connection on new thread...\n");
        pthread_create( &thread, NULL, &new_conn_h, this );
    }
}

/*
* Handle a new connection added to the queue on a new thread
* context is the server instance that is running
*/
void * Server::new_conn_h( void * context ) {
    /* Get server context */
    Server * s = ( ( Server * )context );

    /* Getting thread ID*/
    pid_t tid = gettid();
    fprintf( s->_log_level, "DEBUG: Staring connection process on thread ID: %d\n", ( int )tid);

    /* Start processing connection */
    struct client_info req_ds = s->req_pop();
    char req[ MESSAGE_SIZE ];
    s->read_request( req_ds.req_fd, req );
    
    /* New connection must be new user */
    ClientMessage in_req = s->parse_request( req );

    int in_opt = in_req.option();

    string usr_nm;

    if( in_opt == 1 ) {
        usr_nm = s->register_user( in_req.synchronize(), req_ds );
    } else {
        usr_nm = "";
        s->send_response( req_ds.req_fd, &req_ds.socket_info, s->error_response( "You must log in first\n" ) );
        fprintf( s->_log_level, "DEBUG: Unable to process new connection exiting thread ID: %d\n", ( int )tid);
        close( req_ds.req_fd );
        pthread_exit( NULL );
    }

    /* Server infinite loop */
    if( usr_nm != "" ){
        fprintf( s->_log_level, "INFO: Listenging for client '%s' messages on thread ID: %d\n", usr_nm.c_str(), ( int )tid);
        client_info user_ifo = s->get_user( usr_nm );
        char req[ MESSAGE_SIZE ];
        while( 1 ) {
            /* Read for new messages */
            int read_sz = s->read_request( req_ds.req_fd, req );

            /* Check if it has error or is empty */
            if( read_sz <= 0 ) {
                /* Error will disconnect user */
                fprintf(s->_log_level, "ERROR: Unable to read request\n");
                fprintf(s->_log_level,"INFO: Disconnecting user %s on fd %d\n", usr_nm.c_str(), req_ds.req_fd);
                s->delete_user( usr_nm );
                fprintf(s->_log_level, "DEBUG: Closing Client fd\n");
                close( req_ds.req_fd ); 
                break;
            }

            /* Valid incomming request */
            fprintf(s->_log_level, "INFO: Incomming request from user %s on fd %d...\n", usr_nm.c_str(), req_ds.req_fd);
            ServerMessage res = s->process_request( s->parse_request( req ), user_ifo );
            
            if( s->send_response( req_ds.req_fd, &req_ds.socket_info, res ) < -1 ) {
                fprintf( s->_log_level, "ERROR: Error sending response to fd %d\n", req_ds.req_fd );
            } else {
                fprintf(s->_log_level, "DEBUG: Sent re to fd %d\n", req_ds.req_fd);
            }

        }
    } else {
        close( req_ds.req_fd ); 
    }
    pthread_exit( NULL );
}

/*
* Pop request from queue using mutex
*/
client_info Server::req_pop() {
    client_info req;
    pthread_mutex_lock( &_req_queue_mutex );
    req = _req_queue.front();
    _req_queue.pop();
    pthread_mutex_unlock( &_req_queue_mutex );
    return req;
}

/*
* Add request to queue using mutex
*/
void Server::req_push( client_info el ) {
    pthread_mutex_lock( &_req_queue_mutex );
    _req_queue.push( el );
    pthread_mutex_unlock( &_req_queue_mutex );
}

/*
* Get a user from the connected list using mutex
*/
client_info Server::get_user( string key ) {
    client_info user;
    pthread_mutex_lock( &_user_list_mutex );
    user = _user_list[ key ];
    pthread_mutex_unlock( &_user_list_mutex );
    return user;
}

/*
* Add a user to the connected list using mutex;
*/
void Server::add_user( client_info el ) {
    pthread_mutex_lock( &_user_list_mutex );
    _user_list[ el.name ] = el;
    pthread_mutex_unlock( &_user_list_mutex );
}

/*
* Get a copy of the connected users
*/
map<std::string, client_info> Server::get_all_users() {
    map<std::string, client_info> tmp;
    pthread_mutex_lock( &_user_list_mutex );
    tmp = _user_list;
    pthread_mutex_unlock( &_user_list_mutex );
    return tmp;
}

/*
* Delete user with key
*/
void Server::delete_user( string key ) {
    pthread_mutex_lock( &_user_list_mutex );
    _user_list.erase( key );
    pthread_mutex_unlock( &_user_list_mutex );
}

/*
* Get a user by its id
*/
client_info Server::get_user( int id ) {
    map<std::string, client_info>::iterator it;
    client_info usr;
    pthread_mutex_lock( &_user_list_mutex );
    for( it = _user_list.begin(); it != _user_list.end(); it++ ) {
            if( it->second.id == id) {
                usr = it->second;
                break;
            }
        }
    pthread_mutex_unlock( &_user_list_mutex );
    return usr;
}

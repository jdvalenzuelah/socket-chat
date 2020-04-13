#include "Chat.h"

using namespace chat;

Client::Client( char * username, FILE *log_level ) {
    _user_id = -1;
    _sock = -1;
    _close_issued = 0;
    pthread_mutex_init(  &_res_queue_mutex, NULL );
    pthread_mutex_init( &_stop_mutex, NULL );
    _username = username;
    _log_level = log_level;
}

/*
* Connect to server on server_address:server_port
* returns 0 on succes -1 on error
*/
int Client::connect_server(char *server_address, int server_port) {
    /* Create socket */
    fprintf(_log_level,"INFO: Creating socket\n");
    if( ( _sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 ) {
        fprintf(_log_level,"ERROR: Error on socket creation");
        return -1;
    }

    /* Save server info on ds*/
    fprintf(_log_level, "DEBUG: Saving server info\n");
    _serv_addr.sin_family = AF_INET; 
    _serv_addr.sin_port = htons(server_port); // Convert to host byte order
    fprintf(_log_level,"Converting to network address");
    if(  inet_pton(AF_INET, server_address, &_serv_addr.sin_addr) <= 0 ) { // Convert ti network address
        fprintf(_log_level,"ERROR: Invalid server address\n");
        return -1;
    }

    /* Set up connection */
    fprintf(_log_level,"INFO: Setting up connection\n");
    if( connect(_sock, (struct sockaddr *)&_serv_addr, sizeof(_serv_addr)) < 0 ) {
        fprintf(_log_level,"ERROR: Unable to connect to server\n");
        return -1;
    }

    return 0;
}

/*
* Log in to server using username
* Return 0 on succes -1 on error
*/
int Client::log_in() {

    /* Step 1: Sync option 1 */
    fprintf(_log_level,"DEBUG: Building log in request\n");
    MyInfoSynchronize * my_info(new MyInfoSynchronize);
    char ip[256];
    gethostname(ip, sizeof(ip)); // Get client ip address
    my_info->set_username(_username);
    my_info->set_ip(ip);

    ClientMessage msg;
    msg.set_option(1);
    msg.set_allocated_synchronize(my_info);

    /* Sending sync request to server */
    send_request( msg );

    /* Step 2: Read ack from server */
    fprintf(_log_level,"DEBUG: Waiting for server ack\n");
    char ack_res[ MESSAGE_SIZE ];
    int rack_res_sz = read_message( ack_res );
    ServerMessage res = parse_response( ack_res );

    fprintf(_log_level,"INFO: Checking for response option\n");
    if( res.option() == 3 ) {
        fprintf(_log_level,"ERROR: Server returned error: %s\n", res.error().errormessage().c_str());
        return -1;
    } else if( res.option() != 4 ){
        fprintf(_log_level,"ERROR: Unexpected response from server\n");
        return -1;
    }

    fprintf(_log_level,"DEBUG: User id was returned by server\n");
    _user_id = res.myinforesponse().userid() ;

    /* Step 3: Send ack to server */
    // TODO MyInfoAcknowledge Not defined on protocol
    MyInfoAcknowledge * my_info_ack(new MyInfoAcknowledge);
    my_info_ack->set_userid(_user_id);

    ClientMessage res_ack;
    res_ack.set_option(10);

    send_request(res_ack);

    return 0;
}

/*
* Get all connected users to server
* returns client message with request details
*/
ClientMessage Client::get_connected_request() {
    /* Build request */
    fprintf(_log_level,"DEBUG: Building connected request\n");
    connectedUserRequest * msg(new connectedUserRequest);
    msg->set_userid( _user_id );
    msg->set_username( _username );

    ClientMessage res;
    res.set_option( 2 );
    res.set_allocated_connectedusers( msg );
    
    return res;
}

/*
* Build request to change the status to n_st
*/
ClientMessage Client::change_status( string n_st ) {
    ChangeStatusRequest * n_st_res( new ChangeStatusRequest );
    n_st_res->set_status( n_st );
    ClientMessage res;
    res.set_option( 3 );
    res.set_allocated_changestatus( n_st_res );
    return res;
}

/*
* Build a request to broadcast msg to all connected users on server
*/
ClientMessage Client::broadcast_message( string msg ) {
    BroadcastRequest * br_msg( new BroadcastRequest );
    br_msg->set_message( msg );
    ClientMessage res;
    res.set_option( 4 );
    res.set_allocated_broadcast( br_msg );
    return res;
}

/*
* Build request to send a direct message
*/
ClientMessage Client::direct_message( string msg, int dest_id, string dest_nm ) {
    DirectMessageRequest * dm( new DirectMessageRequest );
    dm->set_message( msg );
    /* Verify optional params were passed */
    if( dest_id > 0 ) {
        dm->set_userid( dest_id );
    }

    if( dest_nm != "" ) {
        dm->set_username( dest_nm );
    }

    ClientMessage res;
    res.set_option( 5 );
    res.set_allocated_directmessage( dm );
    return res;
}

/*
* Send request to server
* returns 0 on succes -1 on error
*/
int Client::send_request(ClientMessage request) {
    /* Serealize string */
    int res_code = request.option();
    fprintf(_log_level,"DEBUG: Serealizing request with option %d\n", res_code);
    std::string srl_req;
    request.SerializeToString(&srl_req);

    char c_str[ srl_req.size() + 1 ];
    strcpy( c_str, srl_req.c_str() );

    /* Send request to server */
    fprintf(_log_level,"INFO: Sending request\n");
    if( sendto( _sock, c_str, strlen(c_str ), 0, (struct sockaddr*)&_serv_addr,sizeof( &_serv_addr ) ) < 0 ) {
        fprintf(_log_level,"ERROR: Error sending request");
        return -1;
    }

    fprintf(_log_level,"DEBUG: Request was send to socket %d\n", _sock);
    return 0;
}

/*
* Read for messages from server
* Returns the read message
*/
int Client::read_message( void *res ) {
    /* Read for server response */
    int rec_sz;
    fprintf(_log_level, "DEBUG: Waiting for messages from server on fd %d\n", _sock);
    if( ( rec_sz = recvfrom(_sock, res, MESSAGE_SIZE, 0, NULL, NULL) ) < 0 ) {
        fprintf(_log_level,"ERROR: Error reading response");
        exit(EXIT_FAILURE);
    }
    fprintf(_log_level, "DEBUG: Received message from server\n");
    return rec_sz;
}

/*
* Parse the response to Server message
*/
ServerMessage Client::parse_response( char *res ) {
    ServerMessage response;
    response.ParseFromString(res);
    return response;
 }

 /*
 * Backgorund listener for server messages and adds them to response queue
 */
void * Client::bg_listener( void * context ) {
    /* Get Client context */
    Client * c = ( ( Client * )context );

    pid_t tid = gettid();
    fprintf(c->_log_level, "DEBUG: Staring client interface on thread ID: %d\n", ( int )tid);

    /* read for messages */
    while( c->get_stopped_status() == 0 ) {
        char ack_res[ MESSAGE_SIZE ];
        int ack_res_sz = c->read_message( ack_res );
        fprintf(c->_log_level,"DEBUG: New messages was received from server\n");
        ServerMessage res = c->parse_response( ack_res );
        fprintf(c->_log_level, "DEBUG: Adding response to queue\n" );
        c->push_res( res );
    }
    fprintf(c->_log_level, "INFO: Exiting listening thread\n");
    pthread_exit( NULL );
}

 /*
 * Start a new session on server on cli interface
 */
void Client::start_session() {
    /* Verify a connection with server was stablished */
    if ( _sock < 0 ){
        fprintf(_log_level, "ERROR: No connection to server was found\n" );
        exit( EXIT_FAILURE );
    }

    /* Verify if client was registered on server */
    if( _user_id < 0 ) { // No user has been registered
        /* Attempt to log in to server */
        if( log_in() < 0 ) {
            fprintf(_log_level, "ERROR: Unable to log in to server\n");
            exit( EXIT_FAILURE );
        } else {
            fprintf(_log_level, "INFO: Logged in to server user id %d\n", _user_id);
        }
    }
    /* Create a new thread to listen for messages from server */
    pthread_t thread;
    pthread_create( &thread, NULL, &bg_listener, this );

    /* Start client interface */
    while(1) {
        string k;
        cout << "Client loop \n";
        cin >> k;
        if( k == "2" )
            break;
        send_request( get_connected_request() );
    }
    stop_session();
}

/*
* Stop the server session
*/
void Client::stop_session() {
    fprintf(_log_level, "DEBUG: Starting shutting down process...\n");
    close( _sock ); // Stop send an write operations
    send_stop(); // Set stop flag
}

/*
* Get the stopped flag to verify if Client::stop_session has been called
*/
int Client::get_stopped_status() {
    int tmp = 0;
    pthread_mutex_lock( &_stop_mutex );
    tmp = _close_issued;
    pthread_mutex_unlock( &_stop_mutex );
    return tmp;
}

/*
* Send stop sets the stopped flag to start the shutdown process
*/
void Client::send_stop() {
    fprintf(_log_level, "DEBUG: Setting shutdown flag\n");
    pthread_mutex_lock( &_stop_mutex );
    _close_issued = 1;
    pthread_mutex_unlock( &_stop_mutex );
    fprintf(_log_level, "DEBUG: Shutdown flag set correctly\n");
}

/*
* Add respoonse to queue using mutext locks
*/
void Client::push_res( ServerMessage el ) {
    pthread_mutex_lock( &_res_queue_mutex );
    _res_queue.push( el );
    pthread_mutex_unlock( &_res_queue_mutex );
}

/*
* Get element from response queue
*/
ServerMessage Client::pop_res() {
    ServerMessage res;
    pthread_mutex_lock( &_res_queue_mutex );
    res = _res_queue.front();
    _res_queue.pop();
    pthread_mutex_unlock( &_res_queue_mutex );
    return res;
}
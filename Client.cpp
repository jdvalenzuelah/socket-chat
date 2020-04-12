#include "Chat.h"

using namespace chat;

Client::Client( char * username, FILE *log_level ) {
    _user_id = -1;
    _sock = -1;
    _username = username;
    _log_level = log_level;
}

/*
* Connect to server on server_address:server_port
* returns 0 on succes -1 on error
*/
int Client::connect_server(char *server_address, int server_port) {
    /* Create socket */
    printf("Creating socket\n");
    if( ( _sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 ) {
        perror("Error on socket creation");
        return -1;
    }

    /* Save server info on ds*/
    printf("Saving server info");
    _serv_addr.sin_family = AF_INET; 
    _serv_addr.sin_port = htons(server_port); // Convert to host byte order
    printf("Converting to network address");
    if(  inet_pton(AF_INET, server_address, &_serv_addr.sin_addr) <= 0 ) { // Convert ti network address
        perror("Invalid server address\n");
        return -1;
    }

    /* Set up connection */
    printf("Setting up connection\n");
    if( connect(_sock, (struct sockaddr *)&_serv_addr, sizeof(_serv_addr)) < 0 ) {
        perror("Unable to connect to server\n");
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
    printf("Building log in request");
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
    printf("Waiting for server ack\n");
    char ack_res[ MESSAGE_SIZE ];
    int rack_res_sz = read_message( ack_res );
    ServerMessage res = parse_response( ack_res );

    printf("Checking for response option\n");
    if( res.option() == 3 ) {
        printf("Server returned error: %s", res.error().errormessage().c_str());
        return -1;
    } else if( res.option() != 4 ){
        perror("Unexpected response from server");
        return -1;
    }

    printf("User id was returned by server\n");
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
    printf("Building connected request");
    connectedUserRequest * msg(new connectedUserRequest);
    msg->set_userid(_user_id);
    msg->set_username(_username);

    ClientMessage req;
    req.set_option(2);
    req.set_allocated_connectedusers(msg);
    
    return req;
}

/*
* Send request to server
* returns 0 on succes -1 on error
*/
int Client::send_request(ClientMessage request) {
    /* Serealize string */
    int res_code = request.option();
    printf("Serealizing request with option %d\n", res_code);
    std::string srl_req;
    request.SerializeToString(&srl_req);

    char c_str[ srl_req.size() + 1 ];
    strcpy( c_str, srl_req.c_str() );

    /* Send request to server */
    printf("Sending request\n");
    if( sendto( _sock, c_str, strlen(c_str ), 0, (struct sockaddr*)&_serv_addr,sizeof( &_serv_addr ) ) < 0 ) {
        perror("Error sending request");
        return -1;
    }

    printf("Request was send to socket %d\n", _sock);
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
        perror("Error reading response");
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
 * Backgorund listener for server messages
 */
void * Client::bg_listener( void * context ) {
    /* Get Client context */
    Client * c = ( ( Client * )context );

    pid_t tid = gettid();
    fprintf( c->_log_level, "DEBUG: Staring client interface on thread ID: %d\n", ( int )tid);

    /* read for messages */
    while(1) {
        string k;
        cout << "Client loop \n";
        cin >> k;
        c->send_request( c->get_connected_request() );
    }
    pthread_exit( NULL );
}

 /*
 * Start a new session on server on cli interface
 */
void Client::start_session() {
    /* Verify a connection with server was stablished */
    if ( _sock < 0 ){
        fprintf( _log_level, "No connection to server was found\n" );
        exit( EXIT_FAILURE );
    }

    /* Verify if client was registered on server */
    if( _user_id < 0 ) { // No user has been registered
        /* Attempt to log in to server */
        if( log_in() < 0 ) {
            fprintf(_log_level, "ERROR: Unable to log in to server\n");
            exit( EXIT_FAILURE );
        } else {
            fprintf(_log_level, "LOG: Logged in to server user id %d\n", _user_id);
        }
    }
    /* Create a new thread to listen for messages from server */
    pthread_t thread;
    //pthread_create( &thread, NULL, &bg_listener, this );

    /* Start client listener */
    while(1) {
        string k;
        cout << "Client loop \n";
        cin >> k;
        send_request( get_connected_request() );

        char ack_res[ MESSAGE_SIZE ];
        int ack_res_sz = read_message( ack_res );
        printf("New messages was received from server\n");
        ServerMessage res = parse_response( ack_res );
    }
}
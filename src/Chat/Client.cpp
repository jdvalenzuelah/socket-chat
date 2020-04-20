#include "Chat.h"

using namespace chat;

Client::Client( char * username, FILE *log_level ) {
    _user_id = -1;
    _sock = -1;
    _close_issued = 0;
    pthread_mutex_init(  &_noti_queue_mutex, NULL );
    pthread_mutex_init( &_stop_mutex, NULL );
    pthread_mutex_init( &_connected_users_mutex, NULL );
    pthread_mutex_init( &_error_queue_mutex, NULL );
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
    my_info->set_username(_username);

    ClientMessage msg;
    msg.set_option( SYNCHRONIZED );
    msg.set_allocated_synchronize(my_info);

    /* Sending sync request to server */
    send_request( msg );

    /* Step 2: Read ack from server */
    fprintf(_log_level,"DEBUG: Waiting for server ack\n");
    char ack_res[ MESSAGE_SIZE ];
    int rack_res_sz = read_message( ack_res );
    ServerMessage res = parse_response( ack_res );

    fprintf(_log_level,"INFO: Checking for response option\n");
    if( res.option() == ERROR ) {
        fprintf(_log_level,"ERROR: Server returned error: %s\n", res.error().errormessage().c_str());
        return -1;
    } else if( res.option() != MYINFORESPONSE ){
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
    res_ack.set_option( ACKNOWLEDGE );

    send_request(res_ack);

    return 0;
}

/*
* Get all connected users to server
* returns client message with request details
*/
int Client::get_connected_request() {
    /* Build request */
    fprintf(_log_level,"DEBUG: Building connected request\n");
    connectedUserRequest * msg(new connectedUserRequest);

    ClientMessage req;
    req.set_option( CONNECTEDUSER );
    req.set_allocated_connectedusers( msg );
    
    if( send_request( req ) < 0 ) {
        fprintf( _log_level, "ERROR: Unable to send request\n" );
    }

    return 0;
}

/*
* Parse the connected users on a connected_clients map
*/
map <string, connected_user> Client::parse_connected_users( ConnectedUserResponse c_usr ) {
    /* Save connected users */
    map <string, connected_user> c_users;
    int i;
    for( i = 0; i < c_usr.connectedusers().size(); i++ ) {
        ConnectedUser rec_user = c_usr.connectedusers().at( i );
        struct connected_user lst_users;
        lst_users.name = rec_user.username();
        lst_users.id = rec_user.userid();
        lst_users.status = rec_user.status();
        lst_users.ip = rec_user.status();
        c_users[ rec_user.username() ] = lst_users;
    }
    set_connected_users( c_users );
    return c_users;
}

/*
* Build request to change the status to n_st
*/
int Client::change_status( string n_st ) {
    ChangeStatusRequest * n_st_res( new ChangeStatusRequest );
    n_st_res->set_status( n_st );
    ClientMessage req;
    req.set_option( CHANGESTATUS );
    req.set_allocated_changestatus( n_st_res );
    
    if( send_request( req ) < 0 ) {
        fprintf( _log_level, "ERROR: Unable to send request\n" );
    }

    return 0;

}

/*
* Build a request to broadcast msg to all connected users on server
*/
int Client::broadcast_message( string msg ) {
    BroadcastRequest * br_msg( new BroadcastRequest );
    br_msg->set_message( msg );
    ClientMessage req;
    req.set_option( BROADCASTC );
    req.set_allocated_broadcast( br_msg );

    if( send_request( req ) < 0 ) {
        fprintf( _log_level, "ERROR: Unable to send request\n" );
    }

    return 0;

}

/*
* Build request to send a direct message
*/
int Client::direct_message( string msg, int dest_id, string dest_nm ) {
    DirectMessageRequest * dm( new DirectMessageRequest );
    dm->set_message( msg );
    /* Verify optional params were passed */
    if( dest_id > 0 ) {
        dm->set_userid( dest_id );
    }

    if( dest_nm != "" ) {
        dm->set_username( dest_nm );
    }

    ClientMessage req;
    req.set_option( DIRECTMESSAGE );
    req.set_allocated_directmessage( dm );
    

    if( send_request( req ) < 0 ) {
        fprintf( _log_level, "ERROR: Unable to send request\n" );
    }

    return 0;
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
* Send request to server to change status
*/


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
        if( c->read_message( ack_res ) <= 0 ) {
            fprintf( c->_log_level, "LOG: Server disconnected terminating session..." );
            c->send_stop();
            break;
        }
        fprintf(c->_log_level,"DEBUG: New messages was received from server\n");
        ServerMessage res = c->parse_response( ack_res );
        fprintf(c->_log_level, "DEBUG: Adding response to queue\n" );
        
        /* Process acoorging to response, we ignore status responses */
        switch ( res.option() ) {
            case BROADCASTS:
            case MESSAGE:
                c->push_res( res ); 
                break;
            case CONNECTEDUSERRESPONSE:
                c->parse_connected_users( res.connecteduserresponse() );
                break;
            case ERROR:
                c->handle_error( res.error() );
                break;
            default:
                break;
        }
        
        c->push_res( res );
    }
    fprintf(c->_log_level, "INFO: Exiting listening thread\n");
    pthread_exit( NULL );
}

/*
* Handle error responses from server
*/
void Client::handle_error( ErrorResponse err ) {
    add_error(err);
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
*  Get all the connected users on memory
*/
map <string, connected_user> Client::get_connected_users() {
    map <string, connected_user> tmp;
    pthread_mutex_lock( &_connected_users_mutex );
    tmp = _connected_users;
    pthread_mutex_unlock( &_connected_users_mutex );
    return tmp;
}

/*
* Refresh the list of connected users
*/
void Client::set_connected_users( map <string, connected_user> cn_u ) {
    pthread_mutex_lock( &_connected_users_mutex );
    _connected_users = cn_u;
    pthread_mutex_unlock( &_connected_users_mutex );
}

/*
* Add new error
*/
void Client::add_error( ErrorResponse err ) {
    pthread_mutex_lock( &_error_queue_mutex );
    _error_queue.push( err );
    pthread_mutex_unlock( &_error_queue_mutex );
}

/*
* Get the last error message on queue
*/
int Client::pop_error_message( string * buf ) {
    int res = -1;
    pthread_mutex_lock( &_error_queue_mutex );
    if( !_error_queue.empty() ) {
        *buf = _error_queue.front().errormessage();
        _error_queue.pop();
        res = 0;
    }
    pthread_mutex_unlock( &_error_queue_mutex );
    return res;
}

/*
* Get the latest error
*/
string Client::get_last_error() {
    string error_msg;
    pthread_mutex_lock( &_error_queue_mutex );
    error_msg = _error_queue.front().errormessage();
    _error_queue.pop();
    pthread_mutex_unlock( &_error_queue_mutex );
    return error_msg;
}

/*
* Add respoonse to queue using mutext locks
*/
void Client::push_res( ServerMessage el ) {
    int option = el.option();
    fprintf(_log_level, "LOG: Adding new message to queue option %d\n", option);
    message_received msg;
    pthread_mutex_lock( &_noti_queue_mutex );
    if( option == 1 ) {
        msg.from_id = el.broadcast().userid();
        msg.message = el.broadcast().message();
        msg.type = BROADCAST;
        _br_queue.push( msg );
    } else if( option == 2 ) {
        msg.from_username = el.message().userid();
        msg.message = el.message().message();
        msg.type = DIRECT;
        _dm_queue.push( msg );
    }
    pthread_mutex_unlock( &_noti_queue_mutex );
}

/*
* Get element from response queue
*/
message_received Client::pop_res( message_type mtype ) {
    message_received res;
    pthread_mutex_lock( &_noti_queue_mutex );
    if( mtype == BROADCAST ) {
        res = _br_queue.front();
        _br_queue.pop();
    } else if( mtype == DIRECT ) {
        res = _dm_queue.front();
        _dm_queue.pop();
    }
    pthread_mutex_unlock( &_noti_queue_mutex );
    return res;
}

/*
* Get element to buffer. Returns 0 on succes -1 if empty
*/
int Client::pop_to_buffer( message_type mtype, message_received * buf ) {
    int res;
    fprintf(_log_level, "DEBUG: Locking noti queue mutex\n");
    pthread_mutex_lock( &_noti_queue_mutex );
    if( mtype == BROADCAST ) {
        fprintf(_log_level, "DEBUG: Getting broadcast messages\n");
        if( !_br_queue.empty() ) {
            fprintf(_log_level, "DEBUG: Not empty, popping from queue\n");
            *buf = _br_queue.front();
            _br_queue.pop();
            res = 0;
        } else {
            fprintf(_log_level, "DEBUG: Empty nothing to show\n");
            res = -1;
        }
    } else if( mtype == DIRECT ) {
        fprintf(_log_level, "DEBUG: Getting direct messages\n");
        if( !_dm_queue.empty() ) {
            fprintf(_log_level, "DEBUG: Not empty, popping from queue\n");
            *buf = _dm_queue.front();
            _dm_queue.pop();
            res = 0;
        } else {
            fprintf(_log_level, "DEBUG: Empty nothing to show\n");
            res = -1;
        }
    }
    fprintf(_log_level, "DEBUG: Unlocking noti queue mutex\n");
    pthread_mutex_unlock( &_noti_queue_mutex );
    return res;
}

/*
* Get a connected users info by its username
*/
connected_user Client::get_connected_user( string name ) {
    connected_user tmp;
    pthread_mutex_lock( &_connected_users_mutex );
    tmp = _connected_users[ name ];
    pthread_mutex_unlock( &_connected_users_mutex );
    return tmp;
}

/*
* Get a connected users info by its id
*/
connected_user Client::get_connected_user( int id ) {
    connected_user tmp;
    map<string, connected_user>::iterator it;
    pthread_mutex_lock( &_connected_users_mutex );
    for( it = _connected_users.begin(); it != _connected_users.end(); it++ ){
        if( it->second.id == id )
            tmp = it->second;
    }
    pthread_mutex_unlock( &_connected_users_mutex );
    return tmp;
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
    int no = 0;
    message_type msg_t;
    while(1) {

        if( no ) {
            string err;

            if( pop_error_message( &err ) == 0 ) {
                cout << "Error: " << err << endl;
            } else if( no == 1 ) {
                message_received mtp;
                while( pop_to_buffer( msg_t, &mtp ) == 0 ) {
                    cout << "--------- Mensages ---------" << endl;
                    cout << "ID from: " << mtp.from_id << endl;
                    cout << "User name from: " << mtp.from_username << endl;
                    cout << "Message: " << mtp.message << endl;
                    cout << "----------------------------" << endl;
                }
            } else if( no == 2 ) {
                map <string, connected_user> tmp = get_connected_users();
                map<string, connected_user>::iterator it;
                cout << "--------- Usuarios conetados ---------" << endl;
                for( it = tmp.begin(); it != tmp.end(); it++ ) {
                    cout << "User id: " << it->second.id << endl;
                    cout << "User name: " << it->first << endl;
                    cout << "User status: " << it->second.status << endl;
                    cout << "--------------------------------------" << endl;
                }
            }
        }
        
        int input;
        printf("\n\n------ Bienvenido al chat %s ------\n", _username);
        printf("Seleccione una opcion:\n");
        printf("\t1. Enviar mensaje al canal publico\n");
        printf("\t2. Enviar mensaje directo \n");
        printf("\t3. Cambiar de estado \n");
        printf("\t4. Ver usuarios conectados \n");
        printf("\t5. Ver informacion de usuario \n");
        printf("\t6. Ver canal general \n");
        printf("\t7. Ver mensajes directos \n");
        printf("\t8. Salir \n");
        cin >> input;
        string t;
        getline(cin, t);
        
        string br_msg = "";
        string dm = "";
        string dest_nm = "";
        int res_cd;
        no = 0;
        int st;
        string n_sts = "activo";
        int mm_ui, usr_id;
        string usr;
        connected_user c_usr;
        switch ( input ) {
            case 1:
                cout << "Ingrese mensaje a enviar:\n";
                getline( cin, br_msg );
                break;
            case 2:
                printf("Ingrese nombre de usuario del destinatario:\n");
                getline( cin, dest_nm );
                printf("Ingrese el mensaje a enviar: \n");
                getline( cin, dm );
                res_cd = direct_message( br_msg, -1, dest_nm );
                break;
            case 3:
                printf("Seleccione un estado: \n");
                printf("1. Activo\n");
                printf("2. Inctivo\n");
                printf("3. Ocupado\n");
                cin >> st;
                getline(cin, t);
                if(st == 1)
                    n_sts = "activo";
                else if( st == 2 )
                    n_sts = "inactivo";
                else if( st == 3 )
                    n_sts = "ocupado";
                res_cd = change_status( n_sts );
                break;
            case 4:
                res_cd = get_connected_request();
                no = 2;
                break;
            case 5:
                printf("Buscar usuario por:\n");
                printf("1. ID de usuario\n");
                printf("2. Nombre de usuario\n");
                cin >> mm_ui;
                getline(cin, t);
                switch ( mm_ui ){
                    case 1:
                        printf("Ingresa el id de usuario:\n");
                        cin >> usr_id;
                        getline(cin, t);
                        c_usr = get_connected_user( usr_id );
                        break;
                    case 2:
                        printf("Ingresa el nombre de usuario:\n");
                        getline(cin, usr);
                        c_usr = get_connected_user( usr );
                        break;
                    default:
                        printf("Opcion invalida!");
                        break;
                }
                cout << "--------------------------------------" << endl;
                cout << "User id: " << c_usr.id << endl;
                cout << "User name: " << c_usr.name << endl;
                cout << "User status: " << c_usr.status << endl;
                cout << "User IP: " << c_usr.ip << endl;
                cout << "--------------------------------------" << endl;
                break;
            case 6:
                msg_t = BROADCAST;
                no = 1;
                break;
            case 7:
                msg_t = DIRECT;
                no = 1;
                break;
            default:
                break;
        }
        sleep( 1 );
        if( input == 8 )
            break;
    }
    stop_session();
}
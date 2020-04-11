#include "Chat.h"

using namespace chat;

Client::Client( char * username ) {
    _username = username;
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
    ServerMessage res = read_message();

    printf("Checking for response option\n");
    if( res.option() == 3 ) {
        std::cout << "Server returned error: " << res.error().errormessage() << std::endl;
        return -1;
    } else if( res.option() != 4 ){
        perror("Unexpected response from server");
        return -1;
    }

    printf("User id was returned by server\n");
    _user_id = res.myinforesponse().userid() ;

    /* Step 3: Send ack to server */
    // TODO MyInfoAcknowledge Not defined on protocol
    /*MyInfoAcknowledge * my_info_ack(new MyInfoAcknowledge);
    my_info_ack->set_userid(_user_id);

    ClientMessage res_ack;
    res_ack.set_option(10);

    send_request(res_ack);*/

    return 0;
}

/*
* Get all connected users to server
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
    if( send( _sock, c_str, sizeof(c_str), 0 ) < 0 ) {
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
ServerMessage Client::read_message() {

    /* Read for server response */
    char response[MESSAGE_SIZE] = {0};
    if( recvfrom(_sock, response, MESSAGE_SIZE, 0, NULL, NULL) < 0 ) {
        perror("Error reading response");
        exit(EXIT_FAILURE);
    }

    ServerMessage res;
    res.ParseFromString(response);

    if( res.option() == 3 ) {
        std::cout << "Server returned error: " << res.error().errormessage() << std::endl;
        exit(EXIT_FAILURE);
    }

    return res;
}
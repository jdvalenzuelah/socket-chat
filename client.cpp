#include <stdio.h>
#include<stdlib.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h>
#include <string.h>
#include "mensaje.pb.h"

#define MESSAGE_SIZE 8192

using namespace chat;

int sock;
struct sockaddr_in serv_addr;
int user_id;

/*
* Connect to server on server_address:server_port
* returns 0 on succes -1 on error
*/
int connect_server(char *server_address, int server_port) {
    /* Create socket */
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 ) {
        perror("Error on socket creation");
        return -1;
    }

    /* Save server info on ds*/
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(server_port); // Convert to host byte order
    if(  inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0 ) { // Convert ti network address
        perror("Invalid server address\n");
        return -1;
    }

    /* Set up connection */
    if( connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
        perror("Unable to connect to server\n");
        return -1;
    }

    return 0;
}

/*
* Log in to server using username
*/
int log_in( std::string username ) {

    /* Step 1: Sync option 1 */
    MyInfoSynchronize * my_info(new MyInfoSynchronize);
    char ip[256];
    gethostname(ip, sizeof(ip)); // Get client ip address
    my_info->set_username(username);
    my_info->set_ip(ip);

    ClientMessage msg;
    msg.set_option(1);
    msg.set_allocated_synchronize(my_info);
    std::string srl_msg;
    msg.SerializeToString(&srl_msg);

    char c_str[ srl_msg.size() - 1 ];
    strcpy( c_str, srl_msg.c_str() );
    if( send( sock, c_str, sizeof(c_str), 0 ) < 0) { // Send sync as cstr
        perror("Error sending login message to server");
        return -1;
    }

    /* Step 2: Read ack from server */
    char response[MESSAGE_SIZE] = {0};
    if( read(sock, response, MESSAGE_SIZE) < 0 ) {
        perror("Error on server ack");
        return -1;
    }

    ServerMessage res;
    res.ParseFromString(response);

    if( res.option() == 3 ) {
        std::cout << "Server returned error: " << res.error().errormessage() << std::endl;
        return -1;
    } else if( res.option() != 4 ){
        perror("Unexpected response from server");
        return -1;
    }

    user_id = res.myinforesponse().userid() ;

    /* Step 3: Send ack to server */
    // TODO MyInfoAcknowledge Not defined on protocol
    MyInfoAcknowledge * my_info_ack(new MyInfoAcknowledge);
    my_info_ack->set_userid(user_id);

    ClientMessage res_ack;
    res_ack.set_option(0);

    std::string srl_res_ack;
    res_ack.SerializeToString(&srl_res_ack);

    char c_str_ack[ srl_res_ack.size() - 1 ];
    strcpy( c_str_ack, srl_res_ack.c_str() );

    if( send( sock, c_str_ack, sizeof(c_str_ack), 0 ) < 0) { // Send ack as cstr
        perror("Error sending ack message to server");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    if(argc < 4) {
        printf("Not enough params!\n");
        return 1;
    }

    int port = atoi(argv[3]);
    char *address = argv[2];
    char *username = argv[1];

    if( connect_server(address, port) < 0 ) {
        printf("Connection error!\n");
        return -1;
    } else {
        printf("Connected to server at %s:%d\n", address, port);
    }

    log_in(username);

    printf("User id: %d", user_id);
    return 0;

}
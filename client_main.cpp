#include <stdio.h>
#include "Chat.h"

int main(int argc, char *argv[]) {

    if(argc < 4) {
        printf("Not enough params!\n");
        return 1;
    }

    /* Create client with username */
    Client client(argv[1]);

    /* Connect to server on address:port*/
    int port = atoi(argv[3]);
    char *address = argv[2];

    if( client.connect_server(address, port) < 0 ) {
        printf("Connection error!\n");
        return -1;
    }

    printf("Connected to server at %s:%d\n", address, port);

    /* Log in to server */
    /*printf("Logging in to server username: %s...\n", argv[1]);
    if( client.log_in() < 0 ) {
        printf("Unable to log in\n");
        return -1;
    }*/

    client.start_session();
    return 0;

}
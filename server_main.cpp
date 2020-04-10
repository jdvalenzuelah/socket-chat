#include <stdio.h>
#include "Chat.h"

int main(int argc, char *argv[]) {

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if( argc < 2) {
        perror("No port was passed!");
        return -1;
    }

    int port = atoi(argv[1]);
    Server server(port);

    if( server.initiate() < 0 ) {
        perror("Unable to initiate server");
        return -1;
    }

    server.start();

}
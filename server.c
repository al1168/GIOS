#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
1. create socket
2. define a address  
    struct sockaddr /* generic address struct
    struct sockaddr_in /* "in" = intenret, address for ipv4
    2a. set fields in adddress
            sin: shorthand socketaddr_in 
        sin_family: AF_INET
        sin_port: htons(8888) // host to network short
        sin_addr.s_addr : INADDR_ANY // This is actually setting the address, here we are setting to  "any" available IP address on a host.
*/
int main(void){
    int serverSocket = socket(AF_LOCAL, SOCK_STREAM, 0);
    struct sockaddr_in serverAddress; 
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(9052);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    bind(serverSocket, (struct sockaddr *) serverAddress, sizeof(serverAddress));
    listen
    return 0;
}

#include <netinet/in.h> //structure for storing address information 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h> //for socket APIs 
#include <sys/types.h> 

int main(int argc, char const* argv[]) 
{ 

    int clientfd = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in address = {0};
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;
    // address.sin_port = 

}

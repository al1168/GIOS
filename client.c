#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/*
Create Server procedure 
1. define socket
    
2. bind socket
3. listen socket
4. accept socket
*/
int main(void){
    int clientSocket = socket(AF_LOCAL, SOCK_STREAM,0); // 0 is default
    struct sockaddr_in server_address; 
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9052);
    server_address.sin_addr.s_addr = INADDR_ANY;
    int connectSuccess = connect(clientSocket, (struct sockaddr *) &server_address, sizeof(server_address));
    if (connectSuccess == -1){
        printf("Something is wrong with connection");
    }
    char serverResponse[256];
    recv(clientSocket,serverResponse, sizeof(serverResponse),0); 
    close(clientSocket);
    return 0;
}
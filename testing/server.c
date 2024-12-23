

#include <netinet/in.h> //structure for storing address information 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h> //for socket APIs 
#include <sys/types.h> 
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#define PORT 8080
int main(int argc, char const* argv[]) { 
  int serverfd = socket(AF_INET,SOCK_STREAM,0);
  if (serverfd == -1){
    perror("socket creation failed");
  }
  int optval = 1;
  setsockopt(serverfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
  struct sockaddr_in address = {0}; 
  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(serverfd, (struct sockaddr*)&address,sizeof(address))){
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  int listen_status = listen(serverfd,5);
  if (listen_status < 0){
    perror("listen failed");
    printf("%d",listen_status);
    close(serverfd);
  }
  printf("Server is listening on port 8080...\n");
  while (1){
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    int client_socket = accept(serverfd, (struct sockaddr*)&client_address, &client_len);

    if (client_socket < 0) {
      perror("Accept failed");
      continue; // Try accepting the next connection
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Connection accepted from %s:%d\n", client_ip, ntohs(client_address.sin_port));
    char* message = "Hello";
    write(client_socket,message,strlen(message));
    
    close(client_socket);
    printf("Connection with %s:%d closed.\n", client_ip, ntohs(client_address.sin_port));
  }
  close(serverfd);
}
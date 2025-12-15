#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <regex.h>
#include <resolv.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

int main(){
  int portno = 39485;
  struct addrinfo hints, *servinfo, *p;
  int rv, yes = 1;
  char port_str[16];
  snprintf(port_str, sizeof port_str, "%d", portno);

  memset(&hints, 0, sizeof hints);
  // hints.ai_family = AF_UNSPEC;     
  hints.ai_family = AF_INET;    
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_flags = AI_PASSIVE; 

  if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  int server_socket_fd;
  for (p = servinfo; p != NULL; p = p->ai_next) {
    server_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (server_socket_fd == -1) {
      perror("socket");
      continue;
    }

    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
      perror("setsockopt");
      close(server_socket_fd);
      exit(2);
    }

    if (bind(server_socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
      perror("bind");
      close(server_socket_fd);
      continue;
    }
    // we connected
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(3);
  }

  freeaddrinfo(servinfo);
  int MAX_CONNECTIONS = 5;
  if (listen(server_socket_fd, MAX_CONNECTIONS) == -1) {
    perror("listen");
    exit(2);
  }

  printf("server: waiting for connections on port %d...\n", portno);

  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  char client_request[512];
  
  for (;;) {
    sin_size = sizeof client_addr;
    int client_fd = accept(server_socket_fd, (struct sockaddr *)&client_addr, &sin_size);
    if (client_fd == -1) {
      perror("accept");
      // continue;
    }
    ssize_t recieved = recv(client_fd,client_request,512,0);
    if ((recieved) < 0){
        // break;
        printf("shit fked cus didn't recieve request\n");
    }
    client_request[recieved] = '\0';
    printf("This is what we received %s\n", client_request);
    // char * res = "GETFILE OK 26\r\n\r\nabcdefghijklmnopqrstuvwxyz";
    // till 4 is 12
    // char * res = "GETFILE OK 4\r\n\r\nabcd";
    // char *res = "GETFILE OK 600\r\n\r\nabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm...";
    // char *res = "GETFILE OK 511\r\n\r\ncdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk...";
    // char *res = "GETFILE OK 1024\r\n\r\nabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
// char *res ="GETFILE OK 600\r\n\r\n"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
//     char *res = "GETFILE OK 1024\r\n\r\n"
// "0000000000000000000000000000000000000000000000000000000000000000"
// "1111111111111111111111111111111111111111111111111111111111111111"
// "2222222222222222222222222222222222222222222222222222222222222222"
// "3333333333333333333333333333333333333333333333333333333333333333"
// "4444444444444444444444444444444444444444444444444444444444444444"
// "5555555555555555555555555555555555555555555555555555555555555555"
// "6666666666666666666666666666666666666666666666666666666666666666"
// "7777777777777777777777777777777777777777777777777777777777777777"
// "8888888888888888888888888888888888888888888888888888888888888888"
// "9999999999999999999999999999999999999999999999999999999999999999"
// "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
// "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
// "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
// "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"
// "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
    char *res = "GETFILE FILE_NOT_FOUND\r\n\r\n";
    // const char *res = "GETFILE OK ";
    // char *res = "GETFILE O";
    // char* res = "GETFILE ERROR\r\n\r\n";
    size_t bytes_sent = send(client_fd,res,strlen(res),0);
    printf("sending %zu bytes:", bytes_sent);
    if (bytes_sent == -1) {
    perror("send");
  }
    // s.send(b"GETFILE OK ")
    close(client_fd);
  }
}
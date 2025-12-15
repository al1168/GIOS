// #define _POSIX_C_SOURCE 200112L
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h> 

#define BUFSIZE 1024

#define USAGE                                                        \
    "usage:\n"                                                         \
    "  echoserver [options]\n"                                         \
    "options:\n"                                                       \
    "  -m                  Maximum pending connections (default: 5)\n" \
    "  -p                  Port (Default: 39483)\n"                    \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help",          no_argument,            NULL,           'h'},
    {"maxnpending",   required_argument,      NULL,           'm'},
    {"port",          required_argument,      NULL,           'p'},
    {NULL,            0,                      NULL,             0}
};

void sigchld_handler(int s){
    (void)s;
    int prev_errno = errno;
    while (waitpid(-1,NULL,WNOHANG));
    errno = prev_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }
    else{
        return &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
}

int main(int argc, char **argv) {
    int portno = 39483; /* port to listen on */
    int option_char;
    int maxnpending = 5;
  
    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'h': // help
            fprintf(stdout, "%s ", USAGE);
            exit(0);
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;                                        
        case 'm': // server
            maxnpending = atoi(optarg);
            break; 
        default:
            fprintf(stderr, "%s ", USAGE);
            exit(1);
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }

    char buffer[16];
    

  /* Socket Code Here */
    int server_fd; 
    int client_fd;
    // addrinfo is a modern way to interact with both ipv4 and ipv6 with one interface
    // instead of managing both ipv4 and ipv6 seperately with sockaddr_in and sockaddrin6,
    // we can just use the addrinfo interface
    // int MAXDATASIZE = 10000;
    // char buffer[MAXDATASIZE];
    
    struct sockaddr_storage client_addr; 
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    // struct sigaction sa;
    int enabled=1;
    char addrerssString[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    // first need to find a available addresses?
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    char portStr[20];
    sprintf(portStr, "%d", portno);
    if ((rv = getaddrinfo(NULL,portStr, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    int off = 0;
    // get back 
    for (p = servinfo; p!=NULL; p = p->ai_next){
       if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
        perror("server: socket");
        continue;
       }
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(int)) == -1){
            perror("setsockopt");
            continue;
        }
        
        if (p->ai_family == AF_INET6) {
            // ensure dual-stack: 0 => accept IPv4-mapped too
            if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off) == -1) {
                perror("setsockopt IPV6_V6ONLY");
                // not fatal on Linux if default is already 0, but keep going
            }

        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1){
            close(server_fd);
            perror("server:bind");
            continue;
        }
        break;
    }
    

    if (p == NULL){
        fprintf(stderr, "server: failed to bind\n");
    }
    if (listen(server_fd, maxnpending) == -1){
        perror("listen");
    } 
    // remove zombies when using fork
    // sa.sa_handler = sigchld_handler;
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = SA_RESTART;
    // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    //     perror("sigaction");
    //     exit(1);
    // }

    for (;;){
        sin_size = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (client_fd == -1){
            perror("accept");
            continue;
        }
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*) &client_addr),addrerssString, sizeof addrerssString );

        //  if (!fork()) {
        //     close(server_fd);
        //     if (send(client_fd, "Hello, dddworld!", 13, 0) == -1)
        //         perror("send");
        //     close(client_fd);
        //     exit(0);
        // }
        int numbytes;
        if ((numbytes = recv(client_fd, buffer, sizeof(buffer), 0)) == -1) {
            perror("recv");
            exit(1);
        }
        // buffer[numbytes] = '\0';
        // printf("server: received '%s'\n",buffer);
        // fprintf()
        // fprintf(stdout, "%s", buffer);
        printf("%s", buffer);
        send(client_fd, buffer,sizeof(buffer), 0);
        close(client_fd);  // parent doesn't need this
    }
    freeaddrinfo(servinfo);

}

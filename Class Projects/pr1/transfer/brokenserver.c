#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <sys/stat.h>
#define BUFSIZE 512

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n"   \
    "  -p                  Port (Default: 29345)\n"          \
    "  -h                  Show this help message\n"         \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len) {
        ssize_t n = send(fd, p, len, 0);
        if (n > 0) { p += n; len -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}
long getFileSize(const char *filename) {
    FILE *fp;
    long size = -1; 
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        return size;
    }
    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
    } else {
        perror("Error seeking to end of file");
    }
    fclose(fp); 
    return size;
}
int main(int argc, char **argv)
{
    int portno = 29345;             /* port to listen on */
    int option_char;
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'f': // file to transfer
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        }
    }


    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */

    /* 
    In this part of the assignment, you will implement code to transfer files on a server to clients.

    The basic mechanism is simple: when the client connects to the server,
    the server opens a pre-defined file (from the command line arguments),
    reads the contents from the file and writes them to the client via the
    socket (connection). When the server is done sending the file contents, 
    the server closes the connection.
    */
    int server_fd;
    int client_fd;
    char addrerssString[INET6_ADDRSTRLEN];
    char portStr[20];
    sprintf(portStr, "%d", portno);
    
    // serverSocketInit(&server_fd,5,portStr);

    int maxnpending = 5;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int off = 0;
    int enabled=1;
    int rv;
    if ((rv = getaddrinfo(NULL,portStr, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
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
        freeaddrinfo(servinfo);
        break;
    }
    

    if (p == NULL){
        fprintf(stderr, "server: failed to bind\n");
    }
    if (listen(server_fd, maxnpending) == -1){
        perror("listen");
    }

    struct sockaddr_storage client_addr; 
    socklen_t sin_size;
    for(;;){
        sin_size = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (client_fd == -1){
            perror("accept");
            continue;
        }
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*) &client_addr),addrerssString, sizeof addrerssString );
        // printf("client at %s is connected",addrerssString);
        FILE * pFile;
        // char *myfilename = "text/6200.txt";
        // pFile = fopen(myfilename,"rb");
        pFile = fopen(filename,"rb");
        
        if (pFile == NULL){
            perror("fopen failed");
        }

        // send filename 

        size_t name_len = strlen(filename);
        uint32_t nl_be = htonl((uint32_t)name_len);
        // printf("Size of filename: %jd",(intmax_t)name_len);

        if (send_all(client_fd, &nl_be, sizeof nl_be) < 0){
             fclose(pFile); return -1;
        }
        if (send_all(client_fd, filename, name_len) < 0) { 
            fclose(pFile); 
            return -1;
        }

        // get file 
        struct stat st;
        if (stat(filename, &st) == -1) { 
            perror("stat");
        }

        off_t filesize = st.st_size;
        uint32_t filesize_networkByte = htonl((uint32_t)filesize);
        if (send_all(client_fd, &filesize_networkByte, sizeof filesize_networkByte) < 0){
            fclose(pFile);
            // printf("something went wrong with sending");
            return -1;
        }
        // printf("File size: %zu");
        // printf("filesize: %jd\n", (intmax_t)filesize);
        // printf("filesize: %jd\n", (intmax_t)filesize);
        // read the contents


        
        size_t n;
        char buffer[500];
        unsigned int totalBytesSent = 0;

        while ((n = fread(buffer, 1, sizeof buffer, pFile)) > 0) {
            size_t off = 0; // total number of bytes read for current iteration
            while (off < n) {
                ssize_t sent = send(client_fd, buffer + off, n - off, 0);
                if (sent < 0) {
                    if (errno == EINTR) continue;  // interrupted, retry
                    perror("send");
                    return -1;
                }
                off += (size_t)sent;
                totalBytesSent += (unsigned int) sent;
            }
        }
        // printf("sent %u bytes\n", totalBytesSent);
        fclose(pFile);
    }
    
}

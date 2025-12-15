#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFSIZE 512

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -s                  Server (Default: localhost)\n"      \
  "  -p                  Port (Default: 29345)\n"            \
  "  -o                  Output file (Default cs6200.txt)\n" \
  "  -h                  Show this help message\n"           \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"output", required_argument, NULL, 'o'},
    {"server", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// int recv_file_size(int sockfd, uint32_t *filesize) {
//     uint32_t net_size;
//     ssize_t recvd = recv(sockfd, &net_size, sizeof(net_size), MSG_WAITALL);
//     if (recvd == -1) {
//         perror("recv");
//         return -1;
//     }
//     if (recvd != sizeof(net_size)) {
//         fprintf(stderr, "recv: expected %zu bytes, got %zd\n",
//                 sizeof(net_size), recvd);
//         return -1;
//     }

//     *filesize = ntohl(net_size);   // convert to host order
//     return 0;
// }

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    unsigned short portno = 29345;
    char *hostname = "localhost";
    char *filename = "cs6200.txt";

    setbuf(stdout, NULL);

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 's': // server
            hostname = optarg;
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    char portStr[20];
    sprintf(portStr, "%d", portno);
    int sockfd; 
    int rv;
    char s[INET6_ADDRSTRLEN];
    struct addrinfo hints, *servinfo, *p;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, portStr, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        printf("connection succesful");
        freeaddrinfo(servinfo);
        break;
    }
    // uint32_t filesize;
    // int isSizeReceived = recv_file_size(sockfd, &filesize);
    // if (isSizeReceived !=0){
    //     perror("client: Failed to get size");
    // }
    // else{
    //     printf("Sent the Filesize number: %u\n", filesize);
    // }

    // receive file in bytes
    char buffer[BUFSIZE];
     
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        close(sockfd);
        return 3;
    }
    size_t total_bytes_written = 0;
    ssize_t total_bytes_recv = 0;
    for (;;) {
        ssize_t n = recv(sockfd, buffer, sizeof buffer, 0);
        if (n == 0) break;             
        if (n < 0) {
            if (errno == EINTR) continue; 
            perror("recv");
            fclose(fp);
            close(sockfd);
            return 4;
        }
        total_bytes_recv += n;
        size_t off = 0;
        while (off < (size_t)n) {
            
        size_t w = fwrite(buffer + off, 1, (size_t)n - off, fp);
        if (w == 0) {
            if (ferror(fp)) { perror("fwrite"); }
            fclose(fp);
            close(sockfd);
            return 5;
        }
        total_bytes_written += w;
        off += w;
        }
  }
    fclose(fp);
    close(sockfd);
    printf("Total bytes written: %zu\n", total_bytes_written);
    printf("Total bytes received: %zd\n", total_bytes_recv);
}
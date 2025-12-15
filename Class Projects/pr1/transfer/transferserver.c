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
#include <sys/stat.h>
#include <arpa/inet.h>
#define BUFSIZE 512
#define MAX_CONNECTIONS 5
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
// static void send_file_size(int socket_fd, FILE *fp,char* filename){
//       struct stat st;
//       if (stat(filename, &st) == -1) { 
//             perror("stat");
//       }

//       off_t filesize = st.st_size;
//       uint32_t filesize_networkByte = htonl((uint32_t)filesize);
//       ssize_t sent = send(socket_fd, &filesize_networkByte, sizeof(filesize_networkByte), 0);
//       if (sent ==0){
//         perror("Sent bytes was 0");
//         exit(EXIT_FAILURE);
//     }
//     printf("Sent the Filesize number: %u\n", ntohl(filesize_networkByte));
// } 
static void send_file(int socket_fd, FILE *fp) {
  char buffer[BUFSIZE];
  for (;;) {
    size_t bytes_to_send = fread(buffer, 1, sizeof buffer, fp);
    if (bytes_to_send == 0) {
      if (ferror(fp)) {
        perror("fread");
        exit(1);
      }
      break; 
    }
    size_t bytes_sent = 0;
    while (bytes_sent < bytes_to_send) {
      ssize_t n = send(socket_fd, buffer + bytes_sent, bytes_to_send - bytes_sent, 0);
      if (n < 0) {
        perror("send");
        exit(1);
      }
      bytes_sent += (size_t)n;
    }
  }
}
int main(int argc, char **argv)
{
    int portno = 29345;             /* port to listen on */
    int option_char;
    char *filename = "6200.txt"; /* file to transfer */
    // char *filename = "text/testfile.bin"; /* file to transfer */
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

  struct addrinfo hints, *servinfo, *p;
  int rv, yes = 1;
  char port_str[16];
  snprintf(port_str, sizeof port_str, "%d", portno);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     
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

  if (listen(server_socket_fd, MAX_CONNECTIONS) == -1) {
    perror("listen");
    exit(2);
  }

  printf("server: waiting for connections on port %d...\n", portno);

  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  for (;;) {
    sin_size = sizeof client_addr;
    int client_fd = accept(server_socket_fd, (struct sockaddr *)&client_addr, &sin_size);
    if (client_fd == -1) {
      perror("accept");
      continue;
    }
    // char *myfilename = "text/6200.txt";
    // char *myfilename = "text/testfile.bin";
    FILE *fp  = fopen(filename,"rb");

    // FILE *fp = fopen(filename, "rb");
    if (!fp) {
      perror("fopen");
      close(client_fd);
      continue;
    }
    // send_file_size(client_fd,fp, myfilename);
    send_file(client_fd, fp);
    fclose(fp);
    close(client_fd);
  }

  return 0;

}
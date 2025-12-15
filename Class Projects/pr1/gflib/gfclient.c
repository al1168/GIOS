#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "gfclient-student.h"
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h> 
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#define BUFSIZE 512
#define HEADERMAXSIZE 47
#define STORAGESIZE 1024
typedef void (*callback_t)(void *, size_t, void *);
struct gfcrequest_t {
  int socket_fd; 
  unsigned short port; 
  const char *path; 
  const char *server;
  gfstatus_t status; 
  size_t bytes_received; 
  size_t file_size; 
  char *response; 
  char *content; 
  callback_t header_func;
  void *header_args; 
  callback_t write_func;
  void *write_args;
};
bool is_integer(const char *str) {
    char *endptr;
    strtol(str, &endptr, 10);
    return (*str != '\0' && *endptr == '\0');
}
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int send_all(int sockfd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;     
            perror("send");
            return -1;
        }
        if (n == 0) {          
            fprintf(stderr, "send: wrote 0 bytes\n");
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}
int build_and_send_request(int sockfd, const char *path) {
    char request[512];
    memset(request,0,sizeof(request));
    int numchars = snprintf(request, sizeof request, "GETFILE GET %s\r\n\r\n", path);
    if (numchars < 0 || (size_t)numchars >= sizeof request) {
        fprintf(stderr, "Request too long\n");
        return -1;
    }
    ssize_t sent =  send(sockfd, request, strlen(request),0);
    printf("sent: %zd\n", sent);
    return sent;
    // return send_all(sockfd, request, (size_t)numchars); 
}

bool isEqual(char* str1, char* str2){
    return strcmp(str1, str2) == 0 ? true : false;
}


// called when we have enough bytes for header or end of bytes
int validateAndParseHeader(gfcrequest_t **gfr, char* storage, size_t usedStorage){
    // set the fields in gfr after fully parsing through header
    // also add the extra bytes after \r\n\r\n to (*gfr)->bytesrecieved
    // 
    printf("\nWE ARE CALLLING THE VALIDATING PARASE FUNCTION NOW \n");
    char *DELIMITER = " ";
    char* scheme = strtok(storage, DELIMITER);
    if (scheme == NULL){
        printf("scheme is NULL\n");
        return 1;
    }
    if (!isEqual(scheme,"GETFILE")){
        printf("scheme error:[%s] is not GETFILE\n", scheme);
        return 1;
    }

    // check if its an OK STATUS OR NOT

    char  *status = strtok(NULL,DELIMITER);
    char final_status[15]; 
    memset(final_status, 0, sizeof final_status);
    if (status == NULL){
        printf("status is NULL\n");
        return 1;
    }
    bool isOKstatus = false;
    char *termStart = strstr(status,"\r\n\r\n");
    if ( termStart == NULL){
        // this means OK
        if(!isEqual(status,"OK")){
           return 1;
        }
        isOKstatus = true;
        memcpy(final_status, status, strlen(status));
    } else {
        memcpy(final_status, status, (int)(termStart - status));
    }
    char* filesizeStr = NULL;
    char finalFilesizeStr[21];
    char *termStartForfileSize;
    memset(finalFilesizeStr,0,sizeof(finalFilesizeStr));
    if (isOKstatus == true){
        filesizeStr = strtok(NULL,DELIMITER);
        if (filesizeStr == NULL){
            return 1;
        }
        termStartForfileSize = strstr(filesizeStr,"\r\n\r\n");
        if (termStartForfileSize == NULL){
            return 1;
        }
        memcpy(finalFilesizeStr,filesizeStr,(int)(termStartForfileSize - filesizeStr));
    }
    printf("scheme: %s\n",scheme);
    printf("Status: %s\n", final_status);
    if (filesizeStr != NULL){
        printf("filesize: %s\n",finalFilesizeStr);
        if(is_integer(finalFilesizeStr)){
            char *endptr;
            long val = strtol(finalFilesizeStr, &endptr, 10);
            printf("SETTING FILE_SIZE TO %zu\n", (size_t)val);
            (*gfr)->file_size = (size_t)val;
        }
        else{
            return 1;
        }
    }
    if (!isEqual(final_status,"OK") && !isEqual(final_status,"FILE_NOT_FOUND") && !isEqual(final_status,"ERROR") && !isEqual(final_status,"INVALID")){
        printf("status error:[%s] is not OK, FILE_NOT_FOUND, ERROR, OR INVALID\n", status);
        // (*gfr)->status = GF_INVALID;
        return 1;
    }
    if(isOKstatus){
        // size_t numContentBytes = usedStorage - (size_t)(termStartForfileSize + 4);
        size_t numContentBytes = usedStorage - ((termStartForfileSize - storage) + 4);
        printf("numContentBytes:%zu\n", numContentBytes);
        (*gfr)->write_func(termStartForfileSize+4,numContentBytes,(*gfr)->write_args);
        (*gfr)->bytes_received += numContentBytes;
    } 
    if (isEqual(final_status,"FILE_NOT_FOUND")){
        return 2;
    }
    if (isEqual(final_status,"ERROR")){
        return 3;
    }
    if (isEqual(final_status,"INVALID")){
        return 1;
    }
    // else {
    //     (*gfr)->status = gfc_strstatus(status);
    // }
    // for status == OK,
        // gotta extract filesize from request
        // gotta get the strip content off the \n\n\n\n
    // for other status
        //
    return 0;
}

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
    if (gfr == NULL || *gfr == NULL) return;
    if ((*gfr)->socket_fd >= 0) {
        close((*gfr)->socket_fd);
        (*gfr)->socket_fd = -1;
    }
    free((*gfr)->response);
    (*gfr)->response = NULL;
    free((*gfr)->content);
    (*gfr)->content = NULL;
    free(*gfr);
    *gfr = NULL;


}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // not yet implemented
  
  return (*gfr)->file_size;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // not yet implemented

 return (*gfr)->bytes_received;
}

gfcrequest_t *gfc_create() {
  // not yet implemented
  gfcrequest_t *req = calloc(1,sizeof(gfcrequest_t));
   if (req) {
      req->socket_fd = -1;   // sentinel: not an open FD
      req->status = GF_ERROR;
      req->bytes_received = 0;
      req->file_size = 0;
      req->response = NULL;
      req->content = NULL;
      req->header_func = NULL;
      req->write_func = NULL;
      req->header_args = NULL;
      req->write_args = NULL;
  }


  return req;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  // not yet implemented
    return (*gfr)->status;

}

void gfc_global_init() {}

void gfc_global_cleanup() {}

/*
 * Performs the transfer as described in the options.  Returns a value of 0
 * if the communication is successful, including the case where the server
 * returns a response with a FILE_NOT_FOUND or ERROR response.  If the
 * communication is not successful (e.g. the connection is closed before
 * transfer is complete or an invalid header is returned), then a negative
 * integer will be returned.
 */
int gfc_perform(gfcrequest_t **gfr) {
  // not yet implemented
  if (gfr == NULL){
    perror("gfr was NULL");
    return -1;
  }
  (*gfr)->status = GF_ERROR;
  // not yet implemented
//   int yes = 1;
//  setsockopt((*gfr)->socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  // create socket connection
  int sockfd; 
  char s[INET6_ADDRSTRLEN]; // store inet6 address
  // 
  struct addrinfo hints, *servinfo, *p;
  memset(&hints,0,sizeof(hints));
//   hints.ai_family = AF_UNSPEC;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  char portStr[20];
  sprintf(portStr, "%d", (*gfr)->port);
  int rv = getaddrinfo((*gfr)->server,portStr,&hints,&servinfo);

  if (rv != 0){
    fprintf(stderr,"Failure at getaddrinfo, params: %s, port: %s\n",(*gfr)->server,portStr);
    return -1;
  }
    int connected = 0;
    sockfd = -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            sockfd = -1;
            continue;
        }
    connected = 1;
    printf("we are connected!");
    break;
    }
  if (sockfd < 0){
    (*gfr)->status = GF_ERROR;
    
    return -1;
  }  
  freeaddrinfo(servinfo);

    if (!connected) {
        fprintf(stderr, "failed to connect to %s:%d\n", (*gfr)->server, (*gfr)->port);
        return -1;
    }
//   build_and_send_request(sockfd,(*gfr)->path);

if (build_and_send_request(sockfd, (*gfr)->path) < 0) {
    close(sockfd);
    (*gfr)->socket_fd = -1;
    return -1;
}
  
  (*gfr)->status = GF_ERROR;
  char headerBuffer[BUFSIZE];
  char buffer[BUFSIZE];
  char contentBuffer[BUFSIZE];
  char storage[BUFSIZE];
  bool headReceived = false;
  memset(buffer, 0, sizeof buffer);
  memset(headerBuffer, 0, sizeof headerBuffer);
  memset(contentBuffer, 0, sizeof contentBuffer);
  memset(storage, 0, sizeof storage);
  printf("time to recieved data\n");
   ssize_t usedStorage = 0;
   int isValid = -1;
   ssize_t current_bytes;
//    memset(buffer,0,sizeof(buffer));
//    printf("starting while loop\n");
   while((current_bytes = recv(sockfd, buffer,sizeof(buffer)-1, 0))>0){

    // printf("buffer:[%s]\n", buffer);
    if (current_bytes <0){
        printf("The connection was suddenly terminated...\n");
        printf("ending recving...\n");
        (*gfr)->status = GF_INVALID;
        isValid = 1;
        if (errno == ECONNRESET) {
            fprintf(stderr, "Connection reset: %s\n", strerror(errno));
        break;
        }
    }
    if (headReceived == false && current_bytes >0){
        // add to storage
        memcpy(storage+usedStorage,buffer,current_bytes);
        usedStorage += current_bytes;
        // printf("usedBytes: %zd\n",usedStorage);
        if (strstr(storage,"\r\n\r\n") == NULL){
            continue;
        }
        // OK/GF_OK - no issues 0
        // INVALID - incomplete header/ communication issues 1
        // FILE_NOT_FOUND
        isValid = validateAndParseHeader(gfr,storage,usedStorage);
        headReceived = true;
        // printf("isValid is : %d\n", isValid);
        if(isValid == 1){
            (*gfr)->status = GF_INVALID;
            break;
        }
        else if (isValid == 2){
            (*gfr)->status = GF_FILE_NOT_FOUND;
            break;
        } else if(isValid == 3){
            (*gfr)->status = GF_ERROR;
            break;
        }
    } else {
        // header is received, all day must be content data
        // 

        if ((*gfr)->bytes_received + current_bytes > (*gfr)->file_size){
            // write only what is need from buffer
            size_t remaining = (*gfr)->file_size - (*gfr)->bytes_received;
            (*gfr)->bytes_received += remaining;     
            (*gfr)->write_func(buffer,remaining,(*gfr)->write_args);
            break;
        }
        (*gfr)->bytes_received += current_bytes;
        (*gfr)->write_func(buffer,current_bytes,(*gfr)->write_args);
        if ((*gfr)->bytes_received == (*gfr)->file_size){
            break;
        }
    }
    if (current_bytes == 0){
        printf("client:we have received all bytes from the server\n");
        printf("closing recving...\n");
        break;
    }
   }
   close(sockfd);
   if(headReceived == false){
    printf("headReceived is still false after loop\n");
    (*gfr)->status = GF_INVALID;
    return -1;
   }
   printf("FINAL RESULT:\n expected byes: %zu, recievedBytes:%zu\n", gfc_get_filelen(gfr),gfc_get_bytesreceived(gfr));
   if (isValid == 0 && (*gfr)->bytes_received == (*gfr)->file_size){
        (*gfr)->status = GF_OK;
        return 0;
   }
//     if (isValid == 0){
//         (*gfr)->status = GF_OK;
//         return 0;
//    }
   if (isValid == 0){
    (*gfr)->status = GF_OK;
   }
   if (isValid == 1){
    // invalid
    return -1;
   }
   if (isValid == 2){
    // file not found, 
    return 0;
   }
   if (isValid == 3){
    return 0;
   }
  return -1;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
    (*gfr)->port = port;
}

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
    (*gfr)->server = server; 
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
    (*gfr)->header_func = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
    (*gfr)->header_args = headerarg;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
    (*gfr) -> path = path;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
    (*gfr)->write_func = writefunc; 
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
    (*gfr)->write_args = writearg;
}

const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_OK: {
      strstatus = "OK";
    } break;

    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;

   case GF_INVALID: {
      strstatus = "INVALID";
    } break;
   
   case GF_ERROR: {
      strstatus = "ERROR";
    } break;

  }

  return strstatus;
}
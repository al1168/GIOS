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

// typedef struct {
//     char scheme[8];
//     char status[15];
//     // char filesize[21];
//     long filesize;
// } response_t;
typedef struct {
    char scheme[32];
    char status[32];
    size_t filesize;   // or long, but match your printf later
} response_t;
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef void (*callback_t)(void *, size_t, void *);
struct gfcrequest_t {
  int req_id;
  unsigned short port;
  const char* path;
  const char *server;
  callback_t writefunc;
  void *writeargs;
  callback_t headerfunc;
  void* headerarg;
  size_t filelen;
  gfstatus_t status;
  size_t bytesreceived;
  size_t expectedBytes;

};

 // Modify this file to implement the interface specified in
 // gfclient.h.

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  free(*gfr);
  *gfr = NULL;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // not yet implemented
  if (gfr == NULL){
    perror("gfr was NULL");
    return (size_t)-1;
  }
  return (*gfr)->filelen;
  
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // not yet implemented
  if (gfr == NULL){
    perror("gfr was NULL");
    return (size_t)-1;
  }
  return (*gfr) -> bytesreceived;
}

gfcrequest_t *gfc_create() {
  // not yet implemented
  
  gfcrequest_t *req = calloc(1,sizeof(gfcrequest_t));
  return req;
  // return &req;
  // return (gfcrequest_t *)NULL;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  // not yet implemented
  return (*gfr) -> status;
  // return -1;
}

void gfc_global_init() {
  
}
// static int send_all(int sockfd, const char *buf, size_t len) {
//     size_t total = 0;
//     while (total < len) {
//         ssize_t n = send(sockfd, buf + total, len - total, 0);
//         if (n < 0) {
//             if (errno == EINTR) continue;     
//             perror("send");
//             return -1;
//         }
//         if (n == 0) {          
//             fprintf(stderr, "send: wrote 0 bytes\n");
//             return -1;
//         }
//         total += (size_t)n;
//     }
//     return 0;
// }

int build_and_send_request(int sockfd, const char *path) {
    char request[512];
    memset(request,0,sizeof(request));
    int numchars = snprintf(request, sizeof request, "GETFILE GET %s\r\n\r\n", path);
    if (numchars < 0 || (size_t)numchars >= sizeof request) {
        fprintf(stderr, "Request too long\n");
        return -1;
    }
    send(sockfd, request, strlen(request),0);
    // return send_all(sockfd, request, (size_t)numchars); 
    return 0;
}
void gfc_global_cleanup() {}
// check for 
/* 
scheme issue 1
status issue 2
filesizeStrWithNullTerm is null 3
can't find nullterm 4
*/
size_t checkForHeader(char *storage, size_t usedSpace, response_t *headerInfo,char *contentBuffer, gfcrequest_t **gfr){
  const char *delimiter = " ";
  
  char* scheme = strtok(storage, delimiter);
  if (strcmp(scheme, "GETFILE") != 0){
    perror("scheme doesn't match GETFILE");
    printf("status: %s != GETFILE\n",scheme);
    return 1;
  }

  memcpy(headerInfo->scheme,scheme,strlen(scheme)+1);
  char* status = strtok(NULL, delimiter);
  printf("tokenized status: %s\n", status);
  if (strcmp(status, "OK") != 0 && strcmp(status, "FILE_NOT_FOUND") != 0 && strcmp(status, "ERROR") != 0 && strcmp(status, "INVALID") != 0){
    perror("status don't match\n");
    printf("status: %s\n",status);
    return 2;
  }
  // headerInfo->status 
  memcpy(headerInfo->status,status,strlen(status)+1);
  // (*gfr)->status = status;
  // memcpy((*gfr)->status,status,strlen(status)+1);
  if (strcmp(status,"OK") == 0){
    (*gfr)->status = GF_OK;
  }

  if (strcmp(status,"FILE_NOT_FOUND") == 0){
    (*gfr)->status = GF_FILE_NOT_FOUND;
  }

  if (strcmp(status,"ERROR") == 0){
    (*gfr)->status = GF_ERROR;
  }


  long filesize = -1; 
  // char* filesizeStr;
  char* nullterm;
  if (strcmp(status, "OK") == 0){
    char* filesizeStrWithNullTerm = strtok(NULL, delimiter);
    if (filesizeStrWithNullTerm == NULL){
      return 3;
    }
    char* indexOfNullTerm = strstr(filesizeStrWithNullTerm, "\r\n\r\n");
    // printf("This is index of null term: %s\n", indexOfNullTerm);
    // check for numerical numbers
    char *endptr;
    long num = strtol(filesizeStrWithNullTerm, &endptr, 10);
    filesize = num;
    printf("found filesize %ld\n", filesize);
    (*gfr)->filelen = filesize;
    nullterm = indexOfNullTerm; 
    // memcpy(headerInfo->filesize,filesizeStrWithNullTerm,(size_t)(filesizeStrWithNullTerm-indexOfNullTerm));
    headerInfo->filesize = num;
  }else{
    nullterm = strtok(NULL, delimiter);
    if (strcmp(nullterm, "\r\n\r\n") !=0){
      perror("can't find nullterm\n");
      printf("%s != \r\n\r\n",nullterm);
      return 4;
    }
  }
  
  size_t prefix = (size_t)(nullterm - storage);
  size_t total  = prefix + 4;
  // printf("start of nullterm: %zu\n end of nullterm %zu\n",prefix, total);
  // printf("UsedSpace: %zu\n",usedSpace);
  // printf("size: %zu\n",usedSpace-total);
  memcpy(contentBuffer,nullterm+4,usedSpace-total);
  // printf("This is content Buffer:\n%s\n",contentBuffer);
  (*gfr)->bytesreceived += strlen(contentBuffer); // edit this when u come back
  return 0;
}

void read_header(int sockfd, char* buffer, bool* headReceived,char *headerBuffer, char *contentBuffer, response_t* headerInfo,gfcrequest_t **gfr){
// headReceived
// keep looking for header in buffer, then store rest of bytes after header into contentBuffer
char storage[STORAGESIZE];
size_t usedStorage = 0;
size_t totalStorage = STORAGESIZE;
// bool errorOccured = false;
memset(storage,0,sizeof(storage));
  for(;;){
    memset(buffer,0,BUFSIZE);
    ssize_t recievedBytes  = recv(sockfd,buffer,BUFSIZE,0);
    
    printf("recieved %zu bytes\n", recievedBytes);
    printf("recieved:\n");
    if(recievedBytes < 0){
      size_t index = checkForHeader(storage,usedStorage,headerInfo,contentBuffer,gfr);
      if (index ==0){
        *headReceived = true;
        printf("headReceived should bet set to True, %d\n",*headReceived);
      }
      return;
    }
    // printf("my buffer %s\n",buffer );
    if (recievedBytes == 0){
      printf("readheader: we recieved all bytes\n");
      break;
    }
    // copy into storage
    if (usedStorage + recievedBytes > totalStorage){
      // something is wrong
      perror("Um, it should not have this big of a size");
      return;
    }
    memcpy((storage+usedStorage),buffer,(size_t)recievedBytes);
    usedStorage += recievedBytes;
    printf("usestorage updated: %zu\n", usedStorage);
    // check for GET 
    if (usedStorage >= HEADERMAXSIZE){
      size_t index = checkForHeader(storage,usedStorage,headerInfo,contentBuffer,gfr);
      printf("index of last terminating char is: %zu\n, ", index);
      if(index == 0){
        *headReceived = true;
        printf("headReceived should bet set to True, %d\n",*headReceived);
      }
      break;
    }
  }
  if (*headReceived == false && usedStorage > 0){
    size_t index = checkForHeader(storage,usedStorage, headerInfo, contentBuffer,gfr);
    if(index == 0){
        *headReceived = true;
    }
  }
  return;
}
int gfc_perform(gfcrequest_t **gfr) {
  
  if (gfr == NULL){
    perror("gfr was NULL");
    return -1;
  }
  (*gfr)->status = GF_ERROR;
  // not yet implemented

  // create socket connection
  int sockfd; 
  char s[INET6_ADDRSTRLEN]; // store inet6 address
  // 
  struct addrinfo hints, *servinfo, *p;
  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  char portStr[20];
  sprintf(portStr, "%d", (*gfr)->port);
  int rv = getaddrinfo((*gfr)->server,portStr,&hints,&servinfo);
  if (rv != 0){
    fprintf(stderr,"Failure at getaddrinfo, params: %s, port: %s\n",(*gfr)->server,portStr);
    freeaddrinfo(servinfo);  
    return -1;
  }
  // get first address that works
  for (p  = servinfo; p != NULL; p = p->ai_next){
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket create");
      continue;
  }

  // inet_ntop
    inet_ntop(p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
        perror("client: socket connect ");
        fprintf(stderr, "connect failed: %s\n", strerror(errno));
        close(sockfd);
        continue;
    }
    printf("connection successful\n");
    freeaddrinfo(servinfo);  
    break;
  }
  //  <scheme> <status> <length>\r\n\r\n<content>
  

  // INITAL REQUEST
  if (build_and_send_request(sockfd, (*gfr)->path) < 0) {
      // handle error
      perror("build_and_send_request errored:");
      close(sockfd);
      return -1;
  }
  
// get back header from server
char headerBuffer[BUFSIZE];
char buffer[BUFSIZE];
char contentBuffer[BUFSIZE];
bool headReceived = false;
memset(buffer, 0, sizeof buffer);
// ssize_t bytesd = recv(sockfd,buffer,BUFSIZE,0);
// printf("GOT: %zd\n",bytesd);
memset(headerBuffer, 0, sizeof headerBuffer);
memset(contentBuffer, 0, sizeof contentBuffer);
// ssize_t total_bytes_recieved; 
response_t *headerinfo = calloc(1, sizeof *headerinfo);
if (!headerinfo) {
    perror("calloc headerinfo");
    close(sockfd);
    return -1;
}
  for(;;){
    memset(buffer,0, sizeof(buffer));
    memset(contentBuffer,0, sizeof(contentBuffer));
    if (!headReceived){
      printf("calling read_header\n");
      read_header(sockfd,buffer,&headReceived, headerBuffer, contentBuffer, headerinfo,gfr);
      printf("READHEADER DONE\n");
      printf("Here is the headerinfo data:\n");
      printf("scheme:%s\n", headerinfo->scheme);
      printf("expected filesize:%ld\n", headerinfo->filesize);
      printf("status:%s\n",headerinfo->status);
      (*gfr)->expectedBytes = headerinfo->filesize;
      if (headReceived == false){
        printf("THIS WAS AN INVALID RESPONSE, ");
        (*gfr)->status = GF_INVALID;
        close(sockfd);
        free(headerinfo);
        return -1;
      }
      (*gfr)->writefunc(contentBuffer,strlen(contentBuffer),(*gfr)->writeargs);
      printf("added %zu from contentbuffer\n", strlen(contentBuffer));
      continue;
    }
    ssize_t recv_bytes = recv(sockfd, buffer, sizeof(buffer), 0);
    (*gfr)->bytesreceived += recv_bytes; 
    printf("still need:%zu\n", (*gfr)->expectedBytes - (*gfr)->bytesreceived);
        // socket closed?
    printf("recv: %zu",recv_bytes);
    if (recv_bytes < 0) {
      printf("recv_byrs is less than 0");
      if (errno == EINTR){
        continue;
      }
      perror("recv"); (*gfr)->status = GF_ERROR; close(sockfd); return -1;
      free(headerinfo);

    }
    if (recv_bytes == 0 ){
      // no more bytes to recieved
      // ERROR: if no header, then its an error
      printf("recieved all bytes from server\n");
      if (!headReceived ){
        (*gfr)->status = GF_ERROR;
        close(sockfd);
        free(headerinfo);
        return -1;
      }
      break;
    }


    // check if header is in there
    (*gfr)->writefunc(buffer,recv_bytes,(*gfr)->writeargs);
    // size_t off = 0;
  }
  free(headerinfo);
  close(sockfd);
  char responseStatus = (*gfr)->status;
  // gfc_strstatus(res)
  if (responseStatus == GF_ERROR){
    return 0;
  }
  if (responseStatus == GF_FILE_NOT_FOUND){
    return 0;
  }
  if (responseStatus == GF_OK && (*gfr)->bytesreceived == (*gfr)->expectedBytes){
    return 0;
  }
  return -1;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> port = port;
}

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  if (server == NULL){
    perror("server was NULL");
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> server = server;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
  if (headerfunc == NULL){
    perror("headerfunc was NULL");
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
    if (headerarg == NULL){
    perror("headerfunc was NULL");
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> headerarg = headerarg;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
  if (path == NULL){
    perror("path was NULL");
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> path = path;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
  if (writefunc == NULL){
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> writefunc =  writefunc;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
  if (writearg == NULL){
    return;
  }
  if (gfr == NULL){
    perror("gfr was NULL");
    return;
  }
  (*gfr) -> writeargs = (FILE*) writearg ;
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

#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <printf.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "cache-student.h"
#include "gfserver.h"
#include "shm_channel.h"
#include <pthread.h> 
// Note that the -n and -z parameters are NOT used for Part 1 
                        
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 8)\n"                      \
"  -p [listen_port]    Listen port (Default: 25362)\n"                                 \
"  -s [server]         The server to connect to (Default: GitHub test data)\n"     \
"  -t [thread_count]   Num worker threads (Default: 8 Range: 200)\n"              \
"  -z [segment_size]   The segment size (in bytes, Default: 5712).\n"                  \
"  -h                  Show this help message\n"


// Options
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"segment-count", required_argument,      NULL,           'n'},
  {"listen-port",   required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},

  {"hidden",        no_argument,            NULL,           'i'}, // server side 
  {NULL,            0,                      NULL,            0}
};


//gfs
static gfserver_t gfs;
static int global_msqid = -1;
static shm_segment_t* global_shm_pool = NULL;
static int global_nsegments = 0;

//handles cache
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static void _sig_handler(int signo){
  if (signo == SIGTERM || signo == SIGINT){
    //cleanup could go here
       if (global_shm_pool != NULL) {
      for (int i = 0; i < global_nsegments; i++) {
        if (global_shm_pool[i].addr != NULL) {
          munmap(global_shm_pool[i].addr, global_shm_pool[i].segsize);
        }
        if (global_shm_pool[i].fd != -1) {
          close(global_shm_pool[i].fd);
        }
        if (global_shm_pool[i].name[0] != '\0') {
          shm_unlink(global_shm_pool[i].name);
        }
        pthread_mutex_destroy(&global_shm_pool[i].lock);
      }
      free(global_shm_pool);
      printf("Proxy: Cleaned up %d shared memory segments\n", global_nsegments);
    }
    gfserver_stop(&gfs);
    exit(signo);
  }
}

int main(int argc, char **argv) {
  int option_char = 0;
  char *server = "https://raw.githubusercontent.com/gt-cs6200/image_data";
  unsigned int nsegments = 8;
  unsigned short port = 25362;
  unsigned short nworkerthreads = 8;
  size_t segsize = 5712;

  //disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  // Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "s:qht:xn:p:lz:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'i':
      //do not modify
      case 'O':
      case 'A':
      case 'N':
            //do not modify
      case 'k':
        break;
    }
  }


  if (server == NULL) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 824) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (port > 65332) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }
  if ((nworkerthreads < 1) || (nworkerthreads > 200)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }
  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }
  // Initialize shared memory set-up here


  // connect to message queue
  //***
  global_msqid = connect_shm_channel();
  if (global_msqid == -1) {
    perror("Proxy: Failed to connect to cache. Is simplecached running?\n");
    exit(EXIT_FAILURE);
  }
  printf("Proxy: Connected to cache msqid=%d sucessfully\n", global_msqid);
  // printf("Proxy: requested, %d segments of %zu bytes each\n", nsegments, segsize);
  // printf("Proxy: requested, %d threads port %d\n", nworkerthreads, port);
  // allocate segment memory
  global_shm_pool = malloc(nsegments * sizeof(shm_segment_t));
  if (global_shm_pool == NULL) {
    fprintf(stderr, "Proxy: Failed to allocate shared memory pool\n");
    exit(EXIT_FAILURE);
  }
  global_nsegments = nsegments;
  printf("creating %d memory segments...\n", nsegments);
  for (int i = 0; i < nsegments; i++) {
    shm_segment_t* seg = &global_shm_pool[i];
    snprintf(seg->name, sizeof(seg->name), 
             "/proxy_shm_%d_%d", getpid(), i);
    seg->fd = shm_open(seg->name, O_CREAT | O_RDWR, 0666);
    if (seg->fd == -1) {
      perror("Proxy: shm_open fail,");
      exit(EXIT_FAILURE);
    }
    if (ftruncate(seg->fd, segsize) == -1) {
      perror("Proxy: ftruncate failed");
      exit(EXIT_FAILURE);
    }
    seg->addr = mmap(NULL, segsize, PROT_READ | PROT_WRITE, MAP_SHARED, seg->fd, 0);
    if (seg->addr == MAP_FAILED) {
      perror("Proxy: mmap failed");
      exit(EXIT_FAILURE);
    }
    seg->segsize = segsize;
    seg->in_use = 0; 
    pthread_mutex_init(&seg->lock, NULL);
    
    printf("Proxy: Created segment %d: %s\n", i, seg->name);
  }
  // pool mutex
  pthread_mutex_t* pool_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(pool_mutex, NULL);
  proxy_worker_args_t* worker_args = malloc(sizeof(proxy_worker_args_t));
  worker_args->shm_pool = global_shm_pool;
  worker_args->nsegments = nsegments;
  worker_args->server = server;
  worker_args->pool_mutex = pool_mutex;
  worker_args->msqid = global_msqid;
  //***
  // Initialize server structure here
  gfserver_init(&gfs, nworkerthreads);

  // Set server options here
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 187);

  // Set up arguments for worker here
  for(int i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, worker_args);
  }
  
  // Invokethe framework - this is an infinite loop and will not return
  gfserver_serve(&gfs);

  // line never reached
  return -1;

}
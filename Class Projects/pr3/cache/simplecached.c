#include <stdio.h>
#include <unistd.h>
#include <printf.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"

// CACHE_FAILURE
#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif 

#define MAX_CACHE_REQUEST_LEN 6112
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 783

unsigned long int cache_delay;

/* Global variables */
static int global_msqid = -1;
static steque_t task_queue;
static sem_t queue_semaphore;
static pthread_mutex_t queue_mutex;
static int flag = 0;
static int nthreads = 0;

static void _sig_handler(int signo){
    if (signo == SIGTERM || signo == SIGINT){
        printf("\nReceived signal %d cleaning up...\n", signo);
        flag = 1;
        cleanup_smh_channel();  
    for (int i = 0; i < nthreads; i++) {
            sem_post(&queue_semaphore); 
        }
        exit(signo);
    }
}

void* boss_thread(void* arg) {
    request_t request;
    printf("Boss thread: Thread started\n");
    while (!flag) {
        printf("Boss threading standby msqid=%d...\n", global_msqid);
        ssize_t result = msgrcv(global_msqid, &request, sizeof(request) - sizeof(long), 0,0);
        if (result == -1) {
            if (errno == EINTR) {
                printf("Boss thread: Interrupted by signal\n");
                continue;
            }
            perror("Boss: msgrcv didn't get anything");
            continue;
        }
        
        printf("Boss thread: received request for the path: %s from thread_id: %lu \n", 
               request.filepath, (unsigned long)request.proxy_thread_id);
        printf("Boss thread: Shared memory: %s, segsize: %zu\n", request.name, request.segsize);
    
        task_item_t* newTask = malloc(sizeof(task_item_t));
        if (newTask == NULL) {
            fprintf(stderr, "Boss: malloc failed\n");
            continue;
        }
        
        strncpy(newTask->filepath, request.filepath, sizeof(newTask->filepath) - 1);
        newTask->filepath[sizeof(newTask->filepath) - 1] = '\0';
        
        strncpy(newTask->name, request.name, sizeof(newTask->name) - 1);
        newTask->name[sizeof(newTask->name) - 1] = '\0';
        
        newTask->segsize = request.segsize;
        newTask->proxy_thread_id = request.proxy_thread_id;
        newTask->offset = request.offset;
        pthread_mutex_lock(&queue_mutex);
        steque_push(&task_queue, newTask);
        pthread_mutex_unlock(&queue_mutex);
        
        printf("Boss thread: Work , signaling worker\n");
        sem_post(&queue_semaphore);
    }
    
    printf("Boss thread: Thread exiting\n");
    return NULL;
}

void* worker_thread(void* arg) {
    int worker_id = *(int*)arg;
    printf("worker %d: Thread started\n", worker_id);
    
    while (!flag) {
        printf("Worker %d: Waiting for work...\n", worker_id);
        sem_wait(&queue_semaphore);
        if (flag) break;
        pthread_mutex_lock(&queue_mutex);
        if (steque_isempty(&task_queue)) {
            pthread_mutex_unlock(&queue_mutex);
            continue;
        }
        task_item_t* work = steque_pop(&task_queue);
        pthread_mutex_unlock(&queue_mutex);
        if (work == NULL) continue;
        
        printf("Worker %d: Processing '%s'\n", worker_id, work->filepath);

        int fd = simplecache_get(work->filepath);
        response_t response;
        response.mtype = work->proxy_thread_id;
        strncpy(response.shm_name, work->name, sizeof(response.shm_name) - 1);
        response.shm_name[sizeof(response.shm_name) - 1] = '\0';
        response.chunk_size = 0;
        response.file_size = 0;
        response.status = -2;
        if (fd == -1) {
            printf("Worker %d: File '%s' NOT FOUND ;c\n", worker_id, work->filepath);
            response.status = -1;
            response.chunk_size = 0;
            response.file_size = 0;
            msgsnd(global_msqid, &response, sizeof(response) - sizeof(long), 0);
            free(work);
            continue;
        }
        int shm_fd = shm_open(work->name, O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("Worker: shm_open failed");
            response.mtype = work->proxy_thread_id;
            response.status = -2;
            response.chunk_size = 0;
            response.file_size = 0;
            msgsnd(global_msqid, &response, sizeof(response) - sizeof(long), 0);
            free(work);
            continue;
        }
        void* shm_addr = mmap(NULL, work->segsize,PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        if (shm_addr == MAP_FAILED) {
            perror("Worker: mmap failed");
            response.mtype = work->proxy_thread_id;
            response.status = -2;
            response.chunk_size = 0;
            response.file_size = 0;
            msgsnd(global_msqid, &response, sizeof(response) - sizeof(long), 0);
            free(work);
            continue;
        }
        struct stat st;
 
        if (fstat(fd, &st) == -1) {
            perror("Worker: fstat failed for some reason");
            response.status = -2;
            response.file_size = 0;
            response.chunk_size = 0;
        } else {
            size_t total_size = st.st_size;
            size_t bytes_remaining = total_size - work->offset;
            size_t chunk_size = (bytes_remaining < work->segsize) ? bytes_remaining : work->segsize;
            ssize_t bytes_read = pread(fd, shm_addr, chunk_size, work->offset);
            if (bytes_read == -1) {
                perror("Worker: pread failed");
                response.status = -2;
                response.file_size = 0;
                response.chunk_size = 0;
            } else {
                response.status = 0;
                response.file_size = total_size;
                response.chunk_size = bytes_read;
                printf("Worker %d: read %zd bytes at offset %zu\n",
                    worker_id, bytes_read, work->offset);
            }
        }
        
       //j        
        if (msgsnd(global_msqid, &response, sizeof(response) - sizeof(long), 0) == -1) {
            perror("Worker: msgsnd failed");
        }
        // cleanup segement
        munmap(shm_addr, work->segsize);
        free(work);
        
        printf("Worker %d: request done\n", worker_id);
    }
    
    printf("Worker %d: thread exiting\n", worker_id);
    return NULL;
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 8, Range is 1-100)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-2500000 (microseconds)\n "	\
"  -h                  Show this help message\n"

//OPTIONS
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	nthreads = 6;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);
	
	while ((option_char = getopt_long(argc, argv, "d:ic:hlt:x", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;				
			case 'h': // help
				Usage();
				exit(0);
				break;    
            case 'c': //cache directory
				cachedir = optarg;
				break;
            case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'o': // do not modify
			case 'a': // experimental
				break;
		}
	}

	if (cache_delay > 2500000) {
		fprintf(stderr, "Cache delay must be less than 2500000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>100) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads must be in between 1-100\n");
		exit(__LINE__);
	}
	
	/* Set up signal handlers */
	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}
	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	printf("init cache from '%s'\n", cachedir);
	simplecache_init(cachedir);
	printf("init message queue...\n");
	global_msqid = init_shm_channel();
	if (global_msqid == -1) {
		fprintf(stderr, "Cache: Failed to initialize message queue\n");
		exit(CACHE_FAILURE);
	}
    printf("Cache: Message queue initialized with msqid=%d\n", global_msqid);
	steque_init(&task_queue);
    pthread_mutex_init(&queue_mutex, NULL);
    sem_init(&queue_semaphore, 0, 0);
	pthread_t boss;
	if (pthread_create(&boss, NULL, boss_thread, NULL) != 0) {
		perror("Failed to create boss thread");
		exit(CACHE_FAILURE);
	}
    pthread_t workers[nthreads];
    int worker_ids[nthreads]; 
    for (int i = 0; i < nthreads; i++) {
        worker_ids[i] = i;
        if (pthread_create(&workers[i], NULL, worker_thread, &worker_ids[i]) != 0) {
			perror("Failed to create worker thread");
			exit(CACHE_FAILURE);
		}
    }
    
    printf("Worker Threads created, + boss thread, waiting for work");
    pthread_join(boss, NULL);
    for (int i = 0; i < nthreads; i++) {
        pthread_join(workers[i], NULL);
    }
    // done remove stuff
    pthread_mutex_destroy(&queue_mutex);
    sem_destroy(&queue_semaphore);
    steque_destroy(&task_queue);
    printf("Cache: Shutdown complete\n");
    return 0;
}
#include <stdlib.h>
#include "gfclient-student.h"
#include <stdbool.h>

#define MAX_THREADS 1024
#define PATH_BUFFER_SIZE 512
#include <stdatomic.h>

// atomic_ulong global_task_id = 1; // start IDs at 1

// unsigned long get_next_task_id() {
//     return atomic_fetch_add(&global_task_id, 1);
// }
#define USAGE                                                             \
  "usage:\n"                                                              \
  "  gfclient_download [options]\n"                                       \
  "options:\n"                                                            \
  "  -h                  Show this help message\n"                        \
  "  -p [server_port]    Server port (Default: 29458)\n"                  \
  "  -t [nthreads]       Number of threads (Default 8 Max: 1024)\n"       \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n" \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"           \
  "  -n [num_requests]   Request download total (Default: 16)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {"nthreads", required_argument, NULL, 't'},
    {"workload", required_argument, NULL, 'w'},
    {"nrequests", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}};

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path) {
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/'))) {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)) {
      if (errno != EEXIST) {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w"))) {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
  FILE *file = (FILE *)arg;
  fwrite(data, 1, data_len, file);
}
// typedef void (*callback_t)(void*);
// typedef struct {
//     unsigned long task_id;
//     callback_t function;
//     void* args;
// } task_t;

// // typedef struct{
// //     pthread_t *threads;
// //     steque_t task_queue;
// //     pthread_mutex_t lock;
// //     pthread_cond_t cond;
// //     int numThreads;
// // } threadpool_t;


// bool isPoison(task_t *task){
//     return task->function == NULL;
// }
// void* worker_thread(void* pool_ptr){
//   // lock the queue
//   threadpool_t* pool = (threadpool_t*)pool_ptr;
//   while (true){
//     // check if there is something in the queue 
//     pthread_mutex_lock(&pool->lock);
//     while(steque_isempty(&pool->task_queue)){
//       // go sleep
//       pthread_cond_wait(&pool->cond,&pool->lock);
//     }
//     // dequeue task
//     task_t* task = (task_t*)steque_pop(&pool->task_queue);
//     pthread_mutex_unlock(&pool->lock);
//     // check for poison pill

//     if (isPoison(task)){
//       printf("worker %lu: recieved poison pill. Exiting...  \n", pthread_self());
//       free(task->args);
//       free(task);
//       break;
//     }

//     task->function(task->args);
//     free(task->args);
//     free(task);
//   }
//   return NULL;
// }
// threadpool_t * create_pool(int numThreads){
//   threadpool_t* pool = malloc(sizeof(threadpool_t));
//   if (!pool){
//     return NULL;
//   }
  
//   pool->numThreads = numThreads;
//   pool->threads = malloc(sizeof(pthread_t)*numThreads);
//   steque_init(&pool->task_queue);
//   pthread_mutex_init(&pool->lock,NULL);
//   pthread_cond_init(&pool->cond, NULL);
//   for (int i=0; i< numThreads; i++){
//     pthread_create(&pool->threads[i],NULL,worker_thread,pool);
//   }
//   printf("created %d worker threads\n", numThreads);
//   return pool;
  
// }

// task_t* create_poison_pill(){
//     task_t* poison = malloc(sizeof(task_t));
//     poison->task_id = -1;
//     poison->function = NULL;
//     poison->args = NULL;
//     return poison;
// }
// bool add_poison_pill(threadpool_t *pool){
//   task_t *pill = create_poison_pill();
//   int didLock = pthread_mutex_lock(&pool->lock);
//   if (didLock != 0) {
//         fprintf(stderr, "Error: failed to acquire lock");
//         free(pill);
//         return false;
//   }
//   steque_item item = (steque_item) pill;
//   steque_enqueue(&pool->task_queue, item);
//   pthread_cond_signal(&pool->cond);
//   int didUnlock = pthread_mutex_unlock(&pool->lock);
//   if (didUnlock != 0) {
//         fprintf(stderr, "Error: failed to acquire mutex lock");
//         free(pill);
//         return false;
//   }
//   return true;
// }
// void addAllPoisonPills(threadpool_t *pool){
//   printf("Adding all poison pills");
//   for (int i=0; i< pool->numThreads; i++){
//       bool addedPoison = add_poison_pill(pool);
//       if (addedPoison == false){
//         printf("Failed to add poison...\n");
//       }
//     }
// }

// bool add_task_to_queue(threadpool_t *pool, callback_t func, void *args){
//   // steque_enqueue()
//   if (pool == NULL || func == NULL) {
//         fprintf(stderr, "Error: invalid arguments to add_task\n");
//         return false;
//     }
//   task_t* newTask = malloc(sizeof(task_t));
//   if (newTask == NULL) {
//         fprintf(stderr, "Error: failed to allocate memory for new task\n");
//         return false;
//     }
//   newTask->args = args;
//   newTask->function = func;
//   newTask->task_id = get_next_task_id();
//   steque_item item = (steque_item) newTask;
//   int didLock = pthread_mutex_lock(&pool->lock);
//   if (didLock != 0) {
//         fprintf(stderr, "Error: failed to acquire lock\n");
//         free(newTask);
//         return false;
//   }
//   steque_enqueue(&pool->task_queue,item);
//   pthread_cond_signal(&pool->cond);  
//   int didUnlock = pthread_mutex_unlock(&pool->lock);
//   if (didUnlock != 0) {
//         fprintf(stderr, "Error: failed to acquire lock");
//         free(newTask);
//         return false;
//   }
//   printf("Added task_id %lu to queue\n", newTask->task_id);
//   return true;
// }


// bool destroy_pool(threadpool_t *pool){
//     //     pthread_t *threads;
//     // steque_t task_queue;
//     // pthread_mutex_t lock;
//     // pthread_cond_t cond;
//     // int numThreads;
    
//     // enqueue all poison pills AFTER all tasks have be enqueued
//     // wait fo all threads to finish first

//     for (int j = 0; j<pool->numThreads; j++){
//       pthread_join(pool->threads[j],NULL);
//     }
//     printf("All workers should have finished\n");
//     printf("Starting to free pool\n");

//     //lock + unlock to destroy queue
//     pthread_mutex_lock(&pool->lock);
//     steque_destroy(&pool->task_queue);
//     pthread_mutex_unlock(&pool->lock);

//     pthread_mutex_destroy(&pool->lock);
//     pthread_cond_destroy(&pool->cond);
//     free(pool->threads);
//     free(pool);
//     return true;
// }
typedef struct {
  char server[512];
  char req_path[PATH_BUFFER_SIZE];
  short port;
} download_metadata_t;
void worker_routine(void* args){
    download_metadata_t* task_metadata = (download_metadata_t*)args;
    
    char local_path[PATH_BUFFER_SIZE];
    FILE *file = NULL;
    gfcrequest_t *gfr = NULL;
    int returncode = 0;

    localPath(task_metadata->req_path, local_path);
    file = openFile(local_path);
    gfr = gfc_create();
    gfc_set_server(&gfr, task_metadata->server);
    gfc_set_port(&gfr, task_metadata->port);
    gfc_set_path(&gfr, task_metadata->req_path);
    gfc_set_writearg(&gfr, file);
    gfc_set_writefunc(&gfr, writecb);

    fprintf(stdout, "Worker %lu: Requesting %s%s\n", 
            pthread_self(), task_metadata->server, task_metadata->req_path);

    if (0 > (returncode = gfc_perform(&gfr))) {
        fprintf(stdout, "Worker %lu: gfc_perform returned error %d\n", 
                pthread_self(), returncode);
        fclose(file);
        if (0 > unlink(local_path)) {
            fprintf(stderr, "warning: unlink failed on %s\n", local_path);
        }
    } else {
        fclose(file);
    }

    if (gfc_get_status(&gfr) != GF_OK) {
        if (0 > unlink(local_path)) {
            fprintf(stderr, "warning: unlink failed on %s\n", local_path);
        }
    }

    fprintf(stdout, "Worker %lu: Status: %s\n", 
            pthread_self(), gfc_strstatus(gfc_get_status(&gfr)));
    fprintf(stdout, "Worker %lu: Received %zu of %zu bytes\n", 
            pthread_self(), gfc_get_bytesreceived(&gfr), gfc_get_filelen(&gfr));

    gfc_cleanup(&gfr);
}
/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  char *workload_path = "workload.txt";
  char *server = "localhost";
  unsigned short port = 29458;
  int option_char = 0;
  int nthreads = 2;

  int returncode = 0;
  char *req_path = NULL;
  // char local_path[PATH_BUFFER_SIZE];
  int nrequests = 3;

  // gfcrequest_t *gfr = NULL;
  // FILE *file = NULL;

  setbuf(stdout, NULL);  // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {

      case 'w':  // workload-path
        workload_path = optarg;
        break;
      case 's':  // server
        server = optarg;
        break;
      case 'r': // nrequests
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 'p':  // port
        port = atoi(optarg);
        break;
      case 't':  // nthreads
        nthreads = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);


      case 'h':  // help
        Usage();
        exit(0);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)) {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(EXIT_FAILURE);
  }
  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "Invalid amount of threads\n");
    exit(EXIT_FAILURE);
  }
  gfc_global_init();
  // add your threadpool creation here
  threadpool_t* pool = create_pool(nthreads);
  if (!pool) {
        fprintf(stderr, "Failed to create threadpool\n");
        exit(EXIT_FAILURE);
    }
  /* Build your queue of requests here */
  for (int i = 0; i < nrequests; i++) {
    /* Note that when you have a worker thread pool, you will need to move this
     * logic into the worker threads */
    req_path = workload_get_path();

    if (strlen(req_path) > PATH_BUFFER_SIZE) {
      fprintf(stderr, "Request path exceeded maximum of %d characters\n.", PATH_BUFFER_SIZE);
      exit(EXIT_FAILURE);
    }

    download_metadata_t* task = malloc(sizeof(download_metadata_t));
    strncpy(task->server, server, sizeof(task->server) - 1);
    task->server[sizeof(task->server) - 1] = '\0';
    strncpy(task->req_path, req_path, sizeof(task->req_path) - 1);
    task->req_path[sizeof(task->req_path) - 1] = '\0';
    task->port = port;

    add_task_to_queue(pool, worker_routine, task);

    /*
     * note that when you move the above logic into your worker thread, you will
     * need to coordinate with the boss thread here to effect a clean shutdown.
     */


    // }
  }
  addAllPoisonPills(pool);
  destroy_pool(pool);

  printf("All downloads completed!\n");
  gfc_global_cleanup(); 

  return returncode;
}
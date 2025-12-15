/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */

#include "gf-student.h"


atomic_ulong global_task_id = 1; // start IDs at 1

unsigned long get_next_task_id() {
    return atomic_fetch_add(&global_task_id, 1);
}


// typedef struct handler_item
// {
//   gfcontext_t *ctx;
//   const char *path;
// } handler_item;


typedef void (*callback_t)(void*);

bool isPoison(task_t *task){
    return task->function == NULL;
}
void* worker_thread(void* pool_ptr){
  // lock the queue
  threadpool_t* pool = (threadpool_t*)pool_ptr;
  while (true){
    // check if there is something in the queue 
    pthread_mutex_lock(&pool->lock);
    while(steque_isempty(&pool->task_queue)){
      // go sleep
      pthread_cond_wait(&pool->cond,&pool->lock);
    }
    // dequeue task
    task_t* task = (task_t*)steque_pop(&pool->task_queue);
    pthread_mutex_unlock(&pool->lock);
    // check for poison pill

    if (isPoison(task)){
      printf("worker %lu: recieved poison pill. Exiting...  \n", pthread_self());
      free(task->args);
      free(task);
      break;
    }

    task->function(task->args);
    free(task->args);
    free(task);
  }
  return NULL;
}
threadpool_t * create_pool(int numThreads){
  threadpool_t* pool = malloc(sizeof(threadpool_t));
  if (!pool){
    return NULL;
  }
  
  pool->numThreads = numThreads;
  pool->threads = malloc(sizeof(pthread_t)*numThreads);
  steque_init(&pool->task_queue);
  pthread_mutex_init(&pool->lock,NULL);
  pthread_cond_init(&pool->cond, NULL);
  for (int i=0; i< numThreads; i++){
    pthread_create(&pool->threads[i],NULL,worker_thread,pool);
  }
  printf("created %d worker threads\n", numThreads);
  return pool;
  
}

task_t* create_poison_pill(){
    task_t* poison = malloc(sizeof(task_t));
    poison->task_id = -1;
    poison->function = NULL;
    poison->args = NULL;
    return poison;
}
bool add_poison_pill(threadpool_t *pool){
  task_t *pill = create_poison_pill();
  int didLock = pthread_mutex_lock(&pool->lock);
  if (didLock != 0) {
        fprintf(stderr, "Error: failed to acquire lock");
        free(pill);
        return false;
  }
  steque_item item = (steque_item) pill;
  steque_enqueue(&pool->task_queue, item);
  pthread_cond_signal(&pool->cond);
  int didUnlock = pthread_mutex_unlock(&pool->lock);
  if (didUnlock != 0) {
        fprintf(stderr, "Error: failed to acquire mutex lock");
        free(pill);
        return false;
  }
  return true;
}
void addAllPoisonPills(threadpool_t *pool){
  printf("Adding all poison pills");
  for (int i=0; i< pool->numThreads; i++){
      bool addedPoison = add_poison_pill(pool);
      if (addedPoison == false){
        printf("Failed to add poison...\n");
      }
    }
}

bool add_task_to_queue(threadpool_t *pool, callback_t func, void *args){
  // steque_enqueue()
  if (pool == NULL || func == NULL) {
        fprintf(stderr, "Error: invalid arguments to add_task\n");
        return false;
    }
  task_t* newTask = malloc(sizeof(task_t));
  if (newTask == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for new task\n");
        return false;
    }
  newTask->args = args;
  newTask->function = func;
  newTask->task_id = get_next_task_id();
  steque_item item = (steque_item) newTask;
  int didLock = pthread_mutex_lock(&pool->lock);
  if (didLock != 0) {
        fprintf(stderr, "Error: failed to acquire lock\n");
        free(newTask);
        return false;
  }
  steque_enqueue(&pool->task_queue,item);
  pthread_cond_signal(&pool->cond);  
  int didUnlock = pthread_mutex_unlock(&pool->lock);
  if (didUnlock != 0) {
        fprintf(stderr, "Error: failed to acquire lock");
        free(newTask);
        return false;
  }
  printf("Added task_id %lu to queue\n", newTask->task_id);
  return true;
}


bool destroy_pool(threadpool_t *pool){
    //     pthread_t *threads;
    // steque_t task_queue;
    // pthread_mutex_t lock;
    // pthread_cond_t cond;
    // int numThreads;
    
    // enqueue all poison pills AFTER all tasks have be enqueued
    // wait fo all threads to finish first

    for (int j = 0; j<pool->numThreads; j++){
      pthread_join(pool->threads[j],NULL);
    }
    printf("All workers should have finished\n");
    printf("Starting to free pool\n");

    //lock + unlock to destroy queue
    pthread_mutex_lock(&pool->lock);
    steque_destroy(&pool->task_queue);
    pthread_mutex_unlock(&pool->lock);

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool->threads);
    free(pool);
    return true;
}

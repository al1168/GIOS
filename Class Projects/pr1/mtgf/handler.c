#include "gfserver-student.h"
#include "gfserver.h"
#include "workload.h"
#include "content.h"
#include <stdlib.h>
extern threadpool_t *pool;
#define BUFSIZE 512
typedef struct {
  int task_id;
  gfcontext_t *ctx;
  char *path;
  bool *isRequestDone;
//   pthread_mutex_t itemlock;
//   pthread_cond_t cond;
} handler_item;
//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//

bool add_handleitem_to_queue(threadpool_t *pool, callback_t func, void *args){
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


void worker_routine(void* args){
	handler_item *item = (handler_item*)args;
	int fd = content_get(item->path);
	struct stat file_stat;
	// gfs_sendheader(&item->ctx, GF_FILE_NOT_FOUND, -1);
    if (fd < 0) {
        gfs_sendheader(&item->ctx, GF_FILE_NOT_FOUND, -1);
        gfs_abort(&item->ctx);
        return;
    }
	// Get file size 
	char buffer[BUFSIZE];
	if (fstat(fd, &file_stat) < 0){
		fprintf(stderr, "failed to get file state");
	} 
	off_t fileSize = file_stat.st_size;
	// printf("filesize: %ld\n",(long)fileSize);
	if (fileSize < 0){
		gfs_sendheader(&item->ctx, GF_ERROR, fileSize);
	}
    // open file
	size_t current_sent = 0;
	gfs_sendheader(&item->ctx,GF_OK, (size_t)fileSize);
	int bytes_read;
	
	while (current_sent < fileSize){
		// bytes_read = pread()
		bytes_read = pread(fd, buffer, BUFSIZE, (off_t)current_sent);
		if (bytes_read <=0){
			break;
		}
		ssize_t numBytesSent = gfs_send(&item->ctx, buffer, bytes_read);
		current_sent += numBytesSent;
		// printf("numBytesSent: %zu\n", numBytesSent);
		// printf("current_sent: %zu\n", current_sent);
	}
	printf("done with this task %d\n", item->task_id);
	// free(item->path);
	// free(item->ctx);
	// free(item);

}

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
	// not yet implemented.
	//pool->task_queue
	// ctx, path
	// worker func
	handler_item *item = malloc(sizeof(handler_item));
	item->task_id = get_next_task_id();
	item->ctx = *ctx;
	*ctx = NULL;
	item->path = strdup(path);
	bool isRequestDone = false;
	item->isRequestDone = &isRequestDone;
	// pthread_mutex_init(&item->itemlock, NULL);
	// pthread_cond_init(&item->cond,NULL);
	if (add_handleitem_to_queue(pool,worker_routine,item) != true){
		// free(item->path);
		// free(item);
		return gfh_failure;
	}
	return gfh_success;
	
}


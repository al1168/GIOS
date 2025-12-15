/*
 You can use this however you want.
 */
 #ifndef __CACHE_STUDENT_H__844
 
 #define __CACHE_STUDENT_H__844

 #include "steque.h"

typedef struct {
    char name[256];           
    int fd;                  
    int in_use;                 
    void* addr;              
    size_t segsize;            
    pthread_mutex_t lock;        
} shm_segment_t;

typedef struct {
    char* server;
    shm_segment_t* shm_pool;
    int nsegments;
    pthread_mutex_t* pool_mutex;
    int msqid;
} proxy_worker_args_t;
typedef struct {
    char filepath[256];     
    char name[256];         
    size_t segsize;         
    pthread_t proxy_thread_id;
    size_t offset; 
} task_item_t;
 #endif // __CACHE_STUDENT_H__844
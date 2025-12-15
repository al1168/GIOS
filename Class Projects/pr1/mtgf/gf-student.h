/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */
#ifndef __GF_STUDENT_H__
#define __GF_STUDENT_H__

#include <errno.h>
#include <stdio.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include "steque.h"
#include <stdlib.h>
#include "steque.h"
// typedef struct threadpool_t threadpool_t;
// atomic_ulong global_task_id = 1; // start IDs at 1

typedef void (*callback_t)(void *);
unsigned long get_next_task_id();
typedef struct {
    unsigned long task_id;
    callback_t function;
    void* args;

} task_t;
typedef struct threadpool_t{
    pthread_t *threads;
    steque_t task_queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int numThreads;
} threadpool_t;
threadpool_t * create_pool(int numThreads);
void addAllPoisonPills(threadpool_t *pool);
bool destroy_pool(threadpool_t *pool);
void* worker_thread(void* pool_ptr);
task_t* create_poison_pill();
bool add_poison_pill(threadpool_t *pool);
bool isPoison(task_t *task);
bool add_task_to_queue(threadpool_t *pool, callback_t func, void *args);
 #endif // __GF_STUDENT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "gfserver.h"
#include "cache-student.h"
#include "shm_channel.h"

ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg) {
    proxy_worker_args_t* worker_args = (proxy_worker_args_t*)arg;
    shm_segment_t* segment = NULL;
    int seg_idx = -1;
    ssize_t bytes_transferred = 0;
    printf("Proxy worker %lu: Handling request for '%s'\n", pthread_self(), path);
    pthread_mutex_lock(worker_args->pool_mutex);
    // find free segment
    for (int i = 0; i < worker_args->nsegments; i++) {
        if (!worker_args->shm_pool[i].in_use) {
            worker_args->shm_pool[i].in_use = 1;
            seg_idx = i;
            segment = &worker_args->shm_pool[i];
            break;
        }
    }
    pthread_mutex_unlock(worker_args->pool_mutex);
    if (segment == NULL) {
        fprintf(stderr, "Proxy worker %lu: No free segments available\n", pthread_self());
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }
    printf("proxy worker %lu: segment %d (%s)\n", pthread_self(), seg_idx, segment->name);
    request_t request;
    request.mtype = 1;  
    strncpy(request.filepath, path, sizeof(request.filepath) - 1);
    request.filepath[sizeof(request.filepath) - 1] = '\0';
    strncpy(request.name, segment->name, sizeof(request.name) - 1);
    request.name[sizeof(request.name) - 1] = '\0';
    request.segsize = segment->segsize;
    request.proxy_thread_id = pthread_self();  
    request.offset = 0;
    if (msgsnd(worker_args->msqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
        perror("Proxy: msgsnd failed");
        segment->in_use = 0;  
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }
    
    printf("Proxy worker %lu: request sent\n", pthread_self());
    response_t response;
    ssize_t result = msgrcv(worker_args->msqid, &response,  sizeof(response) - sizeof(long),pthread_self(), 0);
    
    if (result == -1) {
        perror("Proxy: msgrcv failed");
        segment->in_use = 0;  
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }
    
    printf("Proxy worker %lu: response received, status:%d and size:%zu)\n", pthread_self(), response.status, response.file_size);
    if (response.status == -1) {
        segment->in_use = 0;  
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    if (response.status < 0) {
        segment->in_use = 0;
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }
    if (gfs_sendheader(ctx, GF_OK, response.file_size) < 0) {
        segment->in_use = 0;  
        return -1;
    }
    size_t total_size = response.file_size;
    size_t curr_offset = 0;
    while (curr_offset < total_size) {
        if (curr_offset > 0) {
            printf("Proxy worker %lu: requesting offset %zu\n", pthread_self(), curr_offset);
            request.offset = curr_offset;
            if (msgsnd(worker_args->msqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
                perror("Proxy: msgsnd failed for chunk");
                segment->in_use = 0;
                return -1;
            }
            result = msgrcv(worker_args->msqid, &response, sizeof(response) - sizeof(long),pthread_self(), 0);
            if (result == -1) {
                perror("Proxy: msgrcv fail chunk");
                segment->in_use = 0;
                return -1;
            }
            if (response.status != 0) {
                fprintf(stderr, "Proxy worker %lu: error receiving chunk\n", pthread_self());
                segment->in_use = 0;
                return -1;
            }
            printf("Proxy worker %lu: received chunk (size=%zu)\n", pthread_self(), response.chunk_size);
        }
        size_t chunk_size = response.chunk_size;
        if (chunk_size == 0) {
            break;
        }
        char* data_ptr = (char*)segment->addr;
        ssize_t sent = gfs_send(ctx, data_ptr, chunk_size);
        if (sent != chunk_size) {
            segment->in_use = 0;
            return -1;
        }
        bytes_transferred += sent;
        curr_offset += sent;
    }
    printf("Proxy worker %lu: data transfer done - %zd bytes\n", pthread_self(), bytes_transferred);
    segment->in_use = 0;
    return bytes_transferred;
}
// In case you want to implement the shared memory IPC as a library
// You may use this file. It is optional. It does help with code reuse
//
#ifndef __SHM_CHANNEL_H__
#define __SHM_CHANNEL_H__

#define NAME_SIZE 256
#define FILE_PATH_SIZE 256
#include <sys/types.h>
typedef struct{
    long mtype;
    char name[NAME_SIZE];
    char filepath[FILE_PATH_SIZE];
    pthread_t proxy_thread_id;
    size_t segsize;
    size_t offset;
} request_t;

typedef struct {
    long mtype;
    size_t file_size;
    size_t chunk_size;
    char shm_name[NAME_SIZE];
    int status;
} response_t;

int init_shm_channel(void);
int connect_shm_channel(void);
void cleanup_smh_channel(void);
#endif

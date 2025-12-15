// In case you want to implement the shared memory IPC as a library
// This is optional but may help with code reuse
//
#include "shm_channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <unistd.h> 
#include <errno.h>
int global_mq_id = -1;

/// @brief create new message queue channel
/// @param  // void
/// @return // return the message queue id 
int init_shm_channel(void){
    printf("starting shared memory channel\n");
    key_t key;
    int message_queue_id;
    key = ftok("/tmp",14);
    if (key == -1){
        perror("failed to creating key\n");
        return -1;
    }
    message_queue_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);

    if (message_queue_id == -1){
        // message queue already exists
        perror("init msgq failed:");
        return -1;
    }
    global_mq_id = message_queue_id;
    printf("created message queue with id: %d\n ", message_queue_id);
    return message_queue_id;
}

int connect_shm_channel(void){
    printf("connecting message queue\n");
    key_t key;
    int mqid;
    int curr_retry_attempt = 0;
    int MAX_RETRIES = 20;
    key = ftok("/tmp",14);
    if (key == -1){
        perror("failed to creating key\n");
        return -1;
    }

    while (curr_retry_attempt < MAX_RETRIES){
        mqid = msgget(key, 0666);
        if (mqid != -1){
            return mqid;
        }
        printf("retryin...\n");
        sleep(1);
        curr_retry_attempt++;
    }
    perror("Just failed to connect\n");
    return -1;
}
void cleanup_smh_channel(void){
    printf("cleaningup to shared memory channel id:%d\n",global_mq_id);

    if (global_mq_id == -1) {
        printf("Nothing to cleanup\n");
        return;
    }
    
    int isCleanedup = msgctl(global_mq_id, IPC_RMID, NULL);
    if(isCleanedup == -1){
        perror("something wrong with cleanup\n");
    }
    else {
        printf("MQ all cleanedup! msqid=%d\n", global_mq_id);
    }
    global_mq_id = -1;

}
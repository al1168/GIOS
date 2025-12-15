#include "proxy-student.h"
#include "gfserver.h"
#include <curl/curl.h>



#define MAX_REQUEST_N 512
#define BUFSIZE (6426)
#define OK 200
#define NOTFOUND 404
struct memoryStruct{
	char *response;
	size_t size;
};
////https://www.hackthissite.org/articles/read/1078
//https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
size_t write_callback(char *data, size_t size, size_t nmemb, void *userdata){
	size_t realsize = size * nmemb;
	struct memoryStruct *mem = (struct memoryStruct*)userdata;
	char * ptr = realloc(mem->response, mem->size + realsize+1);
	if (!ptr)return 0;
	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data,realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;
	return realsize;
}
ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
    char* fileserver = (char*)arg;
    // char* fileserver =  https://raw.githubusercontent.com/gt-cs6200/image_data;
    char fullpath[9999];
    strcpy(fullpath, fileserver);
    strcat(fullpath, path);
    struct memoryStruct *chunk = malloc(sizeof(struct memoryStruct));
    chunk->response = malloc(1); 
    chunk->size = 0;   
    
    CURL *myHandle;
    CURLcode result;
    size_t statusCode = 0;
    
    myHandle = curl_easy_init();
    curl_easy_setopt(myHandle, CURLOPT_URL, fullpath);
    curl_easy_setopt(myHandle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(myHandle, CURLOPT_WRITEDATA, (void *)chunk);
    
    result = curl_easy_perform(myHandle);
    ssize_t total_sent = 0;
    
    if (result == CURLE_OK){
        curl_easy_getinfo(myHandle, CURLINFO_RESPONSE_CODE, &statusCode);
        
        if(statusCode == OK){
            printf("It was okay\n");
        
            total_sent = gfs_sendheader(ctx, GF_OK, chunk->size);
            
            size_t bytes_sent = 0;
            while(bytes_sent < chunk->size){
                ssize_t bytes = gfs_send(ctx, chunk->response + bytes_sent, chunk->size - bytes_sent);
                if (bytes <= 0) break;
                bytes_sent += bytes;
            }
            total_sent += bytes_sent;
        }
        else if (statusCode == NOTFOUND){
            printf("It was not found------\n");
            total_sent = gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }
        else{
            printf("Error occurred\n");
            total_sent = gfs_sendheader(ctx, GF_ERROR, 0);
        }
        printf("response code:%ld\n", statusCode);
    }
    
    free(chunk->response); 
    free(chunk);             
    curl_easy_cleanup(myHandle);
    curl_global_cleanup();
    
    return total_sent;	
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl as a convenience for linking!
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}
/*

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
 
CURLcode curl_easy_setopt(CURL *handle, CURLOPT_WRITEFUNCTION, write_callback);

*/



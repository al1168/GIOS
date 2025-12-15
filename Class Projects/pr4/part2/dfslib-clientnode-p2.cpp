#include <regex>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>
#include <dirent.h>
#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

extern dfs_log_level_e DFS_LOG_LEVEL;

using dfs_service::StoreRequest;
using dfs_service::StoreResponse;
using dfs_service::StatRequest;
using dfs_service::StatResponse;
using dfs_service::FetchRequest;
using dfs_service::FetchResponse;
using dfs_service::DeleteRequest;
using dfs_service::DeleteResponse;
using dfs_service::ListRequest;
using dfs_service::ListResponse;
using dfs_service::WriteLockRequest;
using dfs_service::WriteLockResponse;
using dfs_service::FileRequest;
using dfs_service::FileList;
using dfs_service::FileInfo;
//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
//
using FileRequestType = dfs_service::FileRequest;
using FileListResponseType = dfs_service::FileList;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}

// std::string DFSClientNodeP2::WrapPath(const std::string &filepath) {
//     return this->mount_path + filepath;
// }

std::string DFSClientNodeP2::WrapPath(const std::string &filepath) {
    std::string temp_path = this->mount_path;
    if (temp_path.empty() || temp_path.back() != '/') {
        temp_path += "/";
    }
    return temp_path + filepath;
}

grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    WriteLockRequest request;
    request.set_filename(filename);
    request.set_client_id(ClientId());

    WriteLockResponse response;
    Status status = service_stub->RequestWriteLock(&ctx, request, &response);

    if (!status.ok()) {
        if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
            return StatusCode::DEADLINE_EXCEEDED;
        }
        return StatusCode::CANCELLED;
    }

    if (!response.granted()) {
        return StatusCode::RESOURCE_EXHAUSTED;
    }
    
    return StatusCode::OK;
}

grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    std::string client_path = WrapPath(filename);
    if (access(client_path.c_str(), F_OK) == -1) {
        std::cerr << "File not found locally: " << client_path << std::endl;
        return StatusCode::CANCELLED; 
    }
    
    
    ClientContext stat_ctx;
    StatRequest stat_req;
    stat_req.set_filename(filename);
    StatResponse stat_resp;
    stat_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    Status stat_status = service_stub->Stat(&stat_ctx, stat_req, &stat_resp);
    if (stat_status.ok()) {
        uint32_t server_crc = stat_resp.file_info().crc();
        uint32_t client_crc = dfs_file_checksum(client_path, &this->crc_table);
        if (client_crc == server_crc) {
            return StatusCode::ALREADY_EXISTS;
        }
        // else we are good to go 
    }

    StatusCode lock_status = RequestWriteAccess(filename);
    if (lock_status != StatusCode::OK) {
        return lock_status;
    }
    std::ifstream file{WrapPath(filename), std::ios::in | std::ios::binary};
    if (!file.is_open()) {
        std::cout << "This file was not found" << std::endl;
        return StatusCode::NOT_FOUND;
    }
    ClientContext ctx; 
    ctx.AddMetadata("filename", filename);
    ctx.AddMetadata("client_id", ClientId());
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    

    StoreResponse response;
    std::unique_ptr<grpc::ClientWriter<dfs_service::StoreRequest>> writer{service_stub->Store(&ctx, &response)};
    
    const size_t bufsize = 4096; // general efficient size for a buffer cus multiple of the file system's block size 
    char buffer[bufsize];

    while (!file.eof()){
        file.read(buffer, bufsize);
        std::streamsize numBytesRead = file.gcount();
        if (numBytesRead > 0){
            StoreRequest request;
            request.set_data(buffer, numBytesRead);

            if (!writer->Write(request)){
                break;
            }
        }
    }
    file.close();
    writer->WritesDone();
    Status status = writer->Finish();
    if (status.ok()) {
        return StatusCode::OK;
    } else if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == StatusCode::RESOURCE_EXHAUSTED) {
        return StatusCode::RESOURCE_EXHAUSTED;
    }
    return StatusCode::CANCELLED;
}


grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //
    // if (access(client_path.c_str(), F_OK) == -1) {
    //     std::cerr << "File not found locally: " << client_path << std::endl;
    //     return StatusCode::NOT_FOUND; 
    // }
    StatRequest stat_req;
    stat_req.set_filename(filename);
    StatResponse stat_resp;
    ClientContext stat_ctx;
    Status stat_status = service_stub->Stat(&stat_ctx, stat_req, &stat_resp);
    stat_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    if (!stat_status.ok()) {
        if (stat_status.error_code() == StatusCode::NOT_FOUND) return StatusCode::NOT_FOUND;
        if (stat_status.error_code() == StatusCode::DEADLINE_EXCEEDED) return StatusCode::DEADLINE_EXCEEDED;
        return StatusCode::CANCELLED;
    }


    std::string path = WrapPath(filename);

    if (access(path.c_str(), F_OK) != -1) {
        uint32_t client_crc = dfs_file_checksum(path, &this->crc_table);
        uint32_t server_crc = stat_resp.file_info().crc();

        if (client_crc == server_crc) {
            return StatusCode::ALREADY_EXISTS;
        }
    }
    ClientContext ctx;
    ctx.set_deadline(
        std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout)
    );
    FetchRequest request;
    request.set_filename(filename);
    
    std::unique_ptr<grpc::ClientReader<FetchResponse>> reader(service_stub->Fetch(&ctx, request));

    std::ofstream outputfile(path, std::ios::out | std::ios::binary | std::ios::trunc);


    if(!outputfile.is_open()){
        return StatusCode::CANCELLED;
    }

    dfs_service::FetchResponse response;

    while(reader->Read(&response)){
        std::string data = response.data();
        outputfile.write(data.c_str(), data.length());
    }
    outputfile.close();

    Status status = reader->Finish();
    if (status.ok()){
        struct utimbuf filetime;
        filetime.actime = stat_resp.file_info().mtime();  // Access time
        filetime.modtime = stat_resp.file_info().mtime(); // Modification time
        utime(path.c_str(), &filetime);
        return StatusCode::OK;
    }
    else{
        remove(path.c_str());
        if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
            return StatusCode::DEADLINE_EXCEEDED;
        }
        else if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
            return StatusCode::NOT_FOUND;
        }
        return StatusCode::CANCELLED;
    }

}

grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

     if (filename.empty()) {
        return StatusCode::NOT_FOUND;
    }
    ClientContext ctx;
    ctx.set_deadline(
        std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout)
    );
    dfs_service::DeleteRequest request;
    dfs_service::DeleteResponse response;
    request.set_filename(filename);

    // dfs_service::De response;
    Status rpcCallStatus = service_stub->Delete(&ctx,request,&response);
    if (!rpcCallStatus.ok()){
        StatusCode errorStatus = rpcCallStatus.error_code();
        if(errorStatus == StatusCode::DEADLINE_EXCEEDED || errorStatus == StatusCode::NOT_FOUND){
            // std::cout << errorStatus << endl;
            std::cout << "RPC Error: " << rpcCallStatus.error_message() << std::endl;
            return errorStatus;
        }
        return StatusCode::CANCELLED;
    }
    return StatusCode::OK;

}

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
     ListRequest request;
    ListResponse response;
    ClientContext ctx;
    ctx.set_deadline(
        std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout)
    );
    Status rpcCallStatus = service_stub->List(&ctx,request,&response);
    if (!rpcCallStatus.ok()){
        StatusCode errorStatus = rpcCallStatus.error_code();
        if(errorStatus == StatusCode::DEADLINE_EXCEEDED){
            // std::cout << errorStatus << endl;
            std::cout << "RPC Error: " << rpcCallStatus.error_message() << std::endl;
            return errorStatus;
        }
        return StatusCode::CANCELLED;
    }
    // iterate through the retrived files
    for (const auto& file : response.files()){
        (*file_map)[file.filename()] = (int)file.mtime();
        if (display){
            std::cout << "File: " << file.filename() << ", modified time:: " << file.mtime() << std::endl;
        }
    }
    std::cout << "-----end--------------------------------------------------------------------------------" << std::endl;
    return StatusCode::OK;
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    // 1. Setup Context and Deadline
    ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    StatRequest request;
    request.set_filename(filename);

    StatResponse* response = static_cast<StatResponse*>(file_status);
    
    Status status = service_stub->Stat(&ctx, request, response);
    if (status.ok()) {
        return StatusCode::OK;
    } 
    else {
        if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
            return StatusCode::DEADLINE_EXCEEDED;
        }
        else if (status.error_code() == StatusCode::NOT_FOUND) {
            return StatusCode::NOT_FOUND;
        }
        return StatusCode::CANCELLED;
    }
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    std::lock_guard<std::mutex> lock(inotify_mutex);
    callback();

}

//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//
void DFSClientNodeP2::HandleCallbackList() {

    void* tag;

    bool ok = false;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your file list synchronization code here.
    //
    // When the server responds to an asynchronous request for the CallbackList,
    // this method is called. You should then synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the async thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //

    // Block until the next result is available in the completion queue.
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //
            std::lock_guard<std::mutex> lock(inotify_mutex);
            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG3) << "Handling async callback ";

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //


                for (const auto& server_file : call_data->reply.file_info()) {
                    std::string filename = server_file.filename();
                    std::string local_path = WrapPath(filename);
                    
                    struct stat local_stat;
                    bool local_exists = (stat(local_path.c_str(), &local_stat) == 0);

                    if (!local_exists) {
                        dfs_log(LL_DEBUG2) << "New file on server, fetching: " << filename;
                        Fetch(filename);
                    } else {
                        uint32_t client_crc = dfs_file_checksum(local_path, &this->crc_table);
                        uint32_t server_crc = server_file.crc();
                        
                        if (client_crc != server_crc) {
                            // Content differs. Apply "Last Modified Wins" strategy
                            time_t local_mtime = local_stat.st_mtime;
                            time_t server_mtime = server_file.mtime();
                            
                            if (server_mtime > local_mtime) {
                                dfs_log(LL_DEBUG2) << "Server version newer, fetching: " << filename;
                                Fetch(filename);
                            } else if (local_mtime > server_mtime) {
                                dfs_log(LL_DEBUG2) << "Local version newer, storing: " << filename;
                                Store(filename);
                            }
                        }
                    }
                }

                DIR* dir = opendir(mount_path.c_str());
                if (dir != nullptr) {
                    struct dirent* entry;
                    while ((entry = readdir(dir)) != nullptr) {
                        std::string local_filename = entry->d_name;
                        if (local_filename == "." || local_filename == "..") continue;
                        
                        bool found_on_server = false;
                        for (const auto& server_file : call_data->reply.file_info()) {
                            if (server_file.filename() == local_filename) {
                                found_on_server = true;
                                break;
                            }
                        }
                        
                        if (!found_on_server) {
                            std::string local_path = WrapPath(local_filename);
                            dfs_log(LL_DEBUG2) << "File deleted on server, removing locally: " << local_filename;
                            remove(local_path.c_str());
                        }
                    }
                    closedir(dir);
                }

            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

            // Once we're complete, deallocate the call_data object.
            delete call_data;

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }


        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();

    }
}

/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    CallbackList<FileRequestType, FileListResponseType>();
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//


#include <map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;
using dfs_service::FileInfo;
using dfs_service::DeleteRequest;
using dfs_service::DeleteResponse;
using dfs_service::ListRequest;
using dfs_service::ListResponse;
using dfs_service::StoreRequest;
using dfs_service::StoreResponse;
using dfs_service::FetchRequest;
using dfs_service::FetchResponse;
using dfs_service::StatRequest;
using dfs_service::StatResponse;
using dfs_service::WriteLockRequest;
using dfs_service::WriteLockResponse;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = dfs_service::FileRequest;
using FileListResponseType = dfs_service::FileList;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;
    
    std::map<std::string, std::string> write_locks;

    CRC::Table<std::uint32_t, 32> crc_table;

    std::mutex lock_mutex;

    std::shared_timed_mutex write_locks_mutex;

    std::mutex fs_mutex;
    // notify stuff

    // std::vector<std::string> pending_changes;
    // std::mutex notify_mutex;
    // std::condition_variable notify_cv;
    

    // void WaitForChanges() {
    // std::unique_lock<std::mutex> lock(notify_mutex);
    // // notify_cv.wait(lock, [&]{ return !pending_changes.empty(); });
    //     while (pending_changes.empty()) {
    //         notify_cv.wait(lock);
    //     }
    //     pending_changes.clear();
    // }
    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    // const std::string WrapPath(const std::string &filepath) {
    //     return this->mount_path + filepath;
    // }
    std::string WrapPath(const std::string &filepath) {
        std::string temp_path = this->mount_path;
        if (temp_path.empty() || temp_path.back() != '/') {
            temp_path += "/";
        }
    return temp_path + filepath;
    }

 

public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //
        
        std::lock_guard<std::mutex> lock(fs_mutex);
        DIR *directory;
        struct dirent *entry;
        if ((directory = opendir(this->mount_path.c_str())) != NULL) {
            while ((entry = readdir(directory)) != NULL) {
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..") {
                continue;
            }
            std::string filepath = WrapPath(filename);
            struct stat filestat;

            if (stat(filepath.c_str(), &filestat) == 0) {
                
                dfs_service::FileInfo* info = response->add_file_info();
                info->set_filename(filename);
                info->set_mtime(static_cast<int64_t>(filestat.st_mtime));
                info->set_size(static_cast<int64_t>(filestat.st_size));
                uint32_t crc = dfs_file_checksum(filepath, &this->crc_table);
                info->set_crc(crc);
            }
        }
        closedir(directory);
        } else {
            std::cerr << "ProcessCallback: couldn't open mount path: " << this->mount_path << std::endl;
        }

    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //

        
        while(true){
            // this->WaitForChanges();
        // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                // std::lock_guard<std::mutex> lock(queue_mutex);
                // std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::lock_guard<std::mutex> lock(queue_mutex);

                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    Status RequestWriteLock(ServerContext* context, const WriteLockRequest* request, WriteLockResponse* response) override {
        std::lock_guard<std::mutex> lock(lock_mutex);
        
        const std::string& filename = request->filename();
        const std::string& client_id = request->client_id();

        auto it = write_locks.find(filename);
        if (it != write_locks.end()) {
            if (it->second == client_id) {
                response->set_granted(true);
            } else {
                response->set_granted(false);
            }
        } else {

            write_locks[filename] = client_id;
            response->set_granted(true);
        }
        return Status::OK;
    }

    grpc::Status Delete(ServerContext* context, const DeleteRequest *request, DeleteResponse* response) override{
        // stat(request.filename)
        struct stat buffer;
        std::string path = WrapPath(request->filename());
        std::string client_id;
        std::string filename = request->filename();
  
        auto metadata = context->client_metadata();
        auto client_iter = metadata.find("client_id");
        if (client_iter != metadata.end()) {
            client_id = std::string(client_iter->second.data(), client_iter->second.length());
        }

        {
            std::lock_guard<std::mutex> lock(lock_mutex);
            auto it = write_locks.find(filename);
            if (it == write_locks.end() || it->second != client_id) {
                return Status(StatusCode::RESOURCE_EXHAUSTED, "Write lock not held");
            }
        }
        std::lock_guard<std::mutex> fs_lock(fs_mutex);
        // check if it is there
        if(stat(path.c_str(), &buffer) != 0){
            response->set_status(StatusCode::NOT_FOUND);
            response->set_message("File not found");
            return Status(StatusCode::NOT_FOUND, "Can't find file");
        }
        // only needed for long running functions on server to save server time 
        // if (context->IsCancelled()) {
        //     return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline passed");
        // }
        bool isRemoved = (remove(path.c_str()) == 0);
        if (isRemoved){
            {
                std::lock_guard<std::mutex> lock(lock_mutex);
                write_locks.erase(filename);
            }
            response->set_status(StatusCode::OK);
            response->set_message("Deleted successfully");
            return Status::OK;
        }
        else {
            char* error = strerror(errno);
            std::string errorMessage = "Failed to delete file: " + path + "| error:" + error;
            response->set_status(StatusCode::INTERNAL);
            response->set_message(errorMessage);
            return Status(StatusCode::INTERNAL, errorMessage);
        }
    }
    
     Status List(ServerContext* context, const ListRequest *request, ListResponse* response) override{
        // get the files and send it as a stream to the client
        std::lock_guard<std::mutex> lock(fs_mutex);
        struct stat fileStat;
        DIR *directory;
        struct dirent * entry;
        directory = opendir(mount_path.c_str());
        
        if (directory == NULL){
            // This either means the directory doesn't exit or just had no files.
            return Status::OK;
        }
        while((entry = readdir(directory))){
            std::string filename = entry->d_name;
            if (filename == "." || filename == ".."){
                continue;
            }
            // std::string path = mount_path + "/" + filename;
            std::string path = WrapPath(filename);
            if(stat(path.c_str(), &fileStat) == 0){
                FileInfo *fileInfoPtr = response->add_files();
                fileInfoPtr->set_filename(filename);
                fileInfoPtr->set_mtime(static_cast<int64_t>(fileStat.st_mtime));

                fileInfoPtr->set_size(static_cast<int64_t>(fileStat.st_size));
                uint32_t crc = dfs_file_checksum(path, &this->crc_table);
                fileInfoPtr->set_crc(crc);
            }
         }
        closedir(directory);
        return Status::OK;
    }

    Status Store(ServerContext* context, ServerReader<StoreRequest>* reader, StoreResponse *response) override{
        std::string filename;
        std::string client_id;
        auto metadata = context->client_metadata();
        auto iter = metadata.find("filename");
        auto client_iter = metadata.find("client_id");

        if (iter != metadata.end()){
            // it found it bc iter is not at the "null term"
            auto fname = iter->second.data();
            auto flength = iter->second.length();
            filename = std::string(fname, flength);

        }
        else{
            // couldn't find filename metadata
            return Status(StatusCode::INVALID_ARGUMENT, "missing metadata 'filename'"); 
        }

        if (client_iter != metadata.end()) {
            client_id = std::string(client_iter->second.data(), client_iter->second.length());
        }

        {
            std::lock_guard<std::mutex> lock(lock_mutex);
            auto it = write_locks.find(filename);
            if (it == write_locks.end() || it->second != client_id) {
                return Status(StatusCode::RESOURCE_EXHAUSTED, "Write lock not held by client");
            }
        }

        std::string filepath = WrapPath(filename);
        // std::lock_guard<std::mutex> lock(file_mutex_);
        //  std::ofstream ostrm(filename, std::ios::binary);
        std::lock_guard<std::mutex> fs_lock(fs_mutex);
        std::ofstream outputFile(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!outputFile.is_open()){
            std::cerr << "Server: Couldn't open the outputFile: " << filepath << strerror(errno) << std::endl;
            response->set_status(StatusCode::INTERNAL);
            response->set_messagetext("Couldn't open the file on server");
            {
                std::lock_guard<std::mutex> lock(lock_mutex);
                write_locks.erase(filename);
            }
            return Status(StatusCode::INTERNAL, "Could not open file");
        }

        StoreRequest request;

        while (reader->Read(&request)){
            const std::string& data_chunk = request.data();
            outputFile.write(data_chunk.c_str(), data_chunk.length());
            if(outputFile.bad()){
                outputFile.close();
                remove(filepath.c_str()); // Delete the half-written corrupt file
                return Status(StatusCode::RESOURCE_EXHAUSTED, "Write failed");
            }
        }
        outputFile.close();
        {
            std::lock_guard<std::mutex> lock(lock_mutex);
            write_locks.erase(filename);
        }
        struct stat result;
        if(stat(filepath.c_str(), &result) == 0) {
            response->set_mtime(static_cast<int64_t>(result.st_mtime));
        } else {
            response->set_mtime(0);
        }
        response->set_filename(filename);
        response->set_status(StatusCode::OK);
        response->set_messagetext("File received and stored!");

        return Status::OK;
    }

    Status Fetch(ServerContext* context, const FetchRequest* request, ServerWriter<FetchResponse>* writer) override{
        std::string filename = request->filename();
        std::string filepath = WrapPath(filename);
        std::lock_guard<std::mutex> lock(fs_mutex);

        std::ifstream inputFile(filepath, std::ios::in | std::ios::binary);
        if(!inputFile.is_open()){
            return Status(StatusCode::NOT_FOUND, "can't find " + filename + " on server");
        }
        const size_t bufsize = 4096;
        char buffer[bufsize];
        dfs_service::FetchResponse response;
        
        while (!inputFile.eof()){
            inputFile.read(buffer, bufsize);
            std::streamsize numBytes = inputFile.gcount();
            if (numBytes > 0) {
                response.set_data(buffer, numBytes);
                if (!writer->Write(response)) {
                break;
                }
            }   
        }
        inputFile.close();
        return Status::OK;
    }
//std::shared_lock<std::shared_timed_mutex> lock(write_locks_mutex);
    Status Stat(ServerContext* context, const StatRequest* request, StatResponse* response) override {
        std::string filename = request->filename();
        std::string filepath = WrapPath(filename);
        std::lock_guard<std::mutex> lock(fs_mutex);
        struct stat fileStat;
        if (stat(filepath.c_str(), &fileStat) != 0) {
            return Status(StatusCode::NOT_FOUND, "File not found");
        }

        FileInfo* file_info = response->mutable_file_info();
        file_info->set_filename(filename);
        file_info->set_mtime(static_cast<int64_t>(fileStat.st_mtime));
        file_info->set_size(static_cast<int64_t>(fileStat.st_size));
        
        uint32_t crc = dfs_file_checksum(filepath, &this->crc_table);
        file_info->set_crc(crc);

        return Status::OK;
    }
};


    

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //




//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//

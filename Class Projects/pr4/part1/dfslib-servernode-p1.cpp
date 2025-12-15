#include <map>
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
#include <stdbool.h>
#include <cstring>

#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;


using dfs_service::DFSService;
using dfs_service::DeleteRequest;
using dfs_service::DeleteResponse;
using dfs_service::StatResponse;
using dfs_service::ListRequest;
using dfs_service::ListResponse;
using dfs_service::FileInfo;
using dfs_service::StoreRequest;
using dfs_service::StoreResponse;
using dfs_service::FetchRequest;
using dfs_service::FetchResponse;
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest
// using dfs_service::DeleteRequest

//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //

    grpc::Status Delete(ServerContext* context, const DeleteRequest *request, DeleteResponse* response) override{
    // stat(request.filename)
    struct stat buffer;
    std::string path = WrapPath(request->filename());
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
// https://www.ibm.com/docs/en/zvm/7.3.0?topic=descriptions-readdir-read-entry-from-directory
//https://en.wikibooks.org/wiki/C_Programming/POSIX_Reference/dirent.h
// https://www.geeksforgeeks.org/cpp/casting-operators-in-cpp/#1-static_cast
    Status List(ServerContext* context, const ListRequest *request, ListResponse* response) override{
        // get the files and send it as a stream to the client
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
            std::string path = mount_path + "/" + filename;
            if(stat(path.c_str(), &fileStat) == 0){
                FileInfo *fileInfoPtr = response->add_files();
                fileInfoPtr->set_filename(filename);
                fileInfoPtr->set_mtime(static_cast<int64_t>(fileStat.st_mtime));
            }
         }
        closedir(directory);
        return Status::OK;
    }

    Status Store(ServerContext* context, ServerReader<StoreRequest>* reader, StoreResponse *response) override{
        std::string filename;
        auto metadata = context->client_metadata();
        auto iter = metadata.find("filename");
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

        std::string filepath = WrapPath(filename);
        // std::lock_guard<std::mutex> lock(file_mutex_);
        //  std::ofstream ostrm(filename, std::ios::binary);
        std::ofstream outputFile(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!outputFile.is_open()){
            std::cerr << "Server: Couldn't open the outputFile: " << filepath << strerror(errno) << std::endl;
            response->set_status(StatusCode::INTERNAL);
            response->set_messagetext("Couldn't open the file on server");
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
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

#include <regex>
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

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using namespace std;
using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;
using dfs_service::DeleteRequest;
using dfs_service::DeleteResponse;
using dfs_service::ListRequest;
using dfs_service::ListResponse;
using dfs_service::StoreRequest;
using dfs_service::StoreResponse;
using dfs_service::FetchRequest;
using dfs_service::FetchResponse;

// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}
//https://grpc.io/docs/languages/cpp/basics/#why-use-grpc
StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the client
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext ctx; 


    std::ifstream file{WrapPath(filename), std::ios::in | std::ios::binary};
    if (!file.is_open()) {
        std::cout << "This shit was not found" << endl;
        return StatusCode::NOT_FOUND;
    }
    ctx.AddMetadata("filename", filename);
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    

    dfs_service::StoreResponse response;
    std::unique_ptr<grpc::ClientWriter<dfs_service::StoreRequest>> writer{service_stub->Store(&ctx, &response)};
    
    const size_t bufsize = 4096; // general efficient size for a buffer cus multiple of the file system's block size 
    char buffer[bufsize];

    while (!file.eof()){
        file.read(buffer, bufsize);
        std::streamsize numBytesRead = file.gcount();
        if (numBytesRead > 0){
            dfs_service::StoreRequest request;
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
    } else {
        if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
            return StatusCode::DEADLINE_EXCEEDED;
        }
          std::cout << "client: smthing happened that isn't a deadline issue" << endl;
        return StatusCode::CANCELLED;
    }
}   


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {
    
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext ctx;
    ctx.set_deadline(
        std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout)
    );
    FetchRequest request;
    request.set_filename(filename);
    
    std::unique_ptr<grpc::ClientReader<FetchResponse>> reader(service_stub->Fetch(&ctx, request));

    std::string path = WrapPath(filename);
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

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    
    //how to make a grpc call to delete a file on the server
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

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {
    // display is just a useless variable

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
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

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {
    return StatusCode::OK;
    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//



#include "/home/tungi/cpp-httplib/httplib.h"
#include <iostream>

int main() {
    httplib::Client cli("http://localhost:80");

    // Getting the manifest file
    auto res = cli.Get("/stream.mpd");
    if(res && res->status == 200) {
        std::cout << "Fetched manifest file: \n";
        // std::cout << res->body << "\n";
    }

    // Getting the initialization segment
    res = cli.Get("/init-stream0.m4s");
    if(res && res->status == 200) {
        std::cout << "Fetched initialization segment. Size: " << res->body.size() << " bytes\n";
    }

    // Getting the first video segment
    res = cli.Get("/chunk-stream0-00001.m4s");
    if(res && res->status == 200) {
        std::cout << "Fetched video segment. Size: " << res->body.size() << " bytes\n";
        // print the header of the received segment
        for (const auto& header : res->headers) {
            std::cout << header.first << ": " << header.second << "\n";
        }
    }
}
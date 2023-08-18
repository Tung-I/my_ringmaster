#include "/home/tungi/cpp-httplib/httplib.h"

int main() {
    httplib::Server svr;

    svr.set_base_dir("/home/tungi/my_ringmaster/mpd");  // Set your base directory to your 'mpd' folder

    svr.listen("localhost", 8080);
}
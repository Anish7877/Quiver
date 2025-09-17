#include "../include/network.hpp"
#include <iostream>
#include <unistd.h>

pid_t Network::net_pid{ -1 };

int Network::setup_networking(const pid_t& pid){
    std::cout << "Setting Network for PID " << pid << '\n';

    net_pid = fork();
    if(net_pid == 0){
        std::string pid_str{ std::to_string(pid) };
        execl("/usr/bin/slirp4netns", "slirp4netns", "-c", "--enable-sandbox", pid_str.c_str(), "tap0", (char*)NULL);
        std::cerr << "exec slirp4nets failed" << '\n';
        exit(1);
    }
    else if(net_pid > 0){
        return 0;
    }
    else{
        std::cerr << "failes to fork slirp4nets process" << '\n';
        return -1;
    }
}
int Network::forward_port(const int& source, const int& destination){
    return 0;
}
Network::~Network(){};

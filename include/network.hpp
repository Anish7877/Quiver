#pragma once

#include <unistd.h>
class Network {
    public:
        Network()=default;
        static int setup_networking(const pid_t& pid);
        static int forward_port(const int& source,const int& destination);
        static pid_t get_net_pid(){ return net_pid; }
        ~Network();
    private:
        static pid_t net_pid;
};

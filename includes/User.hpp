#pragma once

#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
// #include "Client.hpp"
#include "Config.hpp"
#include "Response.hpp"
#include "Logger.hpp"

class User {
    private:
        const unsigned int _user_id;
        const std::string _user_name;
        const std::string _user_pasword;
        unsigned int _times_conected;
        const std::string _user_email;

    public:
        User(unsigned int user_id, std::string user_name, std::string user_pasword, std::string user_email);
        User(unsigned int user_id, std::string user_name, std::string user_pasword);
        ~User();

        //Geters
        unsigned int getId() const;
        unsigned int getTimesConected() const;
        std::string getName() const;
        std::string getPasword() const;
        std::string getEmail() const;

        //Seters
        void incriseTimesConected();

};

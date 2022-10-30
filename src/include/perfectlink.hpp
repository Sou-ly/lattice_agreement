#pragma once

#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <array>
#include "socket.hpp"

namespace da
{
    void talk(std::atomic<bool>&, 
              udp_socket, 
              std::mutex&, 
              std::vector<std::tuple<udp_sockaddr, unsigned long, void*, size_t>>&);
              
    void listen(std::atomic<bool>&,
                udp_socket,
                std::mutex&,
                std::map<udp_sockaddr, std::vector<unsigned long>>&,
                std::vector<std::tuple<udp_sockaddr, unsigned long, void*, size_t>>&,
                std::vector<std::function<void(std::string &, udp_sockaddr &)>>&);

    class perfect_link
    {
    public:
        /**
         * @brief Construct a new Perfect Link object
         *
         * @param socket
         */
        perfect_link(udp_socket socket);

        /**
         * @brief Destroy the Perfect Link object
         *
         */
        ~perfect_link();

        /**
         * @brief
         *
         * @param buf
         * @param dest
         * @return ssize_t
         */
        void send(const std::string &buf, const udp_sockaddr &dest);

        /**
         * @brief
         *
         * @param deliver
         */
        void upon_deliver(std::function<void(std::string &, udp_sockaddr &)> deliver);

    private:
        std::atomic<bool> listening, talking;
        unsigned long id;
        std::vector<std::function<void(std::string &, udp_sockaddr &)>> handlers;
        std::map<udp_sockaddr, std::vector<unsigned long>> delivered;
        std::mutex mutex;
        std::vector<std::tuple<udp_sockaddr, unsigned long, void*, size_t>> sent;
        std::thread sender, listener;
        udp_socket socket;
    };
}
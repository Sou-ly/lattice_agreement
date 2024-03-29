#include "perfectlink.hpp"

namespace da
{
    perfect_link::perfect_link(udp_socket socket, std::vector<address> &peers)
    {
        this->socket = socket;
        this->peers = peers;
        talking = false;
        listening = false;
        delivered = {};
        sn = 0;
    }

    perfect_link::~perfect_link()
    {
        if (talking)
        {
            talking = false;
            sender.join();
        }
        if (listening)
        {
            listening = false;
            listener.join();
        }
    }

    void perfect_link::talk()
    {
        while (talking)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (auto &[dst, mp] : sent)
            {
                std::lock_guard<std::mutex> lock(mutex[dst]);
                for (auto &[id, pair] : mp)
                {
                    socket->write(std::get<0>(pair), std::get<1>(pair), dst);
                }
            }
        }
    }

    void perfect_link::send(const std::string &buf, const address &dest)
    {
        sn++;
        // header
        unsigned char *bytes = reinterpret_cast<unsigned char *>(malloc(5 + buf.size()));
        // ack
        bytes[0] = static_cast<unsigned char>(false);
        // sequence number
        bytes[1] = (sn >> 24) & 0xFF;
        bytes[2] = (sn >> 16) & 0xFF;
        bytes[3] = (sn >> 8) & 0xFF;
        bytes[4] = (sn >> 0) & 0xFF;
        std::memcpy(&bytes[5], buf.data(), buf.size());
        // grab sent lock and push into the queue
        std::lock_guard<std::mutex> lock(mutex[dest]);
        sent[dest][sn] = std::make_pair(bytes, 5 + buf.size());
        if (!talking)
        {
            talking = true;
            sender = std::thread(&perfect_link::talk, this);
        }
    }

    void perfect_link::listen()
    {
        while (listening)
        {
            address src;
            size_t size;
            unsigned char *rec = reinterpret_cast<unsigned char *>(malloc(128));
            if ((size = socket->read(rec, 128, src)) > 0)
            {
                if (std::find(peers.begin(), peers.end(), src) == peers.end())
                {
                    // dirty fix: need to filter source address for some reason,
                    // socket sometimes uses wrong port despite being bound correctly
                    continue;
                }
                bool ack = static_cast<bool>(rec[0]);
                unsigned long id = (rec[1] << 24) | (rec[2] << 16) | (rec[3] << 8) | (rec[4]);
                if (ack)
                {
                    std::lock_guard<std::mutex> lock(mutex[src]);
                    free(std::get<0>(sent[src][id]));
                    sent[src].erase(id);
                }
                else
                {
                    rec[0] = static_cast<unsigned char>(true);
                    socket->write(&rec[0], 5, src);
                    if (std::find(delivered[src].begin(), delivered[src].end(), id) == delivered[src].end())
                    {
                        std::string msg;
                        msg.append(reinterpret_cast<char *>(&rec[5]), size - 5);
                        delivered[src].push_back(id);
                        for (const auto &deliver : handlers)
                        {
                            deliver(msg, src);
                        }
                    }
                }
            }
            free(rec);
        }
    }

    void perfect_link::upon_deliver(std::function<void(std::string &, address &)> deliver)
    {
        handlers.push_back(deliver);
        if (!listening)
        {
            listening = true;
            listener = std::thread(&perfect_link::listen, this);
        }
    }
}
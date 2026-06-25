#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "socket.h"

namespace iris::chat
{
    struct ChatClient
    {
        std::shared_ptr<iris::network::Socket> socket;
        std::string nick;
        std::mutex sendMutex;
    };

    class ChatHub
    {
    public:
        void join(const std::string &room, const std::shared_ptr<ChatClient> &client);
        void leave(const std::string &room, const std::shared_ptr<ChatClient> &client);

        void broadcast(const std::string &room, const std::shared_ptr<ChatClient> &sender, const std::string &text);
        void system_message(const std::string &room, const std::string &text);

    private:
        std::vector<std::shared_ptr<ChatClient>> snapshot(const std::string &room);
        static void send_line(const std::shared_ptr<ChatClient> &client, const std::string &line);

        std::mutex mutex;
        std::unordered_map<std::string, std::vector<std::shared_ptr<ChatClient>>> rooms;
    };
} // namespace iris::chat

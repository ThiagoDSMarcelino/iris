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

    // Thread-safe registry of named rooms. Each connection handler thread owns
    // one ChatClient and shares a single ChatHub to reach the other members.
    class ChatHub
    {
    public:
        // Adds the client to the room, creating the room if it does not exist yet.
        void join(const std::string &room, const std::shared_ptr<ChatClient> &client);
        void leave(const std::string &room, const std::shared_ptr<ChatClient> &client);

        // Broadcasts to every member of the room except the sender.
        void broadcast(const std::string &room, const std::shared_ptr<ChatClient> &sender, const std::string &text);
        void system_message(const std::string &room, const std::string &text);

    private:
        std::vector<std::shared_ptr<ChatClient>> snapshot(const std::string &room);
        static void send_line(const std::shared_ptr<ChatClient> &client, const std::string &line);

        std::mutex mutex;
        std::unordered_map<std::string, std::vector<std::shared_ptr<ChatClient>>> rooms;
    };
} // namespace iris::chat

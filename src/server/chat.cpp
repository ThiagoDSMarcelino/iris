#include "chat.h"

#include <algorithm>

#include "protocol.h"

namespace iris::chat
{
    void ChatHub::join(const std::string &room, const std::shared_ptr<ChatClient> &client)
    {
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->rooms[room].push_back(client); // creates the room if absent
        }

        this->system_message(room, "*** " + client->nick + " entrou na sala '" + room + "' ***");
    }

    void ChatHub::leave(const std::string &room, const std::shared_ptr<ChatClient> &client)
    {
        {
            std::lock_guard<std::mutex> lock(this->mutex);

            auto it = this->rooms.find(room);
            if (it != this->rooms.end())
            {
                auto &members = it->second;
                members.erase(std::remove(members.begin(), members.end(), client), members.end());
                if (members.empty())
                {
                    this->rooms.erase(it);
                }
            }
        }

        this->system_message(room, "*** " + client->nick + " saiu da sala ***");
    }

    void ChatHub::broadcast(const std::string &room, const std::shared_ptr<ChatClient> &sender, const std::string &text)
    {
        std::string line = sender->nick + ": " + text;
        for (const auto &member : this->snapshot(room))
        {
            if (member == sender)
            {
                continue; // the sender already sees their own message locally
            }
            send_line(member, line);
        }
    }

    void ChatHub::system_message(const std::string &room, const std::string &text)
    {
        for (const auto &member : this->snapshot(room))
        {
            send_line(member, text);
        }
    }

    std::vector<std::shared_ptr<ChatClient>> ChatHub::snapshot(const std::string &room)
    {
        std::lock_guard<std::mutex> lock(this->mutex);

        auto it = this->rooms.find(room);
        if (it == this->rooms.end())
        {
            return {};
        }

        return it->second; // copy keeps each member alive past a concurrent disconnect
    }

    void ChatHub::send_line(const std::shared_ptr<ChatClient> &client, const std::string &line)
    {
        auto frame = iris::protocol::serialize_chat_line(line);

        std::lock_guard<std::mutex> lock(client->sendMutex);
        client->socket->send_all(frame.data(), frame.size());
    }
} // namespace iris::chat

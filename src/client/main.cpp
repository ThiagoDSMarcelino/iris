#include <cstdint>
#include <iostream>
#include <string>

#include "client.h"
#include "log.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        PRINT_ERR("Usage: client <ip> <port>");
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    PRINT("=== IRIS ===");
    PRINT("1. Baixar arquivo");
    PRINT("2. Entrar no chat");
    std::cout << "> ";

    std::string choice;
    if (!std::getline(std::cin, choice))
    {
        return 0;
    }

    auto clientResult = Client::create(ip, port);
    if (!clientResult)
    {
        PRINT_ERR("Failed to create client: " << to_string(clientResult.error()));
        return 1;
    }

    if (choice == "1")
    {
        std::cout << "Arquivo: ";
        std::string filename;
        std::getline(std::cin, filename);
        if (filename.empty())
        {
            PRINT_ERR("Nome de arquivo não pode ser vazio");
            return 1;
        }

        std::cout << "Diretório de saída (vazio = atual): ";
        std::string outputDir;
        std::getline(std::cin, outputDir);

        auto client = std::move(clientResult.value());
        if (auto error = client->fetch(filename.c_str(), outputDir.c_str()))
        {
            PRINT_ERR("Failed to fetch file: " << to_string(*error));
            return 1;
        }

        return 0;
    }

    if (choice == "2")
    {
        std::cout << "Apelido: ";
        std::string nick;
        std::getline(std::cin, nick);

        std::cout << "Sala: ";
        std::string room;
        std::getline(std::cin, room);

        if (nick.empty() || room.empty())
        {
            PRINT_ERR("Apelido e sala não podem ser vazios");
            return 1;
        }

        auto client = std::move(clientResult.value());
        if (auto error = client->chat(nick, room))
        {
            PRINT_ERR("Chat error: " << to_string(*error));
            return 1;
        }

        return 0;
    }

    PRINT_ERR("Opção inválida");
    return 1;
}

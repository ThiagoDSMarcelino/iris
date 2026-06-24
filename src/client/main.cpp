#include <cstdint>
#include <iostream>
#include <string>

#include "client.h"
#include "log.h"

namespace
{
    int run_download(const char *ip, uint16_t port, const std::string &filename, const char *outputDir)
    {
        auto clientResult = Client::create(ip, port, outputDir);
        if (!clientResult)
        {
            PRINT_ERR("Failed to create client: " << to_string(clientResult.error()));
            return 1;
        }

        auto client = std::move(clientResult.value());
        if (auto error = client->fetch(filename.c_str()))
        {
            PRINT_ERR("Failed to fetch file: " << to_string(*error));
            return 1;
        }

        return 0;
    }

    int run_chat(const char *ip, uint16_t port, const std::string &nick, const std::string &room)
    {
        auto clientResult = Client::create(ip, port);
        if (!clientResult)
        {
            PRINT_ERR("Failed to create client: " << to_string(clientResult.error()));
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

    int interactive_menu(const char *ip, uint16_t port)
    {
        PRINT("=== IRIS ===");
        PRINT("1. Baixar arquivo");
        PRINT("2. Entrar no chat");
        std::cout << "> ";

        std::string choice;
        if (!std::getline(std::cin, choice))
        {
            return 0;
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

            return run_download(ip, port, filename, outputDir.empty() ? nullptr : outputDir.c_str());
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

            return run_chat(ip, port, nick, room);
        }

        PRINT_ERR("Opção inválida");
        return 1;
    }
} // namespace

int main(int argc, char *argv[])
{
    // Direct one-shot download (kept for scripting): client <ip> <port> <filename> [output_dir]
    if (argc == 4 || argc == 5)
    {
        const char *outputDir = argc == 5 ? argv[4] : nullptr;
        return run_download(argv[1], static_cast<uint16_t>(std::stoi(argv[2])), argv[3], outputDir);
    }

    // Interactive menu: client <ip> <port>
    if (argc == 3)
    {
        return interactive_menu(argv[1], static_cast<uint16_t>(std::stoi(argv[2])));
    }

    PRINT_ERR("Usage: client <ip> <port>                          (interactive menu)");
    PRINT_ERR("       client <ip> <port> <filename> [output_dir]  (direct download)");
    return 1;
}

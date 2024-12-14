#include <iostream>
#include <cstring>
#include <cstdlib>
#include <winsock2.h>
#include <fstream> // Added for file I/O

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 12345;
constexpr int BUFFER_SIZE = 1024;

void writeChatLog(const std::string& message) {
    std::ofstream logFile("chat_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

DWORD WINAPI receiveMessages(LPVOID param) {
    SOCKET clientSocket = reinterpret_cast<SOCKET>(param);

    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Server disconnected" << std::endl;
            break;
        }

        buffer[bytesRead] = '\0';

        // Display private messages only for the current client
        if (strstr(buffer, " (private):") != nullptr) {
            std::cout << buffer << std::endl;
            writeChatLog(buffer);
        } else {
            // Display messages for all clients
            std::cout << buffer << std::endl;
            writeChatLog(buffer);
        }
    }

    return 0;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error initializing Winsock" << std::endl;
        return EXIT_FAILURE;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    if ((serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1")) == INADDR_NONE) {
        std::cerr << "Invalid address" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to server" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Connected to server" << std::endl;

    // Client registration
    std::cout << "Enter your name: ";
    char clientName[1024];
    std::cin.getline(clientName, sizeof(clientName));
    send(clientSocket, clientName, strlen(clientName), 0);

    // Create a thread to receive messages from the server
    HANDLE receiveThread = CreateThread(nullptr, 0, receiveMessages, reinterpret_cast<LPVOID>(clientSocket), 0, nullptr);

    char message[BUFFER_SIZE];

    while (true) {
        // Prompt for and send the message to the server
        std::cout << "Enter message: ";
        std::cin.getline(message, sizeof(message));

        // Check if it's a private message
        if (message[0] == '@') {
            send(clientSocket, message, strlen(message), 0);
        } else {
            // Send the regular message to the server
            send(clientSocket, message, strlen(message), 0);
        }

        if (strcmp(message, "exit") == 0) {
            std::cout << "Exiting..." << std::endl;
            break;
        }
    }

    // Wait for the receive thread to finish
    WaitForSingleObject(receiveThread, INFINITE);

    closesocket(clientSocket);
    WSACleanup();
    return EXIT_SUCCESS;
}

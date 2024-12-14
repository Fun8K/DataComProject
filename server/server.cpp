#include <iostream>
#include <cstring>
#include <cstdlib>
#include <winsock2.h>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <fstream> // Added for file I/O

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 12345;
constexpr int MAX_CLIENTS = 10;

std::map<SOCKET, std::string> clientNames;

void writeChatLog(const std::string& sender, const std::string& message) {
    std::ofstream logFile("chat_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << sender << "] " << message << std::endl;
        logFile.close();
    }
}

DWORD WINAPI handleClient(LPVOID clientSocket) {
    SOCKET client = reinterpret_cast<SOCKET>(clientSocket);

    char clientName[1024];
    int bytesRead = recv(client, clientName, sizeof(clientName), 0);
    if (bytesRead <= 0) {
        std::cerr << "Client disconnected" << std::endl;
        closesocket(client);
        return 0;
    }
    clientName[bytesRead] = '\0';
    std::cout << "Client registered with name: " << clientName << std::endl;

    clientNames[client] = clientName;

    char buffer[1024];
    while (true) {
        // Get current timestamp
        std::time_t currentTime = std::time(nullptr);
        std::tm tm = *std::localtime(&currentTime);
        std::ostringstream timestamp;
        timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

        bytesRead = recv(client, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Client disconnected" << std::endl;
            closesocket(client);
            break;
        }

        buffer[bytesRead] = '\0';

       if (buffer[0] == '@') {
    // Extract the recipient's name from the message
    std::string recipientName;
    size_t pos = 1; // Start after the '@' symbol
    while (pos < bytesRead && buffer[pos] != ' ') {
        recipientName += buffer[pos];
        pos++;
    }

    // Find the recipient's socket
    auto recipientSocketIter = std::find_if(clientNames.begin(), clientNames.end(),
        [&recipientName](const auto& entry) { return entry.second == recipientName; });

    if (recipientSocketIter != clientNames.end()) {
        SOCKET recipientSocket = recipientSocketIter->first;

        // Send the private message to the recipient
        send(recipientSocket, timestamp.str().c_str(), timestamp.str().length(), 0);
        send(recipientSocket, " ", 1, 0);
        send(recipientSocket, clientName, strlen(clientName), 0);
        send(recipientSocket, " (private): ", 12, 0);
        send(recipientSocket, &buffer[pos + 1], bytesRead - pos - 1, 0);

        // Do not print the private message to the server console
        continue;
    } else {
        // Notify the sender that the recipient is not found
        send(client, "Recipient not found", 20, 0);
    }
} else {
    // Display timestamp for the server
    std::cout << timestamp.str() << " " << clientName << ": " << buffer << std::endl;

    // Write chat log
    writeChatLog(clientName, buffer);

    // Broadcast the message to all clients with timestamp
    for (const auto& entry : clientNames) {
        if (entry.first != client) {
            send(entry.first, timestamp.str().c_str(), timestamp.str().length(), 0);
            send(entry.first, " ", 1, 0);
            send(entry.first, clientName, strlen(clientName), 0);
            send(entry.first, ": ", 2, 0);
            send(entry.first, buffer, bytesRead, 0);
        }
    }
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

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Error binding to port" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (listen(serverSocket, MAX_CLIENTS) == SOCKET_ERROR) {
        std::cerr << "Error listening for connections" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientSize);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        std::cout << "Accepted connection from " << inet_ntoa(clientAddr.sin_addr) << std::endl;

        // Add the new client to the map
        clientNames[clientSocket] = "";

        // Start a new thread to handle the client
        HANDLE threadHandle = CreateThread(nullptr, 0, handleClient, reinterpret_cast<LPVOID>(clientSocket), 0, nullptr);
        if (threadHandle == nullptr) {
            std::cerr << "Error creating thread" << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        CloseHandle(threadHandle);
    }

    closesocket(serverSocket);
    WSACleanup();
    return EXIT_SUCCESS;
}

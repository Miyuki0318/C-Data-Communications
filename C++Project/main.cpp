#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 4096;

void ReceiveMessages(SOCKET socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::cout << buffer << std::endl;
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed." << std::endl;
            break;
        }
        else {
            std::cout << "Error in recv(). Quitting" << std::endl;
            break;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cout << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    std::string mode;
    std::cout << "Enter 'listen' to wait for a connection or 'connect' to connect to another peer: ";
    std::getline(std::cin, mode);

    if (mode == "listen") {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(0);  // Let the system choose a port

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "Bind failed." << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // Get the port number assigned by the system
        int addrLen = sizeof(serverAddr);
        getsockname(sock, (sockaddr*)&serverAddr, &addrLen);
        std::cout << "Listening on port " << ntohs(serverAddr.sin_port) << std::endl;

        if (listen(sock, 1) == SOCKET_ERROR) {
            std::cout << "Listen failed." << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        SOCKET clientSocket = accept(sock, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Accept failed." << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "Connection established." << std::endl;

        std::thread(ReceiveMessages, clientSocket).detach();

        std::string message;
        while (true) {
            std::getline(std::cin, message);
            if (message == "exit") break;
            std::string fullMessage = username + ": " + message;
            send(clientSocket, fullMessage.c_str(), fullMessage.length(), 0);
        }

        closesocket(clientSocket);
    }
    else if (mode == "connect") {
        std::string ip;
        std::string port;
        std::cout << "Enter IP address to connect to: ";
        std::getline(std::cin, ip);
        std::cout << "Enter port number to connect to: ";
        std::getline(std::cin, port);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(port));

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "Failed to connect." << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "Connected to peer." << std::endl;

        std::thread(ReceiveMessages, sock).detach();

        std::string message;
        while (true) {
            std::getline(std::cin, message);
            if (message == "exit") break;
            std::string fullMessage = username + ": " + message;
            send(sock, fullMessage.c_str(), fullMessage.length(), 0);
        }
    }
    else {
        std::cout << "Invalid mode." << std::endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
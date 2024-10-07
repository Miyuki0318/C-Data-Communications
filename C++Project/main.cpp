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
            std::cout << "�ڑ��������܂����B" << std::endl;
            break;
        }
        else {
            std::cout << "��M���ɃG���[���������܂����B�I�����܂��B" << std::endl;
            break;
        }
    }
}

std::string GetLocalIPAddress() {
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        return "�z�X�g���̎擾�Ɏ��s���܂���";
    }

    struct addrinfo hints, * result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
        return "IP�A�h���X�̎擾�Ɏ��s���܂���";
    }

    char ipAddress[INET_ADDRSTRLEN];
    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));

    freeaddrinfo(result);
    return std::string(ipAddress);
}

int main() {
    // �R���\�[���̕����R�[�h��UTF-8�ɐݒ�
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "Winsock�̏������Ɏ��s���܂����B" << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cout << "�\�P�b�g�̍쐬�Ɏ��s���܂����B" << std::endl;
        WSACleanup();
        return 1;
    }

    std::string username;
    std::cout << "���[�U�[������͂��Ă�������: ";
    std::getline(std::cin, username);

    std::string mode;
    std::cout << "�`���b�g���[�����쐬���܂����H (Y/N): ";
    std::getline(std::cin, mode);

    if (mode == "Y" || mode == "y") {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(0);  // �V�X�e���Ƀ|�[�g��I��������

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "�o�C���h�Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // �V�X�e�������蓖�Ă��|�[�g�ԍ����擾
        int addrLen = sizeof(serverAddr);
        getsockname(sock, (sockaddr*)&serverAddr, &addrLen);
        std::string localIP = GetLocalIPAddress();
        std::cout << "���Ȃ���IP�A�h���X: " << localIP << std::endl;
        std::cout << "�|�[�g�ԍ�: " << ntohs(serverAddr.sin_port) << std::endl;

        if (listen(sock, 1) == SOCKET_ERROR) {
            std::cout << "���b�X���Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "�ڑ���҂��Ă��܂�..." << std::endl;

        SOCKET clientSocket = accept(sock, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "�ڑ��̎󂯓���Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "�ڑ����m������܂����B" << std::endl;

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
    else if (mode == "N" || mode == "n") {
        std::string ip;
        std::string port;
        std::cout << "�ڑ����IP�A�h���X����͂��Ă�������: ";
        std::getline(std::cin, ip);
        std::cout << "�ڑ���̃|�[�g�ԍ�����͂��Ă�������: ";
        std::getline(std::cin, port);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(port));

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "�ڑ��Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "�s�A�ɐڑ����܂����B" << std::endl;

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
        std::cout << "�����ȑI���ł��B" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
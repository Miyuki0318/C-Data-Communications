#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>
// #include "gameconnect.hpp"

#pragma comment(lib, "Ws2_32.lib")

// ���b�Z�[�W�T�C�Y���
const int BUFFER_SIZE = 4096;

// �t�@�C���̕ϊ����[�h��UTF8�ɕύX����
void SetUTF8Console() {
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
}

// ���b�Z�[�W��M�p�֐�(socket : �|�[�g�ԍ�)
void ReceiveMessages(SOCKET socket) {
    // ���b�Z�[�W�i�[�p�ϐ�
    char buffer[BUFFER_SIZE];

    while (true) {
        // ��M�����o�C�g�����擾���Abuffer�Ɏ�M�����f�[�^���i�[
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived > 0) {
            // ��M�����f�[�^�̍Ō�ɋ󕶎��ǉ�
            buffer[bytesReceived] = '\0';

            // ��M�����f�[�^���o��
            std::wcout << std::wstring(buffer, buffer + bytesReceived) << std::endl;
        }
        else if (bytesReceived == 0) {
            // �f�[�^��M���Ȃ������ꍇ�A�ڑ����I������
            std::wcout << L"�ڑ��������܂����B" << std::endl;
            break;
        }
        else {
            // �������Ԃ��ꂽ�ꍇ�̓G���[
            std::wcout << L"��M���ɃG���[���������܂����B�I�����܂��B" << std::endl;
            break;
        }
    }
}

// ���g��IP�A�h���X���擾����֐�
std::wstring GetLocalIPAddress() {
    // �z�X�g��
    char hostName[256];

    // �z�X�g�����擾�ł��������`�F�b�N
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        return L"�z�X�g���̎擾�Ɏ��s���܂���";
    }

    struct addrinfo hints, * result = nullptr;
    // ��������0�N���A����
    ZeroMemory(&hints, sizeof(hints));
    // �A�h���X�t�@�~����ݒ�
    hints.ai_family = AF_INET;
    // �\�P�b�g�^�C�v��ݒ�
    hints.ai_socktype = SOCK_STREAM;
    // �ʐM�v���g�R����ݒ�
    hints.ai_protocol = IPPROTO_TCP;

    // IP�A�h���X�̎擾�̉�
    if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
        return L"IP�A�h���X�̎擾�Ɏ��s���܂���";
    }

    // IP�A�h���X�i�[�p������
    char ipAddress[INET_ADDRSTRLEN];
    // �\�P�b�g�A�h���X�i�[�p�ϐ�
    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    // �\�P�b�g�A�h���X�𕶎���ɕϊ��������̂�IP�A�h���X�p������ɑ��
    inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));
    // result�̃����������
    freeaddrinfo(result);
    // IP�A�h���X��Ԃ�
    return std::wstring(ipAddress, ipAddress + strlen(ipAddress));
}

int main() {
    // �t�@�C���̕ϊ����[�h�̕ύX
    SetUTF8Console();

    // Windows�\�P�b�g�̃f�[�^
    WSADATA wsaData;

    //  
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::wcout << L"Winsock�̏������Ɏ��s���܂����B" << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::wcout << L"�\�P�b�g�̍쐬�Ɏ��s���܂����B" << std::endl;
        WSACleanup();
        return 1;
    }

    std::wstring username;
    std::wcout << L"���[�U�[������͂��Ă�������: ";
    std::getline(std::wcin, username);

    std::wstring mode;
    std::wcout << L"�`���b�g���[�����쐬���܂����H (Y/N): ";
    std::getline(std::wcin, mode);

    if (mode == L"Y" || mode == L"y") {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(0);  // �V�X�e���Ƀ|�[�g��I��������

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::wcout << L"�o�C���h�Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // �V�X�e�������蓖�Ă��|�[�g�ԍ����擾
        int addrLen = sizeof(serverAddr);
        getsockname(sock, (sockaddr*)&serverAddr, &addrLen);
        // ���g��IP�A�h���X���擾
        std::wstring localIP = GetLocalIPAddress();
        std::wcout << L"���Ȃ���IP�A�h���X: " << localIP << std::endl;
        std::wcout << L"�|�[�g�ԍ�: " << ntohs(serverAddr.sin_port) << std::endl;

        if (listen(sock, 1) == SOCKET_ERROR) {
            std::wcout << L"���b�X���Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"�ڑ���҂��Ă��܂�..." << std::endl;

        SOCKET clientSocket = accept(sock, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::wcout << L"�ڑ��̎󂯓���Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"�ڑ����m������܂����B" << std::endl;

        std::thread(ReceiveMessages, clientSocket).detach();

        std::wstring message;
        while (true) {
            std::getline(std::wcin, message);
            if (message == L"exit") break;
            std::wstring fullMessage = username + L": " + message;
            std::string utf8Message(fullMessage.begin(), fullMessage.end());
            send(clientSocket, utf8Message.c_str(), utf8Message.length(), 0);
        }

        closesocket(clientSocket);
    }
    else if (mode == L"N" || mode == L"n") {
        std::wstring ip;
        std::wstring port;
        std::wcout << L"�ڑ����IP�A�h���X����͂��Ă�������: ";
        std::getline(std::wcin, ip);
        std::wcout << L"�ڑ���̃|�[�g�ԍ�����͂��Ă�������: ";
        std::getline(std::wcin, port);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        std::string narrow_ip(ip.begin(), ip.end());
        inet_pton(AF_INET, narrow_ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(std::string(port.begin(), port.end())));

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::wcout << L"�ڑ��Ɏ��s���܂����B" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"�s�A�ɐڑ����܂����B" << std::endl;

        std::thread(ReceiveMessages, sock).detach();

        std::wstring message;
        while (true) {
            std::getline(std::wcin, message);
            if (message == L"exit") break;
            std::wstring fullMessage = username + L": " + message;
            std::string utf8Message(fullMessage.begin(), fullMessage.end());
            send(sock, utf8Message.c_str(), utf8Message.length(), 0);
        }
    }
    else {
        std::wcout << L"�����ȑI���ł��B" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
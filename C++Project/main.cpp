#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>
#include <locale>
#include <codecvt>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 4096;

void SetUTF8Console() {
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
}

// UTF-8 to wide string conversion
std::wstring UTF8ToWide(const std::string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0)
    {
        std::wstring wstr(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }
    return L"";
}

// Wide string to UTF-8 conversion
std::string WideToUTF8(const std::wstring& wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        std::string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
        return str;
    }
    return "";
}

void ReceiveMessages(SOCKET socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string utf8Message(buffer, bytesReceived);
            std::wcout << UTF8ToWide(utf8Message) << std::endl;
        }
        else if (bytesReceived == 0) {
            std::wcout << L"�ڑ��������܂����B" << std::endl;
            break;
        }
        else {
            std::wcout << L"��M���ɃG���[���������܂����B�I�����܂��B" << std::endl;
            break;
        }
    }
}

std::wstring GetLocalIPAddress() {
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        return L"�z�X�g���̎擾�Ɏ��s���܂���";
    }

    struct addrinfo hints, * result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
        return L"IP�A�h���X�̎擾�Ɏ��s���܂���";
    }

    char ipAddress[INET_ADDRSTRLEN];
    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));

    freeaddrinfo(result);
    return std::wstring(ipAddress, ipAddress + strlen(ipAddress));
}

int main() {
    SetUTF8Console();

    WSADATA wsaData;
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
            std::string utf8Message = WideToUTF8(fullMessage);
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
        std::string narrow_ip = WideToUTF8(ip);
        inet_pton(AF_INET, narrow_ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(WideToUTF8(port)));

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
            std::string utf8Message = WideToUTF8(fullMessage);
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

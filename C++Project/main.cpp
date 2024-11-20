#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>
#include <locale>
#include <codecvt>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 4096;

// �R���\�[���̏o�͂�UTF-8�ɐݒ�
void SetUnicodeConsole() {
    // UTF-8�R�[�h�y�[�W��L����
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // �W�����o�͂�UTF-8���[�h�ɐݒ�
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // �R���\�[���t�H���g��MS Gothic���̓��{��t�H���g�ɐݒ�
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"MS Gothic");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);
}

// wstring����UTF-8�ɕϊ�����֐��i���ǔŁj
std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // null�I�[�������폜
    if (!utf8_str.empty() && utf8_str.back() == '\0') {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8����wstring�ɕϊ�����֐��i���ǔŁj
std::wstring UTF8ToWString(const std::string& utf8Str) {
    if (utf8Str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    std::wstring wide_str(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // null�I�[�������폜
    if (!wide_str.empty() && wide_str.back() == L'\0') {
        wide_str.pop_back();
    }

    return wide_str;
}

void ReceiveMessages(SOCKET socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string utf8Message(buffer, bytesReceived);
            std::wstring wMessage = UTF8ToWString(utf8Message);
            std::wcout << wMessage << std::endl;
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
    // �R���\�[���̐ݒ���ŏ��ɍs��
    SetUnicodeConsole();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::wcout << L"Winsock�̏������Ɏ��s���܂����B" << std::endl;
        return 1;
    }

    // �o�b�t�@���t���b�V�����邽�߂ɓ���������
    std::wcout.sync_with_stdio(false);
    std::wcin.sync_with_stdio(false);

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
            std::string utf8Message = WStringToUTF8(fullMessage);
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

        std::wcout << L"�ڑ����܂����B" << std::endl;

        std::thread(ReceiveMessages, sock).detach();

        std::wstring message;
        while (true) {
            std::getline(std::wcin, message);
            if (message == L"exit") break;
            std::wstring fullMessage = username + L": " + message;
            std::string utf8Message = WStringToUTF8(fullMessage);
            send(sock, utf8Message.c_str(), utf8Message.length(), 0);
        }

        closesocket(sock);
    }
    else {
        std::wcout << L"�����ȑI���ł��B" << std::endl;
    }

    WSACleanup();
    return 0;
}
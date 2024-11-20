#include "PPDataConnecter.h"
#include <iostream>
#include <string>
#include <thread>
#include <fcntl.h>
#include <io.h>
#include <locale>
#include <codecvt>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 4096;

// wstringからUTF-8に変換する関数（改良版）
std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // null終端文字を削除
    if (!utf8_str.empty() && utf8_str.back() == '\0') {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8からwstringに変換する関数（改良版）
std::wstring UTF8ToWString(const std::string& utf8Str) {
    if (utf8Str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    std::wstring wide_str(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // null終端文字を削除
    if (!wide_str.empty() && wide_str.back() == L'\0') {
        wide_str.pop_back();
    }

    return wide_str;
}

PPDataConnecter::PPDataConnecter() {}

PPDataConnecter::~PPDataConnecter() {
    WSACleanup();
}

void PPDataConnecter::InitializeWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Winsockの初期化に失敗しました。");
    }
}

void PPDataConnecter::SetConsoleToUnicode() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

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

SOCKET PPDataConnecter::CreateSocket() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("ソケットの作成に失敗しました。");
    }
    return sock;
}

void PPDataConnecter::BindAndListen(SOCKET& serverSocket) {
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("バインドに失敗しました。");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        throw std::runtime_error("リッスンに失敗しました。");
    }
}

void PPDataConnecter::AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket) {
    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        throw std::runtime_error("接続の受け入れに失敗しました。");
    }
}

void PPDataConnecter::StartServer(SOCKET& serverSocket, const std::wstring& username) {
    BindAndListen(serverSocket);

    sockaddr_in serverAddr;
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);
    std::wcout << L"あなたのIPアドレス: " << GetLocalIPAddress() << std::endl;
    std::wcout << L"ポート番号: " << ntohs(serverAddr.sin_port) << std::endl;
    std::wcout << L"接続を待っています..." << std::endl;

    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);

    std::wcout << L"接続が確立されました。" << std::endl;
    std::thread(ReceiveMessages, clientSocket).detach();

    std::wstring message;
    while (true) {
        std::getline(std::wcin, message);
        if (message == L"exit") break;
        SendMessage(clientSocket, username, message);
    }

    closesocket(clientSocket);
}

void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const std::wstring& username) {
    std::wstring ip, port;
    std::wcout << L"接続先のIPアドレスを入力してください: ";
    std::getline(std::wcin, ip);
    std::wcout << L"接続先のポート番号を入力してください: ";
    std::getline(std::wcin, port);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, WStringToUTF8(ip).c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(std::stoi(WStringToUTF8(port)));

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("接続に失敗しました。");
    }

    std::wcout << L"接続しました。" << std::endl;
    std::thread(ReceiveMessages, clientSocket).detach();

    std::wstring message;
    while (true) {
        std::getline(std::wcin, message);
        if (message == L"exit") break;
        SendMessage(clientSocket, username, message);
    }
}

void PPDataConnecter::SendMessage(SOCKET sock, const std::wstring& username, const std::wstring& message) {
    std::wstring fullMessage = username + L": " + message;
    std::string utf8Message = WStringToUTF8(fullMessage);
    send(sock, utf8Message.c_str(), utf8Message.length(), 0);
}

void PPDataConnecter::ReceiveMessages(SOCKET socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::wcout << UTF8ToWString(std::string(buffer, bytesReceived)) << std::endl;
        }
        else {
            break;
        }
    }
}

std::wstring PPDataConnecter::GetLocalIPAddress() {
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostName, nullptr, &hints, &result);

    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));

    freeaddrinfo(result);
    return UTF8ToWString(ipAddress);
}

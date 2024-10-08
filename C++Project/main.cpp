#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 4096;

void SetUTF8Console() {
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
}

void ReceiveMessages(SOCKET socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::wcout << std::wstring(buffer, buffer + bytesReceived) << std::endl;
        }
        else if (bytesReceived == 0) {
            std::wcout << L"接続が閉じられました。" << std::endl;
            break;
        }
        else {
            std::wcout << L"受信中にエラーが発生しました。終了します。" << std::endl;
            break;
        }
    }
}

std::wstring GetLocalIPAddress() {
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        return L"ホスト名の取得に失敗しました";
    }

    struct addrinfo hints, * result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
        return L"IPアドレスの取得に失敗しました";
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
        std::wcout << L"Winsockの初期化に失敗しました。" << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::wcout << L"ソケットの作成に失敗しました。" << std::endl;
        WSACleanup();
        return 1;
    }

    std::wstring username;
    std::wcout << L"ユーザー名を入力してください: ";
    std::getline(std::wcin, username);

    std::wstring mode;
    std::wcout << L"チャットルームを作成しますか？ (Y/N): ";
    std::getline(std::wcin, mode);

    if (mode == L"Y" || mode == L"y") {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(0);  // システムにポートを選択させる

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::wcout << L"バインドに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // システムが割り当てたポート番号を取得
        int addrLen = sizeof(serverAddr);
        getsockname(sock, (sockaddr*)&serverAddr, &addrLen);
        std::wstring localIP = GetLocalIPAddress();
        std::wcout << L"あなたのIPアドレス: " << localIP << std::endl;
        std::wcout << L"ポート番号: " << ntohs(serverAddr.sin_port) << std::endl;

        if (listen(sock, 1) == SOCKET_ERROR) {
            std::wcout << L"リッスンに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"接続を待っています..." << std::endl;

        SOCKET clientSocket = accept(sock, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::wcout << L"接続の受け入れに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"接続が確立されました。" << std::endl;

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
        std::wcout << L"接続先のIPアドレスを入力してください: ";
        std::getline(std::wcin, ip);
        std::wcout << L"接続先のポート番号を入力してください: ";
        std::getline(std::wcin, port);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        std::string narrow_ip(ip.begin(), ip.end());
        inet_pton(AF_INET, narrow_ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(std::string(port.begin(), port.end())));

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::wcout << L"接続に失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::wcout << L"ピアに接続しました。" << std::endl;

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
        std::wcout << L"無効な選択です。" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
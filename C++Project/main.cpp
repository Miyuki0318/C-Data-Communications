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
            std::cout << "接続が閉じられました。" << std::endl;
            break;
        }
        else {
            std::cout << "受信中にエラーが発生しました。終了します。" << std::endl;
            break;
        }
    }
}

std::string GetLocalIPAddress() {
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        return "ホスト名の取得に失敗しました";
    }

    struct addrinfo hints, * result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
        return "IPアドレスの取得に失敗しました";
    }

    char ipAddress[INET_ADDRSTRLEN];
    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));

    freeaddrinfo(result);
    return std::string(ipAddress);
}

int main() {
    // コンソールの文字コードをUTF-8に設定
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "Winsockの初期化に失敗しました。" << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cout << "ソケットの作成に失敗しました。" << std::endl;
        WSACleanup();
        return 1;
    }

    std::string username;
    std::cout << "ユーザー名を入力してください: ";
    std::getline(std::cin, username);

    std::string mode;
    std::cout << "チャットルームを作成しますか？ (Y/N): ";
    std::getline(std::cin, mode);

    if (mode == "Y" || mode == "y") {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(0);  // システムにポートを選択させる

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "バインドに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // システムが割り当てたポート番号を取得
        int addrLen = sizeof(serverAddr);
        getsockname(sock, (sockaddr*)&serverAddr, &addrLen);
        std::string localIP = GetLocalIPAddress();
        std::cout << "あなたのIPアドレス: " << localIP << std::endl;
        std::cout << "ポート番号: " << ntohs(serverAddr.sin_port) << std::endl;

        if (listen(sock, 1) == SOCKET_ERROR) {
            std::cout << "リッスンに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "接続を待っています..." << std::endl;

        SOCKET clientSocket = accept(sock, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "接続の受け入れに失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "接続が確立されました。" << std::endl;

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
        std::cout << "接続先のIPアドレスを入力してください: ";
        std::getline(std::cin, ip);
        std::cout << "接続先のポート番号を入力してください: ";
        std::getline(std::cin, port);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(std::stoi(port));

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "接続に失敗しました。" << std::endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        std::cout << "ピアに接続しました。" << std::endl;

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
        std::cout << "無効な選択です。" << std::endl;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
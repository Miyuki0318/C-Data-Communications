#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <thread>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

void receiveMessages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            cout << "相手: " << buffer << endl;
        }
    }
}

void sendMessages(SOCKET sock, const string& username) {
    char buffer[1024];
    while (true) {
        string message;
        getline(cin, message);
        message = username + ": " + message;
        send(sock, message.c_str(), message.size(), 0);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // ポート番号の入力と接続先の情報設定
    string mode;
    cout << "接続モードを選択してください (server/client): ";
    cin >> mode;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ローカルホストを使用
    cin.ignore();

    if (mode == "server") {
        // サーバーモード（相手の接続を待つ）
        int port;
        cout << "使用するポート番号を入力してください: ";
        cin >> port;
        addr.sin_port = htons(port);

        bind(sock, (sockaddr*)&addr, sizeof(addr));
        listen(sock, SOMAXCONN);

        SOCKET clientSock = accept(sock, NULL, NULL);
        cout << "接続されました。" << endl;
        string username;
        cout << "ユーザー名を入力してください: ";
        cin >> username;

        thread receiveThread(receiveMessages, clientSock);
        sendMessages(clientSock, username);
        receiveThread.join();
        closesocket(clientSock);

    }
    else if (mode == "client") {
        // クライアントモード（サーバーに接続）
        int port;
        cout << "相手のポート番号を入力してください: ";
        cin >> port;
        addr.sin_port = htons(port);

        connect(sock, (sockaddr*)&addr, sizeof(addr));
        cout << "接続しました。" << endl;

        string username;
        cout << "ユーザー名を入力してください: ";
        cin >> username;

        thread receiveThread(receiveMessages, sock);
        sendMessages(sock, username);
        receiveThread.join();
        closesocket(sock);
    }

    WSACleanup();
    return 0;
}
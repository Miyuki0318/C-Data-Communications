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
            cout << "����: " << buffer << endl;
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

    // �|�[�g�ԍ��̓��͂Ɛڑ���̏��ݒ�
    string mode;
    cout << "�ڑ����[�h��I�����Ă������� (server/client): ";
    cin >> mode;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ���[�J���z�X�g���g�p
    cin.ignore();

    if (mode == "server") {
        // �T�[�o�[���[�h�i����̐ڑ���҂j
        int port;
        cout << "�g�p����|�[�g�ԍ�����͂��Ă�������: ";
        cin >> port;
        addr.sin_port = htons(port);

        bind(sock, (sockaddr*)&addr, sizeof(addr));
        listen(sock, SOMAXCONN);

        SOCKET clientSock = accept(sock, NULL, NULL);
        cout << "�ڑ�����܂����B" << endl;
        string username;
        cout << "���[�U�[������͂��Ă�������: ";
        cin >> username;

        thread receiveThread(receiveMessages, clientSock);
        sendMessages(clientSock, username);
        receiveThread.join();
        closesocket(clientSock);

    }
    else if (mode == "client") {
        // �N���C�A���g���[�h�i�T�[�o�[�ɐڑ��j
        int port;
        cout << "����̃|�[�g�ԍ�����͂��Ă�������: ";
        cin >> port;
        addr.sin_port = htons(port);

        connect(sock, (sockaddr*)&addr, sizeof(addr));
        cout << "�ڑ����܂����B" << endl;

        string username;
        cout << "���[�U�[������͂��Ă�������: ";
        cin >> username;

        thread receiveThread(receiveMessages, sock);
        sendMessages(sock, username);
        receiveThread.join();
        closesocket(sock);
    }

    WSACleanup();
    return 0;
}
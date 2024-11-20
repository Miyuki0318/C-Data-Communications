#include "PPDataConnector.h"
#include <iostream>
#include <thread>
#include <codecvt>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") // Winsockライブラリをリンクする

using namespace std; // std:: 名前空間を省略する

// コンストラクタ
PPDataConnector::PPDataConnector(int port)
    : port_(port), isHost_(true)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        wcout << L"Winsockの初期化に失敗しました。" << endl;
        exit(1);
    }

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET)
    {
        wcout << L"ソケットの作成に失敗しました。" << endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        wcout << L"バインドに失敗しました。" << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }

    if (listen(socket_, 1) == SOCKET_ERROR)
    {
        wcout << L"リッスンに失敗しました。" << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }
}

// コンストラクタ（クライアント用）
PPDataConnector::PPDataConnector(int port, const wstring& ipAddress)
    : port_(port), isHost_(false), ipAddress_(ipAddress)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        wcout << L"Winsockの初期化に失敗しました。" << endl;
        exit(1);
    }

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET)
    {
        wcout << L"ソケットの作成に失敗しました。" << endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    wcout << L"ホストに接続中..." << endl;
    inet_pton(AF_INET, wstring_to_string(ipAddress_).c_str(), &addr.sin_addr);
    if (connect(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int errCode = WSAGetLastError();
        wcout << L"接続に失敗しました。エラーコード: " << errCode << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }
}

// デストラクタ
PPDataConnector::~PPDataConnector()
{
    closesocket(socket_);
    WSACleanup();
}

// メッセージを送信する関数（UTF-16）
void PPDataConnector::SendPPMessage(const wstring& message)
{
    // 送信メッセージをUTF-16に変換
    int messageLength = message.length() * sizeof(wchar_t); // UTF-16バイト長
    send(socket_, reinterpret_cast<const char*>(&messageLength), sizeof(messageLength), 0); // メッセージ長を送信

    // メッセージ内容を送信
    send(socket_, reinterpret_cast<const char*>(message.c_str()), messageLength, 0);
}

// メッセージを受信する関数（UTF-16）
wstring PPDataConnector::ReceiveMessage()
{
    int messageLength = 0;

    // メッセージの長さを受信
    int bytesReceived = recv(socket_, reinterpret_cast<char*>(&messageLength), sizeof(messageLength), 0);
    if (bytesReceived <= 0) return L""; // 受信失敗または接続切れ

    // メッセージ内容を受信
    char* buffer = new char[messageLength];
    bytesReceived = recv(socket_, buffer, messageLength, 0);
    if (bytesReceived <= 0) {
        delete[] buffer;
        return L"受信失敗"; // 受信失敗
    }

    // 受信したバイナリデータをUTF-16に変換
    wstring message(reinterpret_cast<wchar_t*>(buffer), messageLength / sizeof(wchar_t));
    delete[] buffer;

    return message;
}

// ソケットオブジェクトを取得する関数
SOCKET PPDataConnector::GetSocket() const
{
    return socket_;
}

// ホストのIPアドレスを取得するメソッド
wstring PPDataConnector::GetHostIP()
{
    // Winsockの初期化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return L"IPアドレスの取得に失敗しました。";
    }

    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) != 0)
    {
        WSACleanup();
        return L"ホスト名の取得に失敗しました。";
    }

    struct addrinfo* result = nullptr;
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4アドレス

    // ホスト名からIPアドレスを取得
    if (getaddrinfo(hostName, nullptr, &hints, &result) != 0)
    {
        WSACleanup();
        return L"IPアドレスの取得に失敗しました。";
    }

    std::wstring ipAddress = L"";
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ip, sizeof(ip));
        ipAddress = std::wstring(ip, ip + strlen(ip));
        break; // 最初のIPアドレスを取得
    }

    freeaddrinfo(result);
    WSACleanup();

    return ipAddress;
}

// ソケット通信を開始する関数
void PPDataConnector::Start() const
{
    if (isHost_)
    {
        sockaddr_in clientAddr;
        int addrSize = sizeof(clientAddr);

        wcout << L"接続中・・・" << endl;

        // クライアント接続を待機
        SOCKET clientSocket = accept(socket_, (sockaddr*)&clientAddr, &addrSize);

        if (clientSocket != INVALID_SOCKET)
        {
            wcout << L"接続が確立されました。" << endl;

            // 新しいスレッドでメッセージの受信を開始
            thread([clientSocket]()
                {
                    char buffer[BUFFER_SIZE];
                    while (true)
                    {
                        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (bytesReceived > 0)
                        {
                            buffer[bytesReceived] = '\0';
                            wcout << L"メッセージ受信: " << string_to_wstring(buffer) << endl;
                        }
                        else
                        {
                            break; // 受信失敗時はループを終了
                        }
                    }
                }).detach();
        }
        else
        {
            wcout << L"接続失敗" << endl;
        }
    }
}

// wstringからstringへの変換関数（UTF-16対応）
string wstring_to_string(const wstring& wstr)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
        return str;
    }
    return "";
}

// stringからwstringへの変換関数（UTF-16対応）
wstring string_to_wstring(const string& str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0)
    {
        wstring wstr(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }
    return L"";
}

#include "PPDataConnecter.h"
#include <iostream>
#include <string>
#include <thread>
#include <fcntl.h>
#include <io.h>
#include <locale>
#include <codecvt>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <Windows.h>
#include <ws2tcpip.h>

#undef max  // 型の最大用のmaxを使う為のundef
#pragma comment(lib, "Ws2_32.lib") // Winsock2ライブラリのリンク指定

const int BUFFER_SIZE = 4096; // 受信バッファのサイズを定義

// wstringからUTF-8に変換する関数（改良版）
string WStringToUTF8(const wstring& wstr)
{
    if (wstr.empty()) return string(); // 空文字列の場合はそのまま返す

    // 必要なバッファサイズを取得
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string utf8_str(size_needed, 0); // 必要なサイズで文字列を初期化
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // null終端文字を削除
    if (!utf8_str.empty() && utf8_str.back() == '\0')
    {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8からwstringに変換する関数（改良版）
wstring UTF8ToWString(const string& utf8Str)
{
    if (utf8Str.empty()) return wstring(); // 空文字列の場合はそのまま返す

    // 必要なバッファサイズを取得
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    wstring wide_str(size_needed, 0); // 必要なサイズで文字列を初期化
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // null終端文字を削除
    if (!wide_str.empty() && wide_str.back() == L'\0')
    {
        wide_str.pop_back();
    }

    return wide_str;
}

// コンストラクタ：特に初期化は行わない
PPDataConnecter::PPDataConnecter() {}

// デストラクタ：Winsockのクリーンアップを行う
PPDataConnecter::~PPDataConnecter()
{
    WSACleanup();
}

// Winsockの初期化
void PPDataConnecter::InitializeWinsock()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
    {
        throw runtime_error("Winsockの初期化に失敗しました。");
    }
}

// コンソールをUTF-8対応に設定
void PPDataConnecter::SetConsoleToUnicode()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // コンソールフォントをMSゴシックに設定
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

// ソケットを作成
SOCKET PPDataConnecter::CreateSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        throw runtime_error("ソケットの作成に失敗しました。");
    }
    return sock;
}

// ソケットをバインドし、リッスンを開始
void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 全てのローカルIPアドレスをバインド
    serverAddr.sin_port = htons(0); // 任意のポート番号を使用

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("バインドに失敗しました。");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        throw runtime_error("リッスンに失敗しました。");
    }
}

// 接続を受け入れる
void PPDataConnecter::AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket)
{
    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET)
    {
        throw runtime_error("接続の受け入れに失敗しました。");
    }
}

// サーバーを開始してクライアントからの接続を待つ
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    BindAndListen(serverSocket);

    sockaddr_in serverAddr;
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen); // バインドされたポート番号を取得
    wcout << L"サーバーID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"接続を待っています..." << endl;

    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);

    wcout << L"接続が確立されました。" << endl;
    thread(ReceivePPMessages, clientSocket).detach(); // 別スレッドでメッセージ受信を開始

    wstring message;
    while (true)
    {
        getline(wcin, message);
        if (message == L"exit") break; // exitで終了
        SendPPMessage(clientSocket, username, message);
    }

    closesocket(clientSocket);
}

// クライアントとしてサーバーに接続
void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    string id;
    wcout << L"接続先のサーバーIDを入力してください: ";
    getline(cin, id);
    auto decodeID = DecodeAndReverseIPPort(id);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr); // IPアドレスをバイナリ形式に変換
    serverAddr.sin_port = htons(decodeID.second);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("接続に失敗しました。");
    }

    wcout << L"接続しました。" << endl;
    thread(ReceivePPMessages, clientSocket).detach(); // 別スレッドでメッセージ受信を開始

    wstring message;
    while (true)
    {
        getline(wcin, message);
        if (message == L"exit") break;
        SendPPMessage(clientSocket, username, message);
    }
}

// メッセージを送信
void PPDataConnecter::SendPPMessage(SOCKET sock, const wstring& username, const wstring& message)
{
    wstring fullMessage = username + L": " + message; // ユーザー名とメッセージを結合
    string utf8Message = WStringToUTF8(fullMessage);
    
    // 長さチェック
    if (utf8Message.length() > static_cast<size_t>(std::numeric_limits<int>::max())) 
    {
        throw std::runtime_error("送信メッセージが大きすぎます。");
    }

    send(sock, utf8Message.c_str(), static_cast<int>(utf8Message.length()), 0);
}

// メッセージを受信
void PPDataConnecter::ReceivePPMessages(SOCKET socket)
{
    char buffer[BUFFER_SIZE];
    while (true)
    {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0'; // 受信データをnull終端
            wcout << UTF8ToWString(string(buffer, bytesReceived)) << endl;
        }
        else
        {
            break; // 接続が閉じられた場合
        }
    }
}

// ローカルIPアドレスを取得
string PPDataConnecter::GetLocalIPAddress()
{
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostName, nullptr, &hints, &result);

    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);

    freeaddrinfo(result);
    return string(ipAddress);
}

// ローカルIPアドレスを取得
wstring PPDataConnecter::GetLocalIPAddressW()
{
    return UTF8ToWString(GetLocalIPAddress());
}

// エンコードして反転する関数
string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // inet_ptonを使用してIPアドレスをバイナリ形式に変換
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1) 
    {
        throw invalid_argument("Invalid IP address.");
    }

    // バイナリ形式から16進数に変換
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ipHexStream;
    ipHexStream << uppercase << hex << setw(8) << setfill('0') << ntohl(ip);
    string ipHex = ipHexStream.str();

    // ポート番号を16進数に変換
    ostringstream portHexStream;
    portHexStream << uppercase << hex << setw(4) << setfill('0') << port;
    string portHex = portHexStream.str();

    // IPとポートを結合
    string combined = ipHex + "-" + portHex;

    // 文字列を反転
    reverse(combined.begin(), combined.end());

    return combined;
}

// 反転してデコードする関数
pair<string, unsigned short> PPDataConnecter::DecodeAndReverseIPPort(const string& encodedReversed)
{
    // 文字列を反転
    string combined = encodedReversed;
    reverse(combined.begin(), combined.end());

    // IPとポートを分離
    size_t dashPos = combined.find('-');
    if (dashPos == string::npos) 
    {
        throw invalid_argument("Invalid encoded string format.");
    }

    string ipHex = combined.substr(0, dashPos);
    string portHex = combined.substr(dashPos + 1);

    // IPを復元
    unsigned long ipNum = stoul(ipHex, nullptr, 16);
    ipNum = htonl(ipNum); // ネットワークバイトオーダーに変換

    // inet_ntop を使用してバイナリから文字列IPアドレスに変換
    char ipAddress[INET_ADDRSTRLEN];
    struct in_addr ipAddr;
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr) 
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // ポートを復元
    unsigned short port = static_cast<unsigned short>(stoul(portHex, nullptr, 16));

    return { ipAddress, port };
}
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
#include <winsock2.h>
#include <ws2tcpip.h>

#undef max  // 型の最大用のmaxを使う為のundef
#pragma comment(lib, "Ws2_32.lib") // Winsock2ライブラリのリンク指定

PPDataConnecter* PPDataConnecter::instance = nullptr;
mutex PPDataConnecter::instanceMutex;

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

// 10進数から64進数に変換する関数
string ConvertToBase64(unsigned long number) {
    const string base64chars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

    string result;
    do {
        result = base64chars[number % 64] + result;
        number /= 64;
    } while (number > 0);

    return result;
}

// 64進数から10進数に変換する関数
unsigned long ConvertFromBase64(const string& base64Str) {
    const string base64chars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

    unsigned long result = 0;
    unsigned long base = 1;

    for (int i = base64Str.length() - 1; i >= 0; --i) {
        size_t pos = base64chars.find(base64Str[i]);
        if (pos == string::npos) {
            throw invalid_argument("Invalid base64 character.");
        }
        result += pos * base;
        base *= 64;
    }

    return result;
}

PPDataConnecter::PPDataConnecter() : isRunning(false), isConnecting(false), currentSocket(INVALID_SOCKET) {}

PPDataConnecter::~PPDataConnecter() {
    Finalize();
}

PPDataConnecter* PPDataConnecter::GetNetworkPtr()
{
    lock_guard<mutex> lock(instanceMutex);
    if (!instance)
    {
        instance = new PPDataConnecter();
    }
    return instance;
}

void PPDataConnecter::Initialize() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw runtime_error("Winsockの初期化に失敗しました。");
    }
    isRunning = true;
}

void PPDataConnecter::Finalize() {
    StopCommunication();
    isRunning = false;
    WSACleanup();
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
    CONSOLE_FONT_INFOEX cfi = {};
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

void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        throw runtime_error("ソケットの作成に失敗しました。");
    }

    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0);

    if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw runtime_error("バインドに失敗しました。");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
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

void PPDataConnecter::StartServer()
{
    if (serverThread.joinable()) serverThread.join();

    isConnecting = true;
    serverThread = thread([this]()
        {
            SOCKET serverSocket = CreateSocket();
            SOCKET clientSocket = INVALID_SOCKET;

            try
            {
                BindAndListen(serverSocket);
                sockaddr_in serverAddr = {};
                int addrLen = sizeof(serverAddr);
                getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);

                wstring id = UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), serverAddr.sin_port));
                wcout << L"サーバーID : " << id << endl;
                wcout << L"接続を待機しています..." << endl;

                while (isConnecting) {
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(serverSocket, &readfds);
                    timeval timeout = { 0, 500000 }; // 0.5秒のタイムアウト

                    if (select(0, &readfds, nullptr, nullptr, &timeout) > 0) {
                        AcceptConnection(serverSocket, clientSocket);
                        wcout << L"クライアントと接続されました。" << endl;
                        isConnecting = false;
                        StartCommunication(clientSocket);
                    }
                }
            }
            catch (const exception& e)
            {
                cerr << "サーバーエラー: " << e.what() << endl;
            }

            if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
        }
    );
}
void PPDataConnecter::ConnectToServer()
{
    if (clientThread.joinable()) clientThread.join();

    isConnecting = true;
    clientThread = thread([this]()
        {
            try
            {
                wstring ipAndPort;
                wcout << L"接続先のサーバーIDを入力: ";
                wcin >> ipAndPort;
                auto id = DecodeAndReverseIPPort(WStringToUTF8(ipAndPort));

                SOCKET clientSocket = INVALID_SOCKET;
                ConnectToServerInternal(clientSocket, id.first, id.second);

                wcout << L"サーバーに接続しました。" << endl;
                StartCommunication(clientSocket);
            }
            catch (const exception& e)
            {
                cerr << "クライアントエラー: " << e.what() << endl;
            }
        }
    );
}

// クライアント接続内部処理
void PPDataConnecter::ConnectToServerInternal(SOCKET& clientSocket, const string& ip, int port)
{
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET)
    {
        throw runtime_error("ソケットの作成に失敗しました。");
    }

    // ノンブロッキングモードを有効化
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    while (isConnecting)
    {
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0)
        {
            break;
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    if (!isConnecting) throw runtime_error("接続キャンセルされました。");
}

// 送信バッファにデータを追加
void PPDataConnecter::AddToSendBuffer(const std::string& header, const std::string& data) {
    std::lock_guard<std::mutex> lock(sendMutex);
    sendBuffer.push_back({ header, data });
}

// 受信バッファからデータを取得
bool PPDataConnecter::GetFromRecvBuffer(BufferedData& outData) {
    std::lock_guard<std::mutex> lock(recvMutex);
    if (recvBuffer.empty()) return false;

    outData = recvBuffer.front();
    recvBuffer.pop_front();
    return true;
}

// 受信バッファから特定のヘッダのデータを取得
bool PPDataConnecter::GetFromRecvBufferByHeader(const string& header, BufferedData& outData) {
    std::lock_guard<std::mutex> lock(recvMutex);

    for (auto it = recvBuffer.begin(); it != recvBuffer.end(); ++it) {
        if (it->header == header) {
            outData = *it;
            recvBuffer.erase(it);
            return true;
        }
    }
    return false;  // 指定ヘッダのデータがない
}

void PPDataConnecter::StartCommunication(SOCKET sock)
{
    {
        lock_guard<mutex> lock(socketMutex);
        currentSocket = sock;
    }

    isRunning = true;
    thread sendThread([this, sock]()
        {
            while (isRunning)
            {
                string message;
                getline(cin, message);
                send(sock, message.c_str(), message.size(), 0);
            }
        }
    );

    thread receiveThread([this, sock]()
        {
            char buffer[1024];
            while (isRunning)
            {
                int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
                if (bytesReceived > 0)
                {
                    wcout << L"受信: " << UTF8ToWString(string(buffer, bytesReceived)) << endl;
                }
            }
        }
    );

    sendThread.detach();
    receiveThread.detach();
}

void PPDataConnecter::StopCommunication()
{
    isConnecting = false; // 接続待機中の処理をキャンセル
    if (serverThread.joinable()) serverThread.join();
    if (clientThread.joinable()) clientThread.join();

    lock_guard<mutex> lock(socketMutex);
    if (currentSocket != INVALID_SOCKET) {
        closesocket(currentSocket);
        currentSocket = INVALID_SOCKET;
    }
}

// メッセージを送信
void PPDataConnecter::SendPPMessage(SOCKET sock, const wstring& username, const wstring& message)
{
    wstring fullMessage = username + L": " + message; // ユーザー名とメッセージを結合
    string utf8Message = WStringToUTF8(fullMessage);

    // 長さチェック
    if (utf8Message.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("送信メッセージが大きすぎます。");
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

// バッファからデータを取得し送信する
void PPDataConnecter::SendData() {
    std::lock_guard<std::mutex> lock(sendMutex);
    if (sendBuffer.empty()) return;

    BufferedData data = sendBuffer.front();
    sendBuffer.pop_front();

    // 送信処理（ネットワークAPIと統合する）
    std::string rawData = Serialize(data);
    // send(rawData);  // ここにネットワーク送信処理を実装
}

// 受信データを処理
void PPDataConnecter::ReceiveData(const std::string& rawData) {
    BufferedData data = Deserialize(rawData);

    std::lock_guard<std::mutex> lock(recvMutex);
    recvBuffer.push_back(data);
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

string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // inet_ptonを使用してIPアドレスをバイナリ形式に変換
    sockaddr_in sa = {};
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1)
    {
        throw invalid_argument("Invalid IP address.");
    }

    // バイナリ形式から64進数に変換
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ip64Stream;
    ip64Stream << uppercase << ConvertToBase64(ntohl(ip));
    string ip64 = ip64Stream.str();

    // ポート番号を64進数に変換
    ostringstream port64Stream;
    port64Stream << uppercase << ConvertToBase64(port);
    string port64 = port64Stream.str();

    // IPとポートを結合
    string combined = ip64 + "-" + port64;

    // 文字列を反転
    reverse(combined.begin(), combined.end());

    return combined;
}

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

    string ip64 = combined.substr(0, dashPos);
    string port64 = combined.substr(dashPos + 1);

    // IPを復元
    unsigned long ipNum = ConvertFromBase64(ip64);
    ipNum = htonl(ipNum); // ネットワークバイトオーダーに変換

    // inet_ntop を使用してバイナリから文字列IPアドレスに変換
    char ipAddress[INET_ADDRSTRLEN];
    in_addr ipAddr = {};
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr)
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // ポートを復元
    unsigned short port = ConvertFromBase64(port64);

    return { ipAddress, port };
}


// **データを "ヘッダ:データ" の形式でシリアライズ**
string PPDataConnecter::Serialize(const BufferedData& data) {
    return data.header + ":" + data.data;
}

// **受信データをパース**
BufferedData PPDataConnecter::Deserialize(const std::string& rawData) {
    size_t pos = rawData.find(':');
    if (pos == std::string::npos) {
        return { "Unknown", rawData };  // ヘッダがない場合は "Unknown" とする
    }
    return { rawData.substr(0, pos), rawData.substr(pos + 1) };
}
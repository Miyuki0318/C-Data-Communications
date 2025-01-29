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

PPDataConnecter::PPDataConnecter() : isConnected(false), isWaiting(false), isCanceled(false) {}

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
    isConnected = false;
}

void PPDataConnecter::Finalize() {
    StopCommunication();
    isConnected = false;
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

// 通信待機とキャンセル状態を管理
void PPDataConnecter::StartServerAsync(SOCKET& serverSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // サーバー開始をスレッドで実行
    thread serverThread([&]() {
        try {
            // サーバー開始処理
            StartServer(serverSocket, username);
            isConnected = true;
        }
        catch (const runtime_error& e) {
            wcout << L"サーバーの開始に失敗しました: " << UTF8ToWString(e.what()) << endl;
        }
        isWaiting = false;
        });

    serverThread.detach();
}

// サーバー接続をスレッドで開始
void PPDataConnecter::ConnectToServerAsync(SOCKET& clientSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // クライアント接続処理を非同期で実行
    thread clientThread([&]() {
        try {
            // サーバー接続処理
            ConnectToServer(clientSocket, username);
            isConnected = true;
        }
        catch (const runtime_error& e) {
            wcout << L"サーバーへの接続に失敗しました: " << UTF8ToWString(e.what()) << endl;
        }
        isWaiting = false;
        });

    clientThread.detach();
}

void PPDataConnecter::StartCommunication(SOCKET sock)
{
    {
        lock_guard<mutex> lock(socketMutex);
        currentSocket = sock;
    }

    // 既存の通信スレッドを停止
    StopCommunication();

    // 新しい通信スレッドを開始
    isConnected = true;
}

// 待機中のキャンセル処理
void PPDataConnecter::CancelCommunication()
{
    if (isWaiting) {
        // 待機状態をキャンセル
        isCanceled = true;
        isWaiting = false;
        // 必要なら待機中のスレッドを終了する処理を追加
    }
}

// 通信を停止
void PPDataConnecter::StopCommunication()
{
    if (sendThread.joinable()) {
        sendThread.join();
    }
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    isConnected = false;
}


// サーバーを開始してクライアントからの接続を待つ
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    BindAndListen(serverSocket);
    sockaddr_in serverAddr = {};
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);
    wcout << L"サーバーID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"接続を待っています..." << endl;

    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);
    wcout << L"接続が確立されました。" << endl;

    // ソケットを非ブロッキングモードに設定
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    // 通信開始
    StartCommunication(clientSocket);
}

void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    wstring id;
    wcout << L"接続先のサーバーIDを入力してください: ";
    getline(wcin, id);
    auto decodeID = DecodeAndReverseIPPort(WStringToUTF8(id));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(decodeID.second);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("接続に失敗しました。");
    }

    // ソケットを非ブロッキングモードに設定
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    wcout << L"接続しました。" << endl;
    StartCommunication(clientSocket);
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
            ReceiveData(string(buffer, bytesReceived));
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
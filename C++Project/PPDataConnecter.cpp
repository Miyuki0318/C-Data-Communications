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

#undef max  // 標準ライブラリの `max` を利用するために `max` マクロを解除
#pragma comment(lib, "Ws2_32.lib") // Winsock2ライブラリをリンク

PPDataConnecter* PPDataConnecter::instance = nullptr;
mutex PPDataConnecter::instanceMutex; // インスタンス生成時の排他制御用ミューテックス

// wstring（ワイド文字列）をUTF-8エンコードのstringに変換する関数
string WStringToUTF8(const wstring& wstr)
{
    if (wstr.empty()) return string(); // 空文字列の場合はそのまま返す

    // 必要なバッファサイズを取得
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string utf8_str(size_needed, 0); // 必要なサイズのバッファを確保
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // 末尾のnull終端文字を削除
    if (!utf8_str.empty() && utf8_str.back() == '\0')
    {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8エンコードのstringをwstringに変換する関数
wstring UTF8ToWString(const string& utf8Str)
{
    if (utf8Str.empty()) return wstring(); // 空文字列の場合はそのまま返す

    // 必要なバッファサイズを取得
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    wstring wide_str(size_needed, 0); // 必要なサイズのバッファを確保
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // 末尾のnull終端文字を削除
    if (!wide_str.empty() && wide_str.back() == L'\0')
    {
        wide_str.pop_back();
    }

    return wide_str;
}

// 10進数の数値を64進数表記の文字列に変換する関数
string ConvertToBase64(unsigned long number)
{
    const string base64chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"; // 64進数の文字セット

    string result;
    do {
        result = base64chars[number % 64] + result; // 下位桁から順に変換
        number /= 64;
    } while (number > 0);

    return result;
}

// 64進数表記の文字列を10進数の数値に変換する関数
unsigned long ConvertFromBase64(const string& base64Str)
{
    const string base64chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"; // 64進数の文字セット

    unsigned long result = 0;
    unsigned long base = 1;

    // 文字列の後ろ（最下位桁）から順に処理
    for (int i = base64Str.length() - 1; i >= 0; --i)
    {
        size_t pos = base64chars.find(base64Str[i]);
        if (pos == string::npos)
        {
            throw invalid_argument("Invalid base64 character."); // 不正な文字が含まれていた場合は例外を投げる
        }
        result += pos * base;
        base *= 64;
    }

    return result;
}

// PPDataConnecter クラスのコンストラクタ（メンバ変数を初期化）
PPDataConnecter::PPDataConnecter() :
    currentSocket(INVALID_SOCKET),
    isConnected(false),
    isWaiting(false),
    isCanceled(false)
{
}

// デストラクタ（終了処理を実行）
PPDataConnecter::~PPDataConnecter()
{
    Finalize();
}

// シングルトンパターンでPPDataConnecterのインスタンスを取得
PPDataConnecter* PPDataConnecter::GetNetworkPtr()
{
    lock_guard<mutex> lock(instanceMutex); // スレッドセーフのためミューテックスをロック
    if (!instance)
    {
        instance = new PPDataConnecter();
    }
    return instance;
}

// Winsockを初期化
void PPDataConnecter::Initialize()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw runtime_error("Winsockの初期化に失敗しました。");
    }
    isConnected = false;
}

// 通信終了処理
void PPDataConnecter::Finalize()
{
    StopCommunication(); // 通信停止処理
    isConnected = false;
    WSACleanup(); // Winsockの後始末
}

// コンソールの入出力をUTF-8対応に変更
void PPDataConnecter::SetConsoleToUnicode()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // コンソールフォントをMSゴシックに設定（日本語の表示を考慮）
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

// ソケットを作成する関数
SOCKET PPDataConnecter::CreateSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        throw runtime_error("ソケットの作成に失敗しました。");
    }
    return sock;
}

// サーバーソケットをバインドし、接続待ち状態にする
void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        throw runtime_error("ソケットの作成に失敗しました。");
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0); // OSにポートを自動割り当てさせる

    if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("バインドに失敗しました。");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        throw runtime_error("リッスンに失敗しました。");
    }
}

// クライアントからの接続を受け入れる
void PPDataConnecter::AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket)
{
    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET)
    {
        throw runtime_error("接続の受け入れに失敗しました。");
    }
}

// サーバーを非同期に起動する（接続待機とキャンセルを管理）
void PPDataConnecter::StartServerAsync(SOCKET& serverSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // サーバー処理を別スレッドで実行
    thread serverThread([&]()
        {
            try
            {
                StartServer(serverSocket, username);
                isConnected = true;
            }
            catch (const runtime_error& e)
            {
                wcout << L"サーバーの開始に失敗しました: " << UTF8ToWString(e.what()) << endl;
            }
            isWaiting = false;
        }
    );

    serverThread.detach(); // スレッドをデタッチ（バックグラウンド実行）
}

// サーバー接続をスレッドで開始
void PPDataConnecter::ConnectToServerAsync(SOCKET& clientSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // クライアント接続処理を非同期で実行
    thread clientThread([&]()
        {
            try
            {
                // サーバーに接続を試みる
                ConnectToServer(clientSocket, username);
                isConnected = true;
            }
            catch (const runtime_error& e)
            {
                // 接続エラーを表示
                wcout << L"サーバーへの接続に失敗しました: " << UTF8ToWString(e.what()) << endl;
            }
            isWaiting = false;
        }
    );

    // スレッドをデタッチ（別スレッドで処理を継続）
    clientThread.detach();
}

// 通信の開始
void PPDataConnecter::StartCommunication(SOCKET sock, const wstring& username)
{
    {
        lock_guard<mutex> lock(socketMutex);
        currentSocket = sock;
    }

    // 既存の通信スレッドがあれば停止
    StopCommunication();

    // 通信開始フラグを設定
    isConnected = true;

    // メッセージ送信スレッドを開始
    sendThread = thread([&]()
        {
            try
            {
                wstring message;

                while (isConnected)
                {
                    // ユーザーからの入力を取得
                    getline(wcin, message);
                    if (message == L"exit") break; // "exit" で終了

                    // サーバーにメッセージを送信
                    SendPPMessage(currentSocket, username, message);

                    // 切断確認
                    if (!isConnected)
                    {
                        break;
                    }
                }
            }
            catch (const runtime_error& e)
            {
                wcout << L"送信プロセスでエラーが発生しました : " << UTF8ToWString(e.what()) << endl;
            }
        }
    );

    // メッセージ受信スレッドを開始
    receiveThread = thread([&]()
        {
            try
            {
                BufferedData out;
                BufferedData temp;

                while (isConnected)
                {
                    // 受信データをバッファに追加
                    ReceivePPMessages(currentSocket);

                    // バッファからデータを取得
                    if (!GetFromRecvBuffer(out)) continue;

                    // 重複メッセージの防止
                    if (out.data != temp.data)
                    {
                        wcout << username + L":" << UTF8ToWString(out.data) << endl;
                        temp = out;
                    }

                    // 切断確認
                    if (!isConnected || out.data == "exit")
                    {
                        break;
                    }

                    // データをリセット
                    out = BufferedData();
                }
            }
            catch (const runtime_error& e)
            {
                wcout << L"受信プロセスでエラーが発生しました : " << UTF8ToWString(e.what()) << endl;
            }
        }
    );
}

// 待機状態のキャンセル処理
void PPDataConnecter::CancelCommunication()
{
    if (isWaiting)
    {
        // キャンセルフラグを立てる
        isCanceled = true;
        isWaiting = false;
    }
}

// 通信の停止
void PPDataConnecter::StopCommunication()
{
    // 送信スレッドの終了待機
    if (sendThread.joinable())
    {
        sendThread.join();
    }
    // 受信スレッドの終了待機
    if (receiveThread.joinable())
    {
        receiveThread.join();
    }

    // 接続状態を解除
    isConnected = false;
}

// サーバーを開始してクライアントからの接続を待つ
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    // サーバーをバインドしてリスニングを開始
    BindAndListen(serverSocket);

    // サーバーのIPアドレスとポートを取得
    sockaddr_in serverAddr = {};
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);

    // サーバー情報を表示
    wcout << L"サーバーID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"接続を待っています..." << endl;

    // クライアント接続待機
    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);
    wcout << L"接続が確立されました。" << endl;

    // ソケットを非ブロッキングモードに設定
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    // 通信開始
    StartCommunication(clientSocket, username);
}

// クライアントがサーバーに接続する
void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    wstring id;
    wcout << L"接続先のサーバーIDを入力してください: ";
    getline(wcin, id);

    // サーバーIDをデコードしてIPアドレスとポート番号を取得
    auto decodeID = DecodeAndReverseIPPort(WStringToUTF8(id));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(decodeID.second);

    // サーバーへ接続
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("接続に失敗しました。");
    }

    // ソケットを非ブロッキングモードに設定
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    wcout << L"接続しました。" << endl;

    // 通信開始
    StartCommunication(clientSocket, username);
}

// 送信バッファにデータを追加
void PPDataConnecter::AddToSendBuffer(const string& header, const string& data)
{
    lock_guard<mutex> lock(sendMutex);
    sendBuffer.push_back({ header, data });
}

// 受信バッファからデータを取得
bool PPDataConnecter::GetFromRecvBuffer(BufferedData& outData)
{
    lock_guard<mutex> lock(recvMutex);

    // バッファが空なら取得できない
    if (recvBuffer.empty()) return false;

    // 最古のデータを取得
    outData = recvBuffer.front();
    recvBuffer.pop_front();
    return true;
}

// 受信バッファから特定のヘッダのデータを取得
bool PPDataConnecter::GetFromRecvBufferByHeader(const string& header, BufferedData& outData)
{
    lock_guard<mutex> lock(recvMutex);

    // 指定ヘッダのデータを検索
    for (auto it = recvBuffer.begin(); it != recvBuffer.end(); ++it)
    {
        if (it->header == header)
        {
            // データを取得して削除
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
    // ユーザー名とメッセージを結合
    wstring fullMessage = username + L": " + message;
    string utf8Message = WStringToUTF8(fullMessage);

    // 送信データの長さが整数の最大値を超えないようにチェック
    if (utf8Message.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("送信メッセージが大きすぎます。");
    }

    // サーバーへデータを送信
    send(sock, utf8Message.c_str(), static_cast<int>(utf8Message.length()), 0);
}

// メッセージを受信
void PPDataConnecter::ReceivePPMessages(SOCKET socket)
{
    char buffer[BUFFER_SIZE];

    // 受信処理のループ
    while (true)
    {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0'; // 受信データをnull終端
            ReceiveData(string(buffer, bytesReceived)); // 受信データを処理
        }
        else
        {
            break; // 通信が終了またはエラー発生時にループを抜ける
        }
    }
}

// バッファからデータを取得し送信する
void PPDataConnecter::SendData()
{
    lock_guard<mutex> lock(sendMutex);
    if (sendBuffer.empty()) return;

    // 送信バッファの先頭データを取得
    BufferedData data = sendBuffer.front();
    sendBuffer.pop_front();

    // データを送信可能な形式にシリアライズ
    string rawData = Serialize(data);

    // 送信データの長さチェック
    if (rawData.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("送信メッセージが大きすぎます。");
    }

    // データをソケット経由で送信
    send(currentSocket, rawData.c_str(), static_cast<int>(rawData.length()), 0);
}

// 受信データを処理
void PPDataConnecter::ReceiveData(const string& rawData)
{
    // 受信データを解析して構造化
    BufferedData data = Deserialize(rawData);

    // 受信バッファに追加
    lock_guard<mutex> lock(recvMutex);
    recvBuffer.push_back(data);
}

// ローカルIPアドレスを取得
string PPDataConnecter::GetLocalIPAddress()
{
    char hostName[256];

    // ホスト名を取得
    gethostname(hostName, sizeof(hostName));

    // IPv4アドレス情報を取得
    addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostName, nullptr, &hints, &result);

    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];

    // バイナリIPを文字列に変換
    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);

    freeaddrinfo(result);
    return string(ipAddress);
}

// ローカルIPアドレスをワイド文字列で取得
wstring PPDataConnecter::GetLocalIPAddressW()
{
    return UTF8ToWString(GetLocalIPAddress());
}

// IPアドレスとポートをエンコードし、反転
string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // IPアドレスをバイナリ形式に変換
    sockaddr_in sa = {};
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1)
    {
        throw invalid_argument("Invalid IP address.");
    }

    // IPアドレスを64進数に変換
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ip64Stream;
    ip64Stream << uppercase << ConvertToBase64(ntohl(ip));
    string ip64 = ip64Stream.str();

    // ポート番号を64進数に変換
    ostringstream port64Stream;
    port64Stream << uppercase << ConvertToBase64(port);
    string port64 = port64Stream.str();

    // IPとポートを結合し、反転
    string combined = ip64 + "-" + port64;
    reverse(combined.begin(), combined.end());

    return combined;
}

// エンコードされたIPアドレスとポートをデコードし、復元
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

    // 64進数からIPアドレスを復元
    unsigned long ipNum = ConvertFromBase64(ip64);
    ipNum = htonl(ipNum);

    // バイナリIPを文字列に変換
    char ipAddress[INET_ADDRSTRLEN];
    in_addr ipAddr = {};
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr)
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // 64進数からポート番号を復元
    unsigned short port = ConvertFromBase64(port64);

    return { ipAddress, port };
}

// データを "ヘッダ:データ" の形式でシリアライズ
string PPDataConnecter::Serialize(const BufferedData& data)
{
    return data.header + ":" + data.data;
}

// 受信データをパース
BufferedData PPDataConnecter::Deserialize(const string& rawData)
{
    size_t pos = rawData.find(':');

    // ヘッダがない場合は "Unknown" とする
    if (pos == string::npos)
    {
        return { "Unknown", rawData };
    }

    // ヘッダとデータを分割して構造体に格納
    return { rawData.substr(0, pos), rawData.substr(pos + 1) };
}
#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <condition_variable>

using namespace std;

// wstringとUTF-8の相互変換を行うユーティリティ関数
string WStringToUTF8(const wstring& wstr); // wstringをUTF-8形式のstringに変換
wstring UTF8ToWString(const string& utf8Str); // UTF-8形式のstringをwstringに変換

// 10進数から64進数に変換する関数
string ConvertToBase64(unsigned long number);
unsigned long ConvertFromBase64(const string& base64Str);

// データバッファのための構造体
struct BufferedData
{
    string header;  // データの種類を識別するヘッダー
    string data;    // 実際のデータ
};

// PPDataConnecterクラスは、データ通信やソケット管理を行う
class PPDataConnecter
{
private:

    // バッファサイズ
    static const int BUFFER_SIZE = 4096;

    // シングルトンインスタンス
    static PPDataConnecter* instance;
    static mutex instanceMutex;

    // 非同期通信用のメンバ
    condition_variable waitCondition;  // 通信待機時に使用

    // 非同期通信のスレッド
    thread sendThread;
    thread receiveThread;

    mutex socketMutex;  // ソケットに関する排他制御

    deque<BufferedData> sendBuffer;
    deque<BufferedData> recvBuffer;
    mutex sendMutex;
    mutex recvMutex;

    SOCKET currentSocket;

public:

    atomic<bool> isWaiting;  // 通信待機状態
    atomic<bool> isCanceled; // 通信キャンセル状態
    atomic<bool> isConnected; // 接続状態

    // コンストラクタとデストラクタ
    PPDataConnecter(); // クラスの初期化
    ~PPDataConnecter(); // クラスの終了処理

    // シングルトンアクセス
    static PPDataConnecter* GetNetworkPtr();

    // 初期化と終了
    void Initialize();
    void Finalize();

    // コンソールをUnicode（UTF-8）に設定
    void SetConsoleToUnicode();

    // 新しいソケットを作成
    SOCKET CreateSocket();

    // サーバーを開始し、クライアントからの接続を待機
    void StartServer(SOCKET& socket, const wstring& username);

    // サーバーに接続して通信を開始
    void ConnectToServer(SOCKET& socket, const wstring& username);

    // メッセージを送信する静的メソッド
    void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message);

    // メッセージを受信する静的メソッド
    void ReceivePPMessages(SOCKET socket);

    // スレッド非同期通信関連
    void StartCommunication(SOCKET sock);
    void CancelCommunication();
    void StopCommunication();
    void Communication(SOCKET sock);
    void BindAndListen(SOCKET& serverSocket);
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);
    void StartServerAsync(SOCKET& serverSocket, const wstring& username); // サーバースレッドの開始
    void ConnectToServerAsync(SOCKET& clientSocket, const wstring& username); // クライアントスレッドの開始

    // 送信バッファにデータを追加
    void AddToSendBuffer(const std::string& header, const std::string& data);

    // 受信バッファからデータを取得
    bool GetFromRecvBuffer(BufferedData& outData);
    bool GetFromRecvBufferByHeader(const string& header, BufferedData& outData);

    // データ送受信の処理
    void SendData();  // バッファから送信
    void ReceiveData(const std::string& rawData);  // 受信データを処理

    // ローカルIPアドレスを取得する静的メソッド
    static string GetLocalIPAddress();
    static wstring GetLocalIPAddressW();

    // IPアドレスとポート番号をサーバーIDに変換する静的メソッド
    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port);

    // サーバーIDをIPアドレスとポート番号に変換する静的メソッド
    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed);

private:

    // 内部用: 送信データのフォーマット変換
    static string Serialize(const BufferedData& data);
    static BufferedData Deserialize(const std::string& rawData);
};

#endif

#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

// wstringとUTF-8の相互変換を行うユーティリティ関数
string WStringToUTF8(const wstring& wstr); // wstringをUTF-8形式のstringに変換
wstring UTF8ToWString(const string& utf8Str); // UTF-8形式のstringをwstringに変換

// PPDataConnecterクラスは、データ通信やソケット管理を行う
class PPDataConnecter
{
public:
    // コンストラクタとデストラクタ
    PPDataConnecter(); // クラスの初期化
    ~PPDataConnecter(); // クラスの終了処理

    // Winsockの初期化
    void InitializeWinsock();

    // コンソールをUnicode（UTF-8）に設定
    void SetConsoleToUnicode();

    // 新しいソケットを作成
    SOCKET CreateSocket();

    // サーバーを開始し、クライアントからの接続を待機
    void StartServer(SOCKET& serverSocket, const wstring& username);

    // サーバーに接続して通信を開始
    void ConnectToServer(SOCKET& clientSocket, const wstring& username);

    // メッセージを送信する静的メソッド
    static void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message);

    // メッセージを受信する静的メソッド
    static void ReceivePPMessages(SOCKET socket);

    // ローカルIPアドレスを取得する静的メソッド
    static string GetLocalIPAddress();
    static wstring GetLocalIPAddressW();

    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port);

    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed);

private:
    // サーバーソケットをバインドしてリッスン状態にする
    void BindAndListen(SOCKET& serverSocket);

    // クライアントの接続を受け入れる
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);
};

#endif

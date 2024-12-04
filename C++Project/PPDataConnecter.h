#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <fstream>
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

    // サーバーを開始し、クライアントからの接続を待機し、ファイルの送受信を行う
    void StartFileTransServer(SOCKET& socket, const std::wstring& username);

    // サーバーに接続して通信を開始し、ファイルの送受信を行う
    void ConnectToFileTransServer(SOCKET& socket, const std::wstring& username);

    // メッセージを送信する静的メソッド
    static void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message);

    // メッセージを受信する静的メソッド
    static void ReceivePPMessages(SOCKET socket);
    
    // ファイルを送信する静的メソッド
    static void SendFile(SOCKET& socket, const wstring& filename, const wstring& fileContent);
    
    // ファイルを受信する静的メソッド
    static void ReceiveFile(SOCKET& clientSocket, wstring& receivedFile);

    // ローカルIPアドレスを取得する静的メソッド
    static string GetLocalIPAddress();
    static wstring GetLocalIPAddressW();

    // IPアドレスとポート番号をサーバーIDに変換する静的メソッド
    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port);

    // サーバーIDをIPアドレスとポート番号に変換する静的メソッド
    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed);

private:

    // サーバーソケットをバインドしてリッスン状態にする
    void BindAndListen(SOCKET& serverSocket);

    // クライアントの接続を受け入れる
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);

    // 文字列をファイルに保存する
    static void SaveString(SOCKET& socket, const wstring& str);

    // ファイルから文字列を取得する
    static void ReadString(SOCKET& socket, string& str);

    // logフォルダ作成関数
    static void CreateLogDirectoryIfNotExist();
};

#endif

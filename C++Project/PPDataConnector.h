#ifndef PPDATACONNECTOR_H
#define PPDATACONNECTOR_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") // Winsockライブラリをリンクする

using namespace std; // std:: 名前空間を省略する

// バッファサイズを定義する
const int BUFFER_SIZE = 4096;

// PPDataConnectorクラスの定義
// このクラスは、ソケット通信を管理するクラスである
class PPDataConnector {
private:
    SOCKET socket_;         // ソケットオブジェクト
    bool isHost_;           // ホストかクライアントかを示すフラグ
    wstring ipAddress_;     // 接続先のIPアドレス
    int port_;              // 使用するポート番号

public:
    // ホスト用コンストラクタ
    PPDataConnector(int port);

    // クライアント用コンストラクタ
    PPDataConnector(int port, const wstring& ipAddress);

    // デストラクタ
    ~PPDataConnector();

    // メッセージを送信する関数
    void SendPPMessage(const wstring& message);

    // メッセージを受信する関数
    wstring ReceiveMessage();

    // ソケットオブジェクトを取得する関数
    SOCKET GetSocket() const;

    // ホストのIPアドレスを取得する関数
    static wstring GetHostIP();

    // ソケット通信を開始する関数
    void Start() const;
};

// wstringをstringに変換する関数
string wstring_to_string(const wstring& wstr);

// stringをwstringに変換する関数
wstring string_to_wstring(const string& str);

#endif // PPDATACONNECTOR_H

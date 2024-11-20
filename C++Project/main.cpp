#include <iostream>
#include <string>
#include <fcntl.h>
#include <io.h>
#include "PPDataConnector.h"

using namespace std;

// コンソールのエンコーディングをUTF-16に設定する関数
static void SetUTF16Console()
{
    if (_setmode(_fileno(stdin), _O_U16TEXT) == -1)
    {
        wcerr << L"標準入力のエンコーディング設定に失敗しました。" << endl;
    }

    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1)
    {
        wcerr << L"標準出力のエンコーディング設定に失敗しました。" << endl;
    }

    if (_setmode(_fileno(stderr), _O_U16TEXT) == -1)
    {
        wcerr << L"標準エラー出力のエンコーディング設定に失敗しました。" << endl;
    }
}

// チャットを実行する関数
static void RunChat(PPDataConnector& connector, const wstring& username)
{
    wstring message;
    while (true)
    {
        // メッセージの入力
        getline(wcin, message);
        if (message == L"exit") break;

        wstring fullMessage = username + L": " + message;
        connector.SendPPMessage(fullMessage); // メッセージ送信

        // メッセージ受信
        wstring receivedMessage = connector.ReceiveMessage();
        if (!receivedMessage.empty())
        {
            wcout << L"受信メッセージ: " << receivedMessage << endl;
        }
    }
}

int main()
{
    SetUTF16Console(); // コンソールのエンコーディングをUTF-16に設定

    wstring username;
    wcout << L"ユーザー名を入力してください: ";
    getline(wcin, username); // ユーザー名の入力

    wstring mode;
    wcout << L"ホストとしてチャットを開始しますか？ (Y/N): ";
    getline(wcin, mode); // モードの入力

    // ホストとしてチャットを開始
    if (mode == L"Y" || mode == L"y")
    {
        wstring portInput;
        wcout << L"開設するポート番号を入力してください: ";
        getline(wcin, portInput); // ポート番号を文字列として入力
        int port = stoi(portInput); // 文字列を整数に変換
        wcout << L"自身のIPアドレスは: " << PPDataConnector::GetHostIP() << endl;

        PPDataConnector connector(port); // コネクタの作成

        connector.Start(); // ホストモード開始
        wcout << L"ホストモードでチャットを開始します..." << endl;

        RunChat(connector, username); // チャットの実行
    }
    else
    {
        wstring ipAddress;
        wcout << L"接続先のIPアドレスを入力してください: ";
        getline(wcin, ipAddress); // IPアドレスの入力

        wstring portInput;
        wcout << L"接続先のポート番号を入力してください: ";
        getline(wcin, portInput); // ポート番号を文字列として入力
        int port = stoi(portInput); // 文字列を整数に変換

        PPDataConnector connector(port, ipAddress); // コネクタの作成

        wcout << L"クライアントとして接続します..." << endl;

        RunChat(connector, username); // チャットの実行
    }

    return 0;
}

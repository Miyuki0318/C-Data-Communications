#include "PPDataConnecter.h"
#include <iostream>
#include <string>

using namespace std;

int main()
{
    try
    {
        // PPDataConnecterクラスのインスタンスを生成
        PPDataConnecter connecter;

        // コンソールをUnicodeに設定（UTF-8対応）
        connecter.SetConsoleToUnicode();

        // Winsockライブラリを初期化
        connecter.Initialize();

        // ソケットを作成
        SOCKET socket = connecter.CreateSocket();

        // ユーザー名を入力
        wstring username;
        wcout << L"ユーザー名を入力してください: ";
        getline(wcin, username);

        // サーバー作成モードかクライアントモードかを選択
        wstring mode;
        wcout << L"チャットルームを作成しますか？ (Y/N): ";
        getline(wcin, mode);

        // サーバーモードの場合
        if (mode == L"Y" || mode == L"y")
        {
            connecter.StartServer(); // サーバーを起動
        }
        // クライアントモードの場合
        else if (mode == L"N" || mode == L"n")
        {
            connecter.ConnectToServer(); // サーバーに接続
        }
        else
        {
            // 無効な選択肢が入力された場合
            wcout << L"無効な選択です。" << endl;
        }

        while (connecter.isRunning || connecter.isConnecting)
        {
            if (!connecter.isRunning)
            {
                break;
            }
        }

        // ソケットを閉じる
        closesocket(socket);
    }
    catch (const exception& ex)
    {
        // 例外発生時のエラーメッセージを表示
        wcout << L"エラー: " << UTF8ToWString(ex.what()) << endl;
    }

    return 0;
}

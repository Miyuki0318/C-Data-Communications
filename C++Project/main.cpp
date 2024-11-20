#include "PPDataConnecter.h"
#include <iostream>
#include <string>

int main() {
    try {
        PPDataConnecter connecter;
        connecter.SetConsoleToUnicode();
        connecter.InitializeWinsock();
        SOCKET socket = connecter.CreateSocket();

        std::wstring username;
        std::wcout << L"ユーザー名を入力してください: ";
        std::getline(std::wcin, username);

        std::wstring mode;
        std::wcout << L"チャットルームを作成しますか？ (Y/N): ";
        std::getline(std::wcin, mode);

        if (mode == L"Y" || mode == L"y") {
            connecter.StartServer(socket, username);
        }
        else if (mode == L"N" || mode == L"n") {
            connecter.ConnectToServer(socket, username);
        }
        else {
            std::wcout << L"無効な選択です。" << std::endl;
        }

        closesocket(socket);
    }
    catch (const std::exception& ex) {
        std::wcout << L"エラー: " << UTF8ToWString(ex.what()) << std::endl;
    }

    return 0;
}

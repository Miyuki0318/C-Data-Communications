#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

// wstringからUTF-8に変換する関数（改良版）
std::string WStringToUTF8(const std::wstring& wstr);

// UTF-8からwstringに変換する関数（改良版）
std::wstring UTF8ToWString(const std::string& utf8Str);

class PPDataConnecter {
public:
    PPDataConnecter();
    ~PPDataConnecter();

    void InitializeWinsock();
    void SetConsoleToUnicode();
    SOCKET CreateSocket();
    void StartServer(SOCKET& serverSocket, const std::wstring& username);
    void ConnectToServer(SOCKET& clientSocket, const std::wstring& username);
    static void SendMessage(SOCKET sock, const std::wstring& username, const std::wstring& message);
    static void ReceiveMessages(SOCKET socket);
    static std::wstring GetLocalIPAddress();

private:
    void BindAndListen(SOCKET& serverSocket);
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);
};

#endif

#include "PPDataConnector.h"
#include <iostream>
#include <thread>
#include <codecvt>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") // Winsock���C�u�����������N����

using namespace std; // std:: ���O��Ԃ��ȗ�����

// �R���X�g���N�^
PPDataConnector::PPDataConnector(int port)
    : port_(port), isHost_(true)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        wcout << L"Winsock�̏������Ɏ��s���܂����B" << endl;
        exit(1);
    }

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET)
    {
        wcout << L"�\�P�b�g�̍쐬�Ɏ��s���܂����B" << endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        wcout << L"�o�C���h�Ɏ��s���܂����B" << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }

    if (listen(socket_, 1) == SOCKET_ERROR)
    {
        wcout << L"���b�X���Ɏ��s���܂����B" << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }
}

// �R���X�g���N�^�i�N���C�A���g�p�j
PPDataConnector::PPDataConnector(int port, const wstring& ipAddress)
    : port_(port), isHost_(false), ipAddress_(ipAddress)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        wcout << L"Winsock�̏������Ɏ��s���܂����B" << endl;
        exit(1);
    }

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET)
    {
        wcout << L"�\�P�b�g�̍쐬�Ɏ��s���܂����B" << endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    wcout << L"�z�X�g�ɐڑ���..." << endl;
    inet_pton(AF_INET, wstring_to_string(ipAddress_).c_str(), &addr.sin_addr);
    if (connect(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int errCode = WSAGetLastError();
        wcout << L"�ڑ��Ɏ��s���܂����B�G���[�R�[�h: " << errCode << endl;
        closesocket(socket_);
        WSACleanup();
        exit(1);
    }
}

// �f�X�g���N�^
PPDataConnector::~PPDataConnector()
{
    closesocket(socket_);
    WSACleanup();
}

// ���b�Z�[�W�𑗐M����֐��iUTF-16�j
void PPDataConnector::SendPPMessage(const wstring& message)
{
    // ���M���b�Z�[�W��UTF-16�ɕϊ�
    int messageLength = message.length() * sizeof(wchar_t); // UTF-16�o�C�g��
    send(socket_, reinterpret_cast<const char*>(&messageLength), sizeof(messageLength), 0); // ���b�Z�[�W���𑗐M

    // ���b�Z�[�W���e�𑗐M
    send(socket_, reinterpret_cast<const char*>(message.c_str()), messageLength, 0);
}

// ���b�Z�[�W����M����֐��iUTF-16�j
wstring PPDataConnector::ReceiveMessage()
{
    int messageLength = 0;

    // ���b�Z�[�W�̒�������M
    int bytesReceived = recv(socket_, reinterpret_cast<char*>(&messageLength), sizeof(messageLength), 0);
    if (bytesReceived <= 0) return L""; // ��M���s�܂��͐ڑ��؂�

    // ���b�Z�[�W���e����M
    char* buffer = new char[messageLength];
    bytesReceived = recv(socket_, buffer, messageLength, 0);
    if (bytesReceived <= 0) {
        delete[] buffer;
        return L"��M���s"; // ��M���s
    }

    // ��M�����o�C�i���f�[�^��UTF-16�ɕϊ�
    wstring message(reinterpret_cast<wchar_t*>(buffer), messageLength / sizeof(wchar_t));
    delete[] buffer;

    return message;
}

// �\�P�b�g�I�u�W�F�N�g���擾����֐�
SOCKET PPDataConnector::GetSocket() const
{
    return socket_;
}

// �z�X�g��IP�A�h���X���擾���郁�\�b�h
wstring PPDataConnector::GetHostIP()
{
    // Winsock�̏�����
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return L"IP�A�h���X�̎擾�Ɏ��s���܂����B";
    }

    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) != 0)
    {
        WSACleanup();
        return L"�z�X�g���̎擾�Ɏ��s���܂����B";
    }

    struct addrinfo* result = nullptr;
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4�A�h���X

    // �z�X�g������IP�A�h���X���擾
    if (getaddrinfo(hostName, nullptr, &hints, &result) != 0)
    {
        WSACleanup();
        return L"IP�A�h���X�̎擾�Ɏ��s���܂����B";
    }

    std::wstring ipAddress = L"";
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ip, sizeof(ip));
        ipAddress = std::wstring(ip, ip + strlen(ip));
        break; // �ŏ���IP�A�h���X���擾
    }

    freeaddrinfo(result);
    WSACleanup();

    return ipAddress;
}

// �\�P�b�g�ʐM���J�n����֐�
void PPDataConnector::Start() const
{
    if (isHost_)
    {
        sockaddr_in clientAddr;
        int addrSize = sizeof(clientAddr);

        wcout << L"�ڑ����E�E�E" << endl;

        // �N���C�A���g�ڑ���ҋ@
        SOCKET clientSocket = accept(socket_, (sockaddr*)&clientAddr, &addrSize);

        if (clientSocket != INVALID_SOCKET)
        {
            wcout << L"�ڑ����m������܂����B" << endl;

            // �V�����X���b�h�Ń��b�Z�[�W�̎�M���J�n
            thread([clientSocket]()
                {
                    char buffer[BUFFER_SIZE];
                    while (true)
                    {
                        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (bytesReceived > 0)
                        {
                            buffer[bytesReceived] = '\0';
                            wcout << L"���b�Z�[�W��M: " << string_to_wstring(buffer) << endl;
                        }
                        else
                        {
                            break; // ��M���s���̓��[�v���I��
                        }
                    }
                }).detach();
        }
        else
        {
            wcout << L"�ڑ����s" << endl;
        }
    }
}

// wstring����string�ւ̕ϊ��֐��iUTF-16�Ή��j
string wstring_to_string(const wstring& wstr)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
        return str;
    }
    return "";
}

// string����wstring�ւ̕ϊ��֐��iUTF-16�Ή��j
wstring string_to_wstring(const string& str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0)
    {
        wstring wstr(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }
    return L"";
}

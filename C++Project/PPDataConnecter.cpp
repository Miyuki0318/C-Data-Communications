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
#include <ws2tcpip.h>

#undef max  // �^�̍ő�p��max���g���ׂ�undef
#pragma comment(lib, "Ws2_32.lib") // Winsock2���C�u�����̃����N�w��

const int BUFFER_SIZE = 4096; // ��M�o�b�t�@�̃T�C�Y���`

// wstring����UTF-8�ɕϊ�����֐��i���ǔŁj
string WStringToUTF8(const wstring& wstr)
{
    if (wstr.empty()) return string(); // �󕶎���̏ꍇ�͂��̂܂ܕԂ�

    // �K�v�ȃo�b�t�@�T�C�Y���擾
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string utf8_str(size_needed, 0); // �K�v�ȃT�C�Y�ŕ������������
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // null�I�[�������폜
    if (!utf8_str.empty() && utf8_str.back() == '\0')
    {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8����wstring�ɕϊ�����֐��i���ǔŁj
wstring UTF8ToWString(const string& utf8Str)
{
    if (utf8Str.empty()) return wstring(); // �󕶎���̏ꍇ�͂��̂܂ܕԂ�

    // �K�v�ȃo�b�t�@�T�C�Y���擾
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    wstring wide_str(size_needed, 0); // �K�v�ȃT�C�Y�ŕ������������
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // null�I�[�������폜
    if (!wide_str.empty() && wide_str.back() == L'\0')
    {
        wide_str.pop_back();
    }

    return wide_str;
}

// �R���X�g���N�^�F���ɏ������͍s��Ȃ�
PPDataConnecter::PPDataConnecter() {}

// �f�X�g���N�^�FWinsock�̃N���[���A�b�v���s��
PPDataConnecter::~PPDataConnecter()
{
    WSACleanup();
}

// Winsock�̏�����
void PPDataConnecter::InitializeWinsock()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
    {
        throw runtime_error("Winsock�̏������Ɏ��s���܂����B");
    }
}

// �R���\�[����UTF-8�Ή��ɐݒ�
void PPDataConnecter::SetConsoleToUnicode()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // �R���\�[���t�H���g��MS�S�V�b�N�ɐݒ�
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"MS Gothic");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);
}

// �\�P�b�g���쐬
SOCKET PPDataConnecter::CreateSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        throw runtime_error("�\�P�b�g�̍쐬�Ɏ��s���܂����B");
    }
    return sock;
}

// �\�P�b�g���o�C���h���A���b�X�����J�n
void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // �S�Ẵ��[�J��IP�A�h���X���o�C���h
    serverAddr.sin_port = htons(0); // �C�ӂ̃|�[�g�ԍ����g�p

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("�o�C���h�Ɏ��s���܂����B");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        throw runtime_error("���b�X���Ɏ��s���܂����B");
    }
}

// �ڑ����󂯓����
void PPDataConnecter::AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket)
{
    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET)
    {
        throw runtime_error("�ڑ��̎󂯓���Ɏ��s���܂����B");
    }
}

// �T�[�o�[���J�n���ăN���C�A���g����̐ڑ���҂�
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    BindAndListen(serverSocket);

    sockaddr_in serverAddr;
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen); // �o�C���h���ꂽ�|�[�g�ԍ����擾
    wcout << L"�T�[�o�[ID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"�ڑ���҂��Ă��܂�..." << endl;

    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);

    wcout << L"�ڑ����m������܂����B" << endl;
    thread(ReceivePPMessages, clientSocket).detach(); // �ʃX���b�h�Ń��b�Z�[�W��M���J�n

    wstring message;
    while (true)
    {
        getline(wcin, message);
        if (message == L"exit") break; // exit�ŏI��
        SendPPMessage(clientSocket, username, message);
    }

    closesocket(clientSocket);
}

// �N���C�A���g�Ƃ��ăT�[�o�[�ɐڑ�
void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    string id;
    wcout << L"�ڑ���̃T�[�o�[ID����͂��Ă�������: ";
    getline(cin, id);
    auto decodeID = DecodeAndReverseIPPort(id);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr); // IP�A�h���X���o�C�i���`���ɕϊ�
    serverAddr.sin_port = htons(decodeID.second);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("�ڑ��Ɏ��s���܂����B");
    }

    wcout << L"�ڑ����܂����B" << endl;
    thread(ReceivePPMessages, clientSocket).detach(); // �ʃX���b�h�Ń��b�Z�[�W��M���J�n

    wstring message;
    while (true)
    {
        getline(wcin, message);
        if (message == L"exit") break;
        SendPPMessage(clientSocket, username, message);
    }
}

// ���b�Z�[�W�𑗐M
void PPDataConnecter::SendPPMessage(SOCKET sock, const wstring& username, const wstring& message)
{
    wstring fullMessage = username + L": " + message; // ���[�U�[���ƃ��b�Z�[�W������
    string utf8Message = WStringToUTF8(fullMessage);
    
    // �����`�F�b�N
    if (utf8Message.length() > static_cast<size_t>(std::numeric_limits<int>::max())) 
    {
        throw std::runtime_error("���M���b�Z�[�W���傫�����܂��B");
    }

    send(sock, utf8Message.c_str(), static_cast<int>(utf8Message.length()), 0);
}

// ���b�Z�[�W����M
void PPDataConnecter::ReceivePPMessages(SOCKET socket)
{
    char buffer[BUFFER_SIZE];
    while (true)
    {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0'; // ��M�f�[�^��null�I�[
            wcout << UTF8ToWString(string(buffer, bytesReceived)) << endl;
        }
        else
        {
            break; // �ڑ�������ꂽ�ꍇ
        }
    }
}

// ���[�J��IP�A�h���X���擾
string PPDataConnecter::GetLocalIPAddress()
{
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostName, nullptr, &hints, &result);

    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);

    freeaddrinfo(result);
    return string(ipAddress);
}

// ���[�J��IP�A�h���X���擾
wstring PPDataConnecter::GetLocalIPAddressW()
{
    return UTF8ToWString(GetLocalIPAddress());
}

// �G���R�[�h���Ĕ��]����֐�
string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // inet_pton���g�p����IP�A�h���X���o�C�i���`���ɕϊ�
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1) 
    {
        throw invalid_argument("Invalid IP address.");
    }

    // �o�C�i���`������16�i���ɕϊ�
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ipHexStream;
    ipHexStream << uppercase << hex << setw(8) << setfill('0') << ntohl(ip);
    string ipHex = ipHexStream.str();

    // �|�[�g�ԍ���16�i���ɕϊ�
    ostringstream portHexStream;
    portHexStream << uppercase << hex << setw(4) << setfill('0') << port;
    string portHex = portHexStream.str();

    // IP�ƃ|�[�g������
    string combined = ipHex + "-" + portHex;

    // ������𔽓]
    reverse(combined.begin(), combined.end());

    return combined;
}

// ���]���ăf�R�[�h����֐�
pair<string, unsigned short> PPDataConnecter::DecodeAndReverseIPPort(const string& encodedReversed)
{
    // ������𔽓]
    string combined = encodedReversed;
    reverse(combined.begin(), combined.end());

    // IP�ƃ|�[�g�𕪗�
    size_t dashPos = combined.find('-');
    if (dashPos == string::npos) 
    {
        throw invalid_argument("Invalid encoded string format.");
    }

    string ipHex = combined.substr(0, dashPos);
    string portHex = combined.substr(dashPos + 1);

    // IP�𕜌�
    unsigned long ipNum = stoul(ipHex, nullptr, 16);
    ipNum = htonl(ipNum); // �l�b�g���[�N�o�C�g�I�[�_�[�ɕϊ�

    // inet_ntop ���g�p���ăo�C�i�����當����IP�A�h���X�ɕϊ�
    char ipAddress[INET_ADDRSTRLEN];
    struct in_addr ipAddr;
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr) 
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // �|�[�g�𕜌�
    unsigned short port = static_cast<unsigned short>(stoul(portHex, nullptr, 16));

    return { ipAddress, port };
}
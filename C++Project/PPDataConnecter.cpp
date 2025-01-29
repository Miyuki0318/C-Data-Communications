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
#include <winsock2.h>
#include <ws2tcpip.h>

#undef max  // �^�̍ő�p��max���g���ׂ�undef
#pragma comment(lib, "Ws2_32.lib") // Winsock2���C�u�����̃����N�w��

PPDataConnecter* PPDataConnecter::instance = nullptr;
mutex PPDataConnecter::instanceMutex;

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

// 10�i������64�i���ɕϊ�����֐�
string ConvertToBase64(unsigned long number) {
    const string base64chars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

    string result;
    do {
        result = base64chars[number % 64] + result;
        number /= 64;
    } while (number > 0);

    return result;
}

// 64�i������10�i���ɕϊ�����֐�
unsigned long ConvertFromBase64(const string& base64Str) {
    const string base64chars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

    unsigned long result = 0;
    unsigned long base = 1;

    for (int i = base64Str.length() - 1; i >= 0; --i) {
        size_t pos = base64chars.find(base64Str[i]);
        if (pos == string::npos) {
            throw invalid_argument("Invalid base64 character.");
        }
        result += pos * base;
        base *= 64;
    }

    return result;
}

PPDataConnecter::PPDataConnecter() : isConnected(false), isWaiting(false), isCanceled(false) {}

PPDataConnecter::~PPDataConnecter() {
    Finalize();
}

PPDataConnecter* PPDataConnecter::GetNetworkPtr()
{
    lock_guard<mutex> lock(instanceMutex);
    if (!instance)
    {
        instance = new PPDataConnecter();
    }
    return instance;
}

void PPDataConnecter::Initialize() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw runtime_error("Winsock�̏������Ɏ��s���܂����B");
    }
    isConnected = false;
}

void PPDataConnecter::Finalize() {
    StopCommunication();
    isConnected = false;
    WSACleanup();
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
    CONSOLE_FONT_INFOEX cfi = {};
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

void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        throw runtime_error("�\�P�b�g�̍쐬�Ɏ��s���܂����B");
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0);

    if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw runtime_error("�o�C���h�Ɏ��s���܂����B");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
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

// �ʐM�ҋ@�ƃL�����Z����Ԃ��Ǘ�
void PPDataConnecter::StartServerAsync(SOCKET& serverSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // �T�[�o�[�J�n���X���b�h�Ŏ��s
    thread serverThread([&]() {
        try {
            // �T�[�o�[�J�n����
            StartServer(serverSocket, username);
            isConnected = true;
        }
        catch (const runtime_error& e) {
            wcout << L"�T�[�o�[�̊J�n�Ɏ��s���܂���: " << UTF8ToWString(e.what()) << endl;
        }
        isWaiting = false;
        });

    serverThread.detach();
}

// �T�[�o�[�ڑ����X���b�h�ŊJ�n
void PPDataConnecter::ConnectToServerAsync(SOCKET& clientSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // �N���C�A���g�ڑ�������񓯊��Ŏ��s
    thread clientThread([&]() {
        try {
            // �T�[�o�[�ڑ�����
            ConnectToServer(clientSocket, username);
            isConnected = true;
        }
        catch (const runtime_error& e) {
            wcout << L"�T�[�o�[�ւ̐ڑ��Ɏ��s���܂���: " << UTF8ToWString(e.what()) << endl;
        }
        isWaiting = false;
        });

    clientThread.detach();
}

void PPDataConnecter::StartCommunication(SOCKET sock)
{
    {
        lock_guard<mutex> lock(socketMutex);
        currentSocket = sock;
    }

    // �����̒ʐM�X���b�h���~
    StopCommunication();

    // �V�����ʐM�X���b�h���J�n
    isConnected = true;
}

// �ҋ@���̃L�����Z������
void PPDataConnecter::CancelCommunication()
{
    if (isWaiting) {
        // �ҋ@��Ԃ��L�����Z��
        isCanceled = true;
        isWaiting = false;
        // �K�v�Ȃ�ҋ@���̃X���b�h���I�����鏈����ǉ�
    }
}

// �ʐM���~
void PPDataConnecter::StopCommunication()
{
    if (sendThread.joinable()) {
        sendThread.join();
    }
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    isConnected = false;
}


// �T�[�o�[���J�n���ăN���C�A���g����̐ڑ���҂�
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    BindAndListen(serverSocket);
    sockaddr_in serverAddr = {};
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);
    wcout << L"�T�[�o�[ID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"�ڑ���҂��Ă��܂�..." << endl;

    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);
    wcout << L"�ڑ����m������܂����B" << endl;

    // �\�P�b�g���u���b�L���O���[�h�ɐݒ�
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    // �ʐM�J�n
    StartCommunication(clientSocket);
}

void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    wstring id;
    wcout << L"�ڑ���̃T�[�o�[ID����͂��Ă�������: ";
    getline(wcin, id);
    auto decodeID = DecodeAndReverseIPPort(WStringToUTF8(id));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(decodeID.second);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("�ڑ��Ɏ��s���܂����B");
    }

    // �\�P�b�g���u���b�L���O���[�h�ɐݒ�
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    wcout << L"�ڑ����܂����B" << endl;
    StartCommunication(clientSocket);
}


// ���M�o�b�t�@�Ƀf�[�^��ǉ�
void PPDataConnecter::AddToSendBuffer(const std::string& header, const std::string& data) {
    std::lock_guard<std::mutex> lock(sendMutex);
    sendBuffer.push_back({ header, data });
}

// ��M�o�b�t�@����f�[�^���擾
bool PPDataConnecter::GetFromRecvBuffer(BufferedData& outData) {
    std::lock_guard<std::mutex> lock(recvMutex);
    if (recvBuffer.empty()) return false;

    outData = recvBuffer.front();
    recvBuffer.pop_front();
    return true;
}

// ��M�o�b�t�@�������̃w�b�_�̃f�[�^���擾
bool PPDataConnecter::GetFromRecvBufferByHeader(const string& header, BufferedData& outData) {
    std::lock_guard<std::mutex> lock(recvMutex);

    for (auto it = recvBuffer.begin(); it != recvBuffer.end(); ++it) {
        if (it->header == header) {
            outData = *it;
            recvBuffer.erase(it);
            return true;
        }
    }
    return false;  // �w��w�b�_�̃f�[�^���Ȃ�
}

// ���b�Z�[�W�𑗐M
void PPDataConnecter::SendPPMessage(SOCKET sock, const wstring& username, const wstring& message)
{
    wstring fullMessage = username + L": " + message; // ���[�U�[���ƃ��b�Z�[�W������
    string utf8Message = WStringToUTF8(fullMessage);

    // �����`�F�b�N
    if (utf8Message.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("���M���b�Z�[�W���傫�����܂��B");
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
            ReceiveData(string(buffer, bytesReceived));
        }
        else
        {
            break; // �ڑ�������ꂽ�ꍇ
        }
    }
}

// �o�b�t�@����f�[�^���擾�����M����
void PPDataConnecter::SendData() {
    std::lock_guard<std::mutex> lock(sendMutex);
    if (sendBuffer.empty()) return;

    BufferedData data = sendBuffer.front();
    sendBuffer.pop_front();

    // ���M�����i�l�b�g���[�NAPI�Ɠ�������j
    std::string rawData = Serialize(data);
    // send(rawData);  // �����Ƀl�b�g���[�N���M����������
}

// ��M�f�[�^������
void PPDataConnecter::ReceiveData(const std::string& rawData) {
    BufferedData data = Deserialize(rawData);

    std::lock_guard<std::mutex> lock(recvMutex);
    recvBuffer.push_back(data);
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

string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // inet_pton���g�p����IP�A�h���X���o�C�i���`���ɕϊ�
    sockaddr_in sa = {};
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1)
    {
        throw invalid_argument("Invalid IP address.");
    }

    // �o�C�i���`������64�i���ɕϊ�
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ip64Stream;
    ip64Stream << uppercase << ConvertToBase64(ntohl(ip));
    string ip64 = ip64Stream.str();

    // �|�[�g�ԍ���64�i���ɕϊ�
    ostringstream port64Stream;
    port64Stream << uppercase << ConvertToBase64(port);
    string port64 = port64Stream.str();

    // IP�ƃ|�[�g������
    string combined = ip64 + "-" + port64;

    // ������𔽓]
    reverse(combined.begin(), combined.end());

    return combined;
}

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

    string ip64 = combined.substr(0, dashPos);
    string port64 = combined.substr(dashPos + 1);

    // IP�𕜌�
    unsigned long ipNum = ConvertFromBase64(ip64);
    ipNum = htonl(ipNum); // �l�b�g���[�N�o�C�g�I�[�_�[�ɕϊ�

    // inet_ntop ���g�p���ăo�C�i�����當����IP�A�h���X�ɕϊ�
    char ipAddress[INET_ADDRSTRLEN];
    in_addr ipAddr = {};
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr)
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // �|�[�g�𕜌�
    unsigned short port = ConvertFromBase64(port64);

    return { ipAddress, port };
}


// **�f�[�^�� "�w�b�_:�f�[�^" �̌`���ŃV���A���C�Y**
string PPDataConnecter::Serialize(const BufferedData& data) {
    return data.header + ":" + data.data;
}

// **��M�f�[�^���p�[�X**
BufferedData PPDataConnecter::Deserialize(const std::string& rawData) {
    size_t pos = rawData.find(':');
    if (pos == std::string::npos) {
        return { "Unknown", rawData };  // �w�b�_���Ȃ��ꍇ�� "Unknown" �Ƃ���
    }
    return { rawData.substr(0, pos), rawData.substr(pos + 1) };
}
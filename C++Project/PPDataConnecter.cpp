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

#undef max  // �W�����C�u������ `max` �𗘗p���邽�߂� `max` �}�N��������
#pragma comment(lib, "Ws2_32.lib") // Winsock2���C�u�����������N

PPDataConnecter* PPDataConnecter::instance = nullptr;
mutex PPDataConnecter::instanceMutex; // �C���X�^���X�������̔r������p�~���[�e�b�N�X

// wstring�i���C�h������j��UTF-8�G���R�[�h��string�ɕϊ�����֐�
string WStringToUTF8(const wstring& wstr)
{
    if (wstr.empty()) return string(); // �󕶎���̏ꍇ�͂��̂܂ܕԂ�

    // �K�v�ȃo�b�t�@�T�C�Y���擾
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string utf8_str(size_needed, 0); // �K�v�ȃT�C�Y�̃o�b�t�@���m��
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, nullptr, nullptr);

    // ������null�I�[�������폜
    if (!utf8_str.empty() && utf8_str.back() == '\0')
    {
        utf8_str.pop_back();
    }

    return utf8_str;
}

// UTF-8�G���R�[�h��string��wstring�ɕϊ�����֐�
wstring UTF8ToWString(const string& utf8Str)
{
    if (utf8Str.empty()) return wstring(); // �󕶎���̏ꍇ�͂��̂܂ܕԂ�

    // �K�v�ȃo�b�t�@�T�C�Y���擾
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    wstring wide_str(size_needed, 0); // �K�v�ȃT�C�Y�̃o�b�t�@���m��
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wide_str[0], size_needed);

    // ������null�I�[�������폜
    if (!wide_str.empty() && wide_str.back() == L'\0')
    {
        wide_str.pop_back();
    }

    return wide_str;
}

// 10�i���̐��l��64�i���\�L�̕�����ɕϊ�����֐�
string ConvertToBase64(unsigned long number)
{
    const string base64chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"; // 64�i���̕����Z�b�g

    string result;
    do {
        result = base64chars[number % 64] + result; // ���ʌ����珇�ɕϊ�
        number /= 64;
    } while (number > 0);

    return result;
}

// 64�i���\�L�̕������10�i���̐��l�ɕϊ�����֐�
unsigned long ConvertFromBase64(const string& base64Str)
{
    const string base64chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"; // 64�i���̕����Z�b�g

    unsigned long result = 0;
    unsigned long base = 1;

    // ������̌��i�ŉ��ʌ��j���珇�ɏ���
    for (int i = base64Str.length() - 1; i >= 0; --i)
    {
        size_t pos = base64chars.find(base64Str[i]);
        if (pos == string::npos)
        {
            throw invalid_argument("Invalid base64 character."); // �s���ȕ������܂܂�Ă����ꍇ�͗�O�𓊂���
        }
        result += pos * base;
        base *= 64;
    }

    return result;
}

// PPDataConnecter �N���X�̃R���X�g���N�^�i�����o�ϐ����������j
PPDataConnecter::PPDataConnecter() :
    currentSocket(INVALID_SOCKET),
    isConnected(false),
    isWaiting(false),
    isCanceled(false)
{
}

// �f�X�g���N�^�i�I�����������s�j
PPDataConnecter::~PPDataConnecter()
{
    Finalize();
}

// �V���O���g���p�^�[����PPDataConnecter�̃C���X�^���X���擾
PPDataConnecter* PPDataConnecter::GetNetworkPtr()
{
    lock_guard<mutex> lock(instanceMutex); // �X���b�h�Z�[�t�̂��߃~���[�e�b�N�X�����b�N
    if (!instance)
    {
        instance = new PPDataConnecter();
    }
    return instance;
}

// Winsock��������
void PPDataConnecter::Initialize()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw runtime_error("Winsock�̏������Ɏ��s���܂����B");
    }
    isConnected = false;
}

// �ʐM�I������
void PPDataConnecter::Finalize()
{
    StopCommunication(); // �ʐM��~����
    isConnected = false;
    WSACleanup(); // Winsock�̌�n��
}

// �R���\�[���̓��o�͂�UTF-8�Ή��ɕύX
void PPDataConnecter::SetConsoleToUnicode()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // �R���\�[���t�H���g��MS�S�V�b�N�ɐݒ�i���{��̕\�����l���j
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

// �\�P�b�g���쐬����֐�
SOCKET PPDataConnecter::CreateSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        throw runtime_error("�\�P�b�g�̍쐬�Ɏ��s���܂����B");
    }
    return sock;
}

// �T�[�o�[�\�P�b�g���o�C���h���A�ڑ��҂���Ԃɂ���
void PPDataConnecter::BindAndListen(SOCKET& serverSocket)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        throw runtime_error("�\�P�b�g�̍쐬�Ɏ��s���܂����B");
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0); // OS�Ƀ|�[�g���������蓖�Ă�����

    if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("�o�C���h�Ɏ��s���܂����B");
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        throw runtime_error("���b�X���Ɏ��s���܂����B");
    }
}

// �N���C�A���g����̐ڑ����󂯓����
void PPDataConnecter::AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket)
{
    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET)
    {
        throw runtime_error("�ڑ��̎󂯓���Ɏ��s���܂����B");
    }
}

// �T�[�o�[��񓯊��ɋN������i�ڑ��ҋ@�ƃL�����Z�����Ǘ��j
void PPDataConnecter::StartServerAsync(SOCKET& serverSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // �T�[�o�[������ʃX���b�h�Ŏ��s
    thread serverThread([&]()
        {
            try
            {
                StartServer(serverSocket, username);
                isConnected = true;
            }
            catch (const runtime_error& e)
            {
                wcout << L"�T�[�o�[�̊J�n�Ɏ��s���܂���: " << UTF8ToWString(e.what()) << endl;
            }
            isWaiting = false;
        }
    );

    serverThread.detach(); // �X���b�h���f�^�b�`�i�o�b�N�O���E���h���s�j
}

// �T�[�o�[�ڑ����X���b�h�ŊJ�n
void PPDataConnecter::ConnectToServerAsync(SOCKET& clientSocket, const wstring& username)
{
    isWaiting = true;
    isCanceled = false;

    // �N���C�A���g�ڑ�������񓯊��Ŏ��s
    thread clientThread([&]()
        {
            try
            {
                // �T�[�o�[�ɐڑ������݂�
                ConnectToServer(clientSocket, username);
                isConnected = true;
            }
            catch (const runtime_error& e)
            {
                // �ڑ��G���[��\��
                wcout << L"�T�[�o�[�ւ̐ڑ��Ɏ��s���܂���: " << UTF8ToWString(e.what()) << endl;
            }
            isWaiting = false;
        }
    );

    // �X���b�h���f�^�b�`�i�ʃX���b�h�ŏ������p���j
    clientThread.detach();
}

// �ʐM�̊J�n
void PPDataConnecter::StartCommunication(SOCKET sock, const wstring& username)
{
    {
        lock_guard<mutex> lock(socketMutex);
        currentSocket = sock;
    }

    // �����̒ʐM�X���b�h������Β�~
    StopCommunication();

    // �ʐM�J�n�t���O��ݒ�
    isConnected = true;

    // ���b�Z�[�W���M�X���b�h���J�n
    sendThread = thread([&]()
        {
            try
            {
                wstring message;

                while (isConnected)
                {
                    // ���[�U�[����̓��͂��擾
                    getline(wcin, message);
                    if (message == L"exit") break; // "exit" �ŏI��

                    // �T�[�o�[�Ƀ��b�Z�[�W�𑗐M
                    SendPPMessage(currentSocket, username, message);

                    // �ؒf�m�F
                    if (!isConnected)
                    {
                        break;
                    }
                }
            }
            catch (const runtime_error& e)
            {
                wcout << L"���M�v���Z�X�ŃG���[���������܂��� : " << UTF8ToWString(e.what()) << endl;
            }
        }
    );

    // ���b�Z�[�W��M�X���b�h���J�n
    receiveThread = thread([&]()
        {
            try
            {
                BufferedData out;
                BufferedData temp;

                while (isConnected)
                {
                    // ��M�f�[�^���o�b�t�@�ɒǉ�
                    ReceivePPMessages(currentSocket);

                    // �o�b�t�@����f�[�^���擾
                    if (!GetFromRecvBuffer(out)) continue;

                    // �d�����b�Z�[�W�̖h�~
                    if (out.data != temp.data)
                    {
                        wcout << username + L":" << UTF8ToWString(out.data) << endl;
                        temp = out;
                    }

                    // �ؒf�m�F
                    if (!isConnected || out.data == "exit")
                    {
                        break;
                    }

                    // �f�[�^�����Z�b�g
                    out = BufferedData();
                }
            }
            catch (const runtime_error& e)
            {
                wcout << L"��M�v���Z�X�ŃG���[���������܂��� : " << UTF8ToWString(e.what()) << endl;
            }
        }
    );
}

// �ҋ@��Ԃ̃L�����Z������
void PPDataConnecter::CancelCommunication()
{
    if (isWaiting)
    {
        // �L�����Z���t���O�𗧂Ă�
        isCanceled = true;
        isWaiting = false;
    }
}

// �ʐM�̒�~
void PPDataConnecter::StopCommunication()
{
    // ���M�X���b�h�̏I���ҋ@
    if (sendThread.joinable())
    {
        sendThread.join();
    }
    // ��M�X���b�h�̏I���ҋ@
    if (receiveThread.joinable())
    {
        receiveThread.join();
    }

    // �ڑ���Ԃ�����
    isConnected = false;
}

// �T�[�o�[���J�n���ăN���C�A���g����̐ڑ���҂�
void PPDataConnecter::StartServer(SOCKET& serverSocket, const wstring& username)
{
    // �T�[�o�[���o�C���h���ă��X�j���O���J�n
    BindAndListen(serverSocket);

    // �T�[�o�[��IP�A�h���X�ƃ|�[�g���擾
    sockaddr_in serverAddr = {};
    int addrLen = sizeof(serverAddr);
    getsockname(serverSocket, (sockaddr*)&serverAddr, &addrLen);

    // �T�[�o�[����\��
    wcout << L"�T�[�o�[ID: " << UTF8ToWString(EncodeAndReverseIPPort(GetLocalIPAddress(), ntohs(serverAddr.sin_port))) << endl;
    wcout << L"�ڑ���҂��Ă��܂�..." << endl;

    // �N���C�A���g�ڑ��ҋ@
    SOCKET clientSocket;
    AcceptConnection(serverSocket, clientSocket);
    wcout << L"�ڑ����m������܂����B" << endl;

    // �\�P�b�g���u���b�L���O���[�h�ɐݒ�
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    // �ʐM�J�n
    StartCommunication(clientSocket, username);
}

// �N���C�A���g���T�[�o�[�ɐڑ�����
void PPDataConnecter::ConnectToServer(SOCKET& clientSocket, const wstring& username)
{
    wstring id;
    wcout << L"�ڑ���̃T�[�o�[ID����͂��Ă�������: ";
    getline(wcin, id);

    // �T�[�o�[ID���f�R�[�h����IP�A�h���X�ƃ|�[�g�ԍ����擾
    auto decodeID = DecodeAndReverseIPPort(WStringToUTF8(id));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, decodeID.first.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(decodeID.second);

    // �T�[�o�[�֐ڑ�
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        throw runtime_error("�ڑ��Ɏ��s���܂����B");
    }

    // �\�P�b�g���u���b�L���O���[�h�ɐݒ�
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    wcout << L"�ڑ����܂����B" << endl;

    // �ʐM�J�n
    StartCommunication(clientSocket, username);
}

// ���M�o�b�t�@�Ƀf�[�^��ǉ�
void PPDataConnecter::AddToSendBuffer(const string& header, const string& data)
{
    lock_guard<mutex> lock(sendMutex);
    sendBuffer.push_back({ header, data });
}

// ��M�o�b�t�@����f�[�^���擾
bool PPDataConnecter::GetFromRecvBuffer(BufferedData& outData)
{
    lock_guard<mutex> lock(recvMutex);

    // �o�b�t�@����Ȃ�擾�ł��Ȃ�
    if (recvBuffer.empty()) return false;

    // �ŌẪf�[�^���擾
    outData = recvBuffer.front();
    recvBuffer.pop_front();
    return true;
}

// ��M�o�b�t�@�������̃w�b�_�̃f�[�^���擾
bool PPDataConnecter::GetFromRecvBufferByHeader(const string& header, BufferedData& outData)
{
    lock_guard<mutex> lock(recvMutex);

    // �w��w�b�_�̃f�[�^������
    for (auto it = recvBuffer.begin(); it != recvBuffer.end(); ++it)
    {
        if (it->header == header)
        {
            // �f�[�^���擾���č폜
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
    // ���[�U�[���ƃ��b�Z�[�W������
    wstring fullMessage = username + L": " + message;
    string utf8Message = WStringToUTF8(fullMessage);

    // ���M�f�[�^�̒����������̍ő�l�𒴂��Ȃ��悤�Ƀ`�F�b�N
    if (utf8Message.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("���M���b�Z�[�W���傫�����܂��B");
    }

    // �T�[�o�[�փf�[�^�𑗐M
    send(sock, utf8Message.c_str(), static_cast<int>(utf8Message.length()), 0);
}

// ���b�Z�[�W����M
void PPDataConnecter::ReceivePPMessages(SOCKET socket)
{
    char buffer[BUFFER_SIZE];

    // ��M�����̃��[�v
    while (true)
    {
        int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0'; // ��M�f�[�^��null�I�[
            ReceiveData(string(buffer, bytesReceived)); // ��M�f�[�^������
        }
        else
        {
            break; // �ʐM���I���܂��̓G���[�������Ƀ��[�v�𔲂���
        }
    }
}

// �o�b�t�@����f�[�^���擾�����M����
void PPDataConnecter::SendData()
{
    lock_guard<mutex> lock(sendMutex);
    if (sendBuffer.empty()) return;

    // ���M�o�b�t�@�̐擪�f�[�^���擾
    BufferedData data = sendBuffer.front();
    sendBuffer.pop_front();

    // �f�[�^�𑗐M�\�Ȍ`���ɃV���A���C�Y
    string rawData = Serialize(data);

    // ���M�f�[�^�̒����`�F�b�N
    if (rawData.length() > static_cast<size_t>(numeric_limits<int>::max()))
    {
        throw runtime_error("���M���b�Z�[�W���傫�����܂��B");
    }

    // �f�[�^���\�P�b�g�o�R�ő��M
    send(currentSocket, rawData.c_str(), static_cast<int>(rawData.length()), 0);
}

// ��M�f�[�^������
void PPDataConnecter::ReceiveData(const string& rawData)
{
    // ��M�f�[�^����͂��č\����
    BufferedData data = Deserialize(rawData);

    // ��M�o�b�t�@�ɒǉ�
    lock_guard<mutex> lock(recvMutex);
    recvBuffer.push_back(data);
}

// ���[�J��IP�A�h���X���擾
string PPDataConnecter::GetLocalIPAddress()
{
    char hostName[256];

    // �z�X�g�����擾
    gethostname(hostName, sizeof(hostName));

    // IPv4�A�h���X�����擾
    addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    getaddrinfo(hostName, nullptr, &hints, &result);

    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];

    // �o�C�i��IP�𕶎���ɕϊ�
    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);

    freeaddrinfo(result);
    return string(ipAddress);
}

// ���[�J��IP�A�h���X�����C�h������Ŏ擾
wstring PPDataConnecter::GetLocalIPAddressW()
{
    return UTF8ToWString(GetLocalIPAddress());
}

// IP�A�h���X�ƃ|�[�g���G���R�[�h���A���]
string PPDataConnecter::EncodeAndReverseIPPort(const string& ipAddress, unsigned short port)
{
    // IP�A�h���X���o�C�i���`���ɕϊ�
    sockaddr_in sa = {};
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1)
    {
        throw invalid_argument("Invalid IP address.");
    }

    // IP�A�h���X��64�i���ɕϊ�
    unsigned long ip = sa.sin_addr.s_addr;
    ostringstream ip64Stream;
    ip64Stream << uppercase << ConvertToBase64(ntohl(ip));
    string ip64 = ip64Stream.str();

    // �|�[�g�ԍ���64�i���ɕϊ�
    ostringstream port64Stream;
    port64Stream << uppercase << ConvertToBase64(port);
    string port64 = port64Stream.str();

    // IP�ƃ|�[�g���������A���]
    string combined = ip64 + "-" + port64;
    reverse(combined.begin(), combined.end());

    return combined;
}

// �G���R�[�h���ꂽIP�A�h���X�ƃ|�[�g���f�R�[�h���A����
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

    // 64�i������IP�A�h���X�𕜌�
    unsigned long ipNum = ConvertFromBase64(ip64);
    ipNum = htonl(ipNum);

    // �o�C�i��IP�𕶎���ɕϊ�
    char ipAddress[INET_ADDRSTRLEN];
    in_addr ipAddr = {};
    ipAddr.s_addr = ipNum;
    if (inet_ntop(AF_INET, &ipAddr, ipAddress, INET_ADDRSTRLEN) == nullptr)
    {
        throw invalid_argument("Invalid IP address format.");
    }

    // 64�i������|�[�g�ԍ��𕜌�
    unsigned short port = ConvertFromBase64(port64);

    return { ipAddress, port };
}

// �f�[�^�� "�w�b�_:�f�[�^" �̌`���ŃV���A���C�Y
string PPDataConnecter::Serialize(const BufferedData& data)
{
    return data.header + ":" + data.data;
}

// ��M�f�[�^���p�[�X
BufferedData PPDataConnecter::Deserialize(const string& rawData)
{
    size_t pos = rawData.find(':');

    // �w�b�_���Ȃ��ꍇ�� "Unknown" �Ƃ���
    if (pos == string::npos)
    {
        return { "Unknown", rawData };
    }

    // �w�b�_�ƃf�[�^�𕪊����č\���̂Ɋi�[
    return { rawData.substr(0, pos), rawData.substr(pos + 1) };
}
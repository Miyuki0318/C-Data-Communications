#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <condition_variable>

using namespace std;

// wstring��UTF-8�̑��ݕϊ����s�����[�e�B���e�B�֐�
string WStringToUTF8(const wstring& wstr); // wstring��UTF-8�`����string�ɕϊ�
wstring UTF8ToWString(const string& utf8Str); // UTF-8�`����string��wstring�ɕϊ�

// 10�i������64�i���ɕϊ�����֐�
string ConvertToBase64(unsigned long number);
unsigned long ConvertFromBase64(const string& base64Str);

// �f�[�^�o�b�t�@�̂��߂̍\����
struct BufferedData
{
    string header;  // �f�[�^�̎�ނ����ʂ���w�b�_�[
    string data;    // ���ۂ̃f�[�^
};

// PPDataConnecter�N���X�́A�f�[�^�ʐM��\�P�b�g�Ǘ����s��
class PPDataConnecter
{
private:

    // �o�b�t�@�T�C�Y
    static const int BUFFER_SIZE = 4096;

    // �V���O���g���C���X�^���X
    static PPDataConnecter* instance;
    static mutex instanceMutex;

    // �񓯊��ʐM�p�̃����o
    condition_variable waitCondition;  // �ʐM�ҋ@���Ɏg�p

    // �񓯊��ʐM�̃X���b�h
    thread sendThread;
    thread receiveThread;

    mutex socketMutex;  // �\�P�b�g�Ɋւ���r������

    deque<BufferedData> sendBuffer;
    deque<BufferedData> recvBuffer;
    mutex sendMutex;
    mutex recvMutex;

    SOCKET currentSocket;

public:

    atomic<bool> isWaiting;  // �ʐM�ҋ@���
    atomic<bool> isCanceled; // �ʐM�L�����Z�����
    atomic<bool> isConnected; // �ڑ����

    // �R���X�g���N�^�ƃf�X�g���N�^
    PPDataConnecter(); // �N���X�̏�����
    ~PPDataConnecter(); // �N���X�̏I������

    // �V���O���g���A�N�Z�X
    static PPDataConnecter* GetNetworkPtr();

    // �������ƏI��
    void Initialize();
    void Finalize();

    // �R���\�[����Unicode�iUTF-8�j�ɐݒ�
    void SetConsoleToUnicode();

    // �V�����\�P�b�g���쐬
    SOCKET CreateSocket();

    // �T�[�o�[���J�n���A�N���C�A���g����̐ڑ���ҋ@
    void StartServer(SOCKET& socket, const wstring& username);

    // �T�[�o�[�ɐڑ����ĒʐM���J�n
    void ConnectToServer(SOCKET& socket, const wstring& username);

    // ���b�Z�[�W�𑗐M����ÓI���\�b�h
    void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message);

    // ���b�Z�[�W����M����ÓI���\�b�h
    void ReceivePPMessages(SOCKET socket);

    // �X���b�h�񓯊��ʐM�֘A
    void StartCommunication(SOCKET sock);
    void CancelCommunication();
    void StopCommunication();
    void Communication(SOCKET sock);
    void BindAndListen(SOCKET& serverSocket);
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);
    void StartServerAsync(SOCKET& serverSocket, const wstring& username); // �T�[�o�[�X���b�h�̊J�n
    void ConnectToServerAsync(SOCKET& clientSocket, const wstring& username); // �N���C�A���g�X���b�h�̊J�n

    // ���M�o�b�t�@�Ƀf�[�^��ǉ�
    void AddToSendBuffer(const std::string& header, const std::string& data);

    // ��M�o�b�t�@����f�[�^���擾
    bool GetFromRecvBuffer(BufferedData& outData);
    bool GetFromRecvBufferByHeader(const string& header, BufferedData& outData);

    // �f�[�^����M�̏���
    void SendData();  // �o�b�t�@���瑗�M
    void ReceiveData(const std::string& rawData);  // ��M�f�[�^������

    // ���[�J��IP�A�h���X���擾����ÓI���\�b�h
    static string GetLocalIPAddress();
    static wstring GetLocalIPAddressW();

    // IP�A�h���X�ƃ|�[�g�ԍ����T�[�o�[ID�ɕϊ�����ÓI���\�b�h
    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port);

    // �T�[�o�[ID��IP�A�h���X�ƃ|�[�g�ԍ��ɕϊ�����ÓI���\�b�h
    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed);

private:

    // �����p: ���M�f�[�^�̃t�H�[�}�b�g�ϊ�
    static string Serialize(const BufferedData& data);
    static BufferedData Deserialize(const std::string& rawData);
};

#endif

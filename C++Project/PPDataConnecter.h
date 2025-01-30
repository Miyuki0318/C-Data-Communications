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

using namespace std; // �W�����C�u�������ȗ��`�Ŏg�p�\�ɂ���

// ������ϊ����[�e�B���e�B�֐�
string WStringToUTF8(const wstring& wstr); // wstring �� UTF-8 �`���� string �ɕϊ�
wstring UTF8ToWString(const string& utf8Str); // UTF-8 �`���� string �� wstring �ɕϊ�

// 64�i���ϊ����[�e�B���e�B�֐�
string ConvertToBase64(unsigned long number); // 10�i������64�i���ɕϊ�
unsigned long ConvertFromBase64(const string& base64Str); // 64�i������10�i���ɕϊ�

// �f�[�^����M�p�̃o�b�t�@�\����
struct BufferedData
{
    string header;  // �f�[�^�̎��ʗp�w�b�_�[
    string data;    // ���ۂ̃f�[�^�{��
};

// �f�[�^�ʐM����у\�P�b�g�Ǘ��N���X
class PPDataConnecter
{
private:

    static const int BUFFER_SIZE = 4096; // ����M�o�b�t�@�̃T�C�Y

    // �V���O���g���C���X�^���X�Ǘ�
    static PPDataConnecter* instance;
    static mutex instanceMutex;

    // �񓯊��ʐM�p�̃����o�ϐ�
    condition_variable waitCondition;  // �ʐM�ҋ@���̓��������p
    thread sendThread;   // ���M�X���b�h
    thread receiveThread; // ��M�X���b�h

    mutex socketMutex;  // �\�P�b�g�ی�p�̃~���[�e�b�N�X
    deque<BufferedData> sendBuffer; // ���M�o�b�t�@
    deque<BufferedData> recvBuffer; // ��M�o�b�t�@
    mutex sendMutex; // ���M�o�b�t�@�̔r������
    mutex recvMutex; // ��M�o�b�t�@�̔r������

    SOCKET currentSocket; // ���݂̐ڑ��\�P�b�g

public:

    atomic<bool> isWaiting;  // �ʐM�ҋ@�t���O
    atomic<bool> isCanceled; // �ʐM�L�����Z���t���O
    atomic<bool> isConnected; // �ڑ���ԃt���O

    // �R���X�g���N�^�E�f�X�g���N�^
    PPDataConnecter();  // ����������
    ~PPDataConnecter(); // ��n������

    // �V���O���g���擾
    static PPDataConnecter* GetNetworkPtr();

    // �������ƏI������
    void Initialize();  // Winsock ������
    void Finalize();    // Winsock �I������

    // �R���\�[���̃G���R�[�f�B���O�ݒ�
    void SetConsoleToUnicode();

    // �\�P�b�g�֘A����
    SOCKET CreateSocket(); // �\�P�b�g�쐬
    void BindAndListen(SOCKET& serverSocket); // �\�P�b�g���o�C���h���đҋ@
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket); // �ڑ����󂯓����

    // �ʐM�J�n
    void StartServer(SOCKET& socket, const wstring& username); // �T�[�o�[�J�n
    void ConnectToServer(SOCKET& socket, const wstring& username); // �N���C�A���g�ڑ�

    // �񓯊��ʐM
    void StartServerAsync(SOCKET& serverSocket, const wstring& username); // �񓯊��T�[�o�[�J�n
    void ConnectToServerAsync(SOCKET& clientSocket, const wstring& username); // �񓯊��N���C�A���g�ڑ�

    // �ʐM�X���b�h����
    void StartCommunication(SOCKET sock, const wstring& username);
    void CancelCommunication();
    void StopCommunication();

    // �f�[�^�̑���M
    void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message); // ���b�Z�[�W���M
    void ReceivePPMessages(SOCKET socket); // ���b�Z�[�W��M
    void AddToSendBuffer(const std::string& header, const std::string& data); // ���M�o�b�t�@�Ƀf�[�^�ǉ�
    bool GetFromRecvBuffer(BufferedData& outData); // ��M�o�b�t�@����f�[�^�擾
    bool GetFromRecvBufferByHeader(const string& header, BufferedData& outData); // ����̃w�b�_�[������

    // ����M����
    void SendData(); // �o�b�t�@���瑗�M
    void ReceiveData(const std::string& rawData); // ��M�f�[�^������

    // �l�b�g���[�N���[�e�B���e�B
    static string GetLocalIPAddress(); // ���[�J��IP�擾
    static wstring GetLocalIPAddressW(); // ���C�h������ł̃��[�J��IP�擾

    // �T�[�o�[ID�G���R�[�h/�f�R�[�h
    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port); // IP�ƃ|�[�g���G���R�[�h
    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed); // �G���R�[�h���ꂽIP�ƃ|�[�g���f�R�[�h

private:

    // ��������
    static string Serialize(const BufferedData& data); // �f�[�^���V���A���C�Y
    static BufferedData Deserialize(const std::string& rawData); // �f�V���A���C�Y
};

#endif

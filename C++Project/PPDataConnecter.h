#ifndef PPDATACONNECTER_H
#define PPDATACONNECTER_H

#include <string>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

// wstring��UTF-8�̑��ݕϊ����s�����[�e�B���e�B�֐�
string WStringToUTF8(const wstring& wstr); // wstring��UTF-8�`����string�ɕϊ�
wstring UTF8ToWString(const string& utf8Str); // UTF-8�`����string��wstring�ɕϊ�

// PPDataConnecter�N���X�́A�f�[�^�ʐM��\�P�b�g�Ǘ����s��
class PPDataConnecter
{
public:
    // �R���X�g���N�^�ƃf�X�g���N�^
    PPDataConnecter(); // �N���X�̏�����
    ~PPDataConnecter(); // �N���X�̏I������

    // Winsock�̏�����
    void InitializeWinsock();

    // �R���\�[����Unicode�iUTF-8�j�ɐݒ�
    void SetConsoleToUnicode();

    // �V�����\�P�b�g���쐬
    SOCKET CreateSocket();

    // �T�[�o�[���J�n���A�N���C�A���g����̐ڑ���ҋ@
    void StartServer(SOCKET& serverSocket, const wstring& username);
    
    // �T�[�o�[�ɐڑ����ĒʐM���J�n
    void ConnectToServer(SOCKET& clientSocket, const wstring& username);

    // �T�[�o�[���J�n���A�N���C�A���g����̐ڑ���ҋ@���A�t�@�C���̑���M���s��
    void StartFileTransServer(SOCKET& socket, const std::wstring& username);

    // �T�[�o�[�ɐڑ����ĒʐM���J�n���A�t�@�C���̑���M���s��
    void ConnectToFileTransServer(SOCKET& socket, const std::wstring& username);

    // ���b�Z�[�W�𑗐M����ÓI���\�b�h
    static void SendPPMessage(SOCKET sock, const wstring& username, const wstring& message);

    // ���b�Z�[�W����M����ÓI���\�b�h
    static void ReceivePPMessages(SOCKET socket);
    
    // �t�@�C���𑗐M����ÓI���\�b�h
    static void SendFile(SOCKET& socket, const wstring& filename, const wstring& fileContent);
    
    // �t�@�C������M����ÓI���\�b�h
    static void ReceiveFile(SOCKET& clientSocket, wstring& receivedFile);

    // ���[�J��IP�A�h���X���擾����ÓI���\�b�h
    static string GetLocalIPAddress();
    static wstring GetLocalIPAddressW();

    // IP�A�h���X�ƃ|�[�g�ԍ����T�[�o�[ID�ɕϊ�����ÓI���\�b�h
    static string EncodeAndReverseIPPort(const string& ipAddress, unsigned short port);

    // �T�[�o�[ID��IP�A�h���X�ƃ|�[�g�ԍ��ɕϊ�����ÓI���\�b�h
    static pair<string, unsigned short> DecodeAndReverseIPPort(const string& encodedReversed);

private:

    // �T�[�o�[�\�P�b�g���o�C���h���ă��b�X����Ԃɂ���
    void BindAndListen(SOCKET& serverSocket);

    // �N���C�A���g�̐ڑ����󂯓����
    void AcceptConnection(SOCKET serverSocket, SOCKET& clientSocket);

    // ��������t�@�C���ɕۑ�����
    static void SaveString(SOCKET& socket, const wstring& str);

    // �t�@�C�����當������擾����
    static void ReadString(SOCKET& socket, string& str);

    // log�t�H���_�쐬�֐�
    static void CreateLogDirectoryIfNotExist();
};

#endif

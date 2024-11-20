#ifndef PPDATACONNECTOR_H
#define PPDATACONNECTOR_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") // Winsock���C�u�����������N����

using namespace std; // std:: ���O��Ԃ��ȗ�����

// �o�b�t�@�T�C�Y���`����
const int BUFFER_SIZE = 4096;

// PPDataConnector�N���X�̒�`
// ���̃N���X�́A�\�P�b�g�ʐM���Ǘ�����N���X�ł���
class PPDataConnector {
private:
    SOCKET socket_;         // �\�P�b�g�I�u�W�F�N�g
    bool isHost_;           // �z�X�g���N���C�A���g���������t���O
    wstring ipAddress_;     // �ڑ����IP�A�h���X
    int port_;              // �g�p����|�[�g�ԍ�

public:
    // �z�X�g�p�R���X�g���N�^
    PPDataConnector(int port);

    // �N���C�A���g�p�R���X�g���N�^
    PPDataConnector(int port, const wstring& ipAddress);

    // �f�X�g���N�^
    ~PPDataConnector();

    // ���b�Z�[�W�𑗐M����֐�
    void SendPPMessage(const wstring& message);

    // ���b�Z�[�W����M����֐�
    wstring ReceiveMessage();

    // �\�P�b�g�I�u�W�F�N�g���擾����֐�
    SOCKET GetSocket() const;

    // �z�X�g��IP�A�h���X���擾����֐�
    static wstring GetHostIP();

    // �\�P�b�g�ʐM���J�n����֐�
    void Start() const;
};

// wstring��string�ɕϊ�����֐�
string wstring_to_string(const wstring& wstr);

// string��wstring�ɕϊ�����֐�
wstring string_to_wstring(const string& str);

#endif // PPDATACONNECTOR_H

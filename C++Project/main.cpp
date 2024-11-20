#include <iostream>
#include <string>
#include <fcntl.h>
#include <io.h>
#include "PPDataConnector.h"

using namespace std;

// �R���\�[���̃G���R�[�f�B���O��UTF-16�ɐݒ肷��֐�
static void SetUTF16Console()
{
    if (_setmode(_fileno(stdin), _O_U16TEXT) == -1)
    {
        wcerr << L"�W�����͂̃G���R�[�f�B���O�ݒ�Ɏ��s���܂����B" << endl;
    }

    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1)
    {
        wcerr << L"�W���o�͂̃G���R�[�f�B���O�ݒ�Ɏ��s���܂����B" << endl;
    }

    if (_setmode(_fileno(stderr), _O_U16TEXT) == -1)
    {
        wcerr << L"�W���G���[�o�͂̃G���R�[�f�B���O�ݒ�Ɏ��s���܂����B" << endl;
    }
}

// �`���b�g�����s����֐�
static void RunChat(PPDataConnector& connector, const wstring& username)
{
    wstring message;
    while (true)
    {
        // ���b�Z�[�W�̓���
        getline(wcin, message);
        if (message == L"exit") break;

        wstring fullMessage = username + L": " + message;
        connector.SendPPMessage(fullMessage); // ���b�Z�[�W���M

        // ���b�Z�[�W��M
        wstring receivedMessage = connector.ReceiveMessage();
        if (!receivedMessage.empty())
        {
            wcout << L"��M���b�Z�[�W: " << receivedMessage << endl;
        }
    }
}

int main()
{
    SetUTF16Console(); // �R���\�[���̃G���R�[�f�B���O��UTF-16�ɐݒ�

    wstring username;
    wcout << L"���[�U�[������͂��Ă�������: ";
    getline(wcin, username); // ���[�U�[���̓���

    wstring mode;
    wcout << L"�z�X�g�Ƃ��ă`���b�g���J�n���܂����H (Y/N): ";
    getline(wcin, mode); // ���[�h�̓���

    // �z�X�g�Ƃ��ă`���b�g���J�n
    if (mode == L"Y" || mode == L"y")
    {
        wstring portInput;
        wcout << L"�J�݂���|�[�g�ԍ�����͂��Ă�������: ";
        getline(wcin, portInput); // �|�[�g�ԍ��𕶎���Ƃ��ē���
        int port = stoi(portInput); // ������𐮐��ɕϊ�
        wcout << L"���g��IP�A�h���X��: " << PPDataConnector::GetHostIP() << endl;

        PPDataConnector connector(port); // �R�l�N�^�̍쐬

        connector.Start(); // �z�X�g���[�h�J�n
        wcout << L"�z�X�g���[�h�Ń`���b�g���J�n���܂�..." << endl;

        RunChat(connector, username); // �`���b�g�̎��s
    }
    else
    {
        wstring ipAddress;
        wcout << L"�ڑ����IP�A�h���X����͂��Ă�������: ";
        getline(wcin, ipAddress); // IP�A�h���X�̓���

        wstring portInput;
        wcout << L"�ڑ���̃|�[�g�ԍ�����͂��Ă�������: ";
        getline(wcin, portInput); // �|�[�g�ԍ��𕶎���Ƃ��ē���
        int port = stoi(portInput); // ������𐮐��ɕϊ�

        PPDataConnector connector(port, ipAddress); // �R�l�N�^�̍쐬

        wcout << L"�N���C�A���g�Ƃ��Đڑ����܂�..." << endl;

        RunChat(connector, username); // �`���b�g�̎��s
    }

    return 0;
}

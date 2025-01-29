#include "PPDataConnecter.h"
#include <iostream>
#include <string>

using namespace std;

int main()
{
    try
    {
        // PPDataConnecter�N���X�̃C���X�^���X�𐶐�
        PPDataConnecter connecter;

        // �R���\�[����Unicode�ɐݒ�iUTF-8�Ή��j
        connecter.SetConsoleToUnicode();

        // Winsock���C�u������������
        connecter.Initialize();

        // �\�P�b�g���쐬
        SOCKET socket = connecter.CreateSocket();

        // ���[�U�[�������
        wstring username;
        wcout << L"���[�U�[������͂��Ă�������: ";
        getline(wcin, username);

        // �T�[�o�[�쐬���[�h���N���C�A���g���[�h����I��
        wstring mode;
        wcout << L"�`���b�g���[�����쐬���܂����H (Y/N): ";
        getline(wcin, mode);

        // �T�[�o�[���[�h�̏ꍇ
        if (mode == L"Y" || mode == L"y")
        {
            connecter.StartServer(); // �T�[�o�[���N��
        }
        // �N���C�A���g���[�h�̏ꍇ
        else if (mode == L"N" || mode == L"n")
        {
            connecter.ConnectToServer(); // �T�[�o�[�ɐڑ�
        }
        else
        {
            // �����ȑI���������͂��ꂽ�ꍇ
            wcout << L"�����ȑI���ł��B" << endl;
        }

        while (connecter.isRunning || connecter.isConnecting)
        {
            if (!connecter.isRunning)
            {
                break;
            }
        }

        // �\�P�b�g�����
        closesocket(socket);
    }
    catch (const exception& ex)
    {
        // ��O�������̃G���[���b�Z�[�W��\��
        wcout << L"�G���[: " << UTF8ToWString(ex.what()) << endl;
    }

    return 0;
}

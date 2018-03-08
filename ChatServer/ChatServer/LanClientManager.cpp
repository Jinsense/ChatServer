#include <wchar.h>

#include "LanClientManager.h"
#include "ChatServer.h"

CLanClientManager::CLanClientManager()
{

}

CLanClientManager::~CLanClientManager()
{

}

void CLanClientManager::Constructor(CChatServer *pChat)
{
	pChatServer = pChat;
	return;
}

void CLanClientManager::OnEnterJoinServer()
{
	//	�������� ���� ���� ��
	CPacket *pPacket = CPacket::Alloc();
	bConnect = true;

	WORD Type = en_PACKET_SS_LOGINSERVER_LOGIN;
	BYTE ServerType = dfSERVER_TYPE_CHAT;
	WCHAR ServerName[32] = L"ChatServer";

	*pPacket << Type << ServerType;
	pPacket->PushData(&ServerName[0], sizeof(ServerName));

	SendPacket(pPacket);
	pPacket->Free();
	return;
}

void CLanClientManager::OnLeaveServer()
{
	//	�������� ������ �������� ��
	bConnect = false;
	return;
}

void CLanClientManager::OnRecv(CPacket *pPacket)
{
	//	��Ŷ �ϳ� ���� �Ϸ� ��
	WORD Type;
	*pPacket >> Type;

	if (en_PACKET_SS_REQ_NEW_CLIENT_LOGIN == Type)
	{
		KEY Session;
//		INT64 AccountNo;
//		CHAR SessionKey[64];
		INT64 Parameter;

		*pPacket >> Session.AccountNo;
		pPacket->PopData(&Session.SessionKey[0], sizeof(Session.SessionKey));
		*pPacket >> Parameter;

		//	ä�ü��� ����Ű ���̺� �߰�
		AcquireSRWLockExclusive(&pChatServer->m_KeyTable_srw);
		pChatServer->m_KeyTable.push_back(Session);
		ReleaseSRWLockExclusive(&pChatServer->m_KeyTable_srw);
		//	����Ű �����Ϸ� ��Ŷ ����
		CPacket *pNewPacket = CPacket::Alloc();
		Type = en_PACKET_SS_RES_NEW_CLIENT_LOGIN;
		*pNewPacket << Type << Session.AccountNo << Parameter;
		SendPacket(pNewPacket);
	}


	return;
}

void CLanClientManager::OnSend(int SendSize)
{
	//	��Ŷ �۽� �Ϸ� ��

	return;
}

void CLanClientManager::OnWorkerThreadBegin()
{

}

void CLanClientManager::OnWorkerThreadEnd()
{

}

void CLanClientManager::OnError(int ErrorCode, WCHAR *pMsg)
{

}


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
	//	서버와의 연결 성공 후
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
	//	서버와의 연결이 끊어졌을 때
	bConnect = false;
	return;
}

void CLanClientManager::OnRecv(CPacket *pPacket)
{
	//	패킷 하나 수신 완료 후
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

		//	채팅서버 세션키 테이블에 추가
		AcquireSRWLockExclusive(&pChatServer->m_KeyTable_srw);
		pChatServer->m_KeyTable.push_back(Session);
		ReleaseSRWLockExclusive(&pChatServer->m_KeyTable_srw);
		//	세션키 공유완료 패킷 전송
		CPacket *pNewPacket = CPacket::Alloc();
		Type = en_PACKET_SS_RES_NEW_CLIENT_LOGIN;
		*pNewPacket << Type << Session.AccountNo << Parameter;
		SendPacket(pNewPacket);
	}


	return;
}

void CLanClientManager::OnSend(int SendSize)
{
	//	패킷 송신 완료 후

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


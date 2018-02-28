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

}

void CLanClientManager::OnLeaveServer()
{

}

void CLanClientManager::OnRecv(CPacket *pPacket)
{

}

void CLanClientManager::OnSend(int SendSize)
{

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


#ifndef _CHATSERVER_CLIENT_MANAGER_H_
#define _CHATSERVER_CLIENT_MANAGER_H_

//#include "ChatServer.h"
#include "LanClient.h"
#include "CommonProtocol.h"


class CChatServer;
class CLanClientManager : public CLanClient
{
public:
	CLanClientManager();
	~CLanClientManager();

	void Constructor(CChatServer *pChat);

	void OnEnterJoinServer();
	void OnLeaveServer();
	void OnRecv(CPacket *pPacket);
	void OnSend(int SendSize);
	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();
	void OnError(int ErrorCode, WCHAR *pMsg);

public:
	CChatServer *pChatServer;
};

#endif _CHATSERVER_CLIENT_MANAGER_H_

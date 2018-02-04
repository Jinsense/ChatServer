#ifndef _CHATSERVER_IOCP_CHATSERVER_H_
#define _CHATSERVER_IOCP_CHATSERVER_H_

#include <map>
#include <list>

#include "NetServer.h"
#include "CommonProtocol.h"

using namespace std;

#define		SECTOR_Y_MAX	50
#define		SECTOR_X_MAX	50

#define		UPDATE_JOIN			1
#define		UPDATE_LEAVE		2
#define		UPDATE_PACKET		3

typedef struct st_UPDATE_MESSAGE
{
	st_UPDATE_MESSAGE()
	{
		iMsgType = NULL;
		pPacket = nullptr;
	}
	int				iMsgType;
	st_SessionInfo	ClientInfo;
	CPacket			*pPacket;
}UPMSG;

typedef struct st_SECTOR_POS
{
	st_SECTOR_POS()
	{
		shX = 10000;
		shY = 10000;
	}
	WORD shX;
	WORD shY;
}POS;

typedef struct st_SECTOR_AROUND
{
	int	iCount;
	POS	Around[9];
}SECTORAROUND;

typedef struct st_PLAYER
{
	st_PLAYER()
	{
		ClientNo = NULL;
		AccountNo = NULL;
		ID[0] = NULL;
		Nickname[0] = NULL;
		SessionKey[0] = NULL;
		ClientPort = NULL;
		DisconnectChatRecv = false;
		LastRecvPacket = GetTickCount64();
	}
	unsigned __int64	ClientNo;
	__int64		AccountNo;
	WCHAR		ID[20];				//	null포함
	WCHAR		Nickname[20];		//	null포함
	char		SessionKey[64];		//	로그인서버가 주는 세션키

	in_addr			ClientIP;
	unsigned short	ClientPort;
	POS				ClientPos;

	bool		DisconnectChatRecv;
	ULONG64		LastRecvPacket;
}PLAYER;

class CChatServer : public CNetServer
{
public:
	CChatServer();
	~CChatServer();

	void	OnClientJoin(st_SessionInfo Info);
	void	OnClientLeave(unsigned __int64 iClientNo);
	void	OnConnectionRequest(WCHAR *pClientIP, int iPort);
	void	OnError(int iErrorCode, WCHAR *pError);
	bool	OnRecv(unsigned __int64 iClientNo, CPacket *pPacket);
	void	MonitorThread_Update();

	PLAYER*	FindPlayer(unsigned __int64 iClientNo);
	bool	PacketProc(unsigned __int64 iClientNo, CPacket *pPacket);
	bool	InsertSectorPlayer(WORD shX, WORD shY, unsigned __int64 ClientNo);
	bool	DeleteSectorPlayer(WORD shX, WORD shY, unsigned __int64 ClientNo);
	void	GetSectorAround(WORD shX, WORD shY, SECTORAROUND *pSectorAround);
	bool	SendSector(WORD shX, WORD shY, CPacket *pPacket);
	bool	SendSectorAround(WORD shX, WORD shY, CPacket *pPacket);
	bool	BroadCast(CPacket *pPacket);

	UPMSG*	GetMessageQ();
	void	UpdateThread_Update();
	void	HeartBeatThread_Update();

private:
	static unsigned int WINAPI UpdateThread(LPVOID arg)
	{
		CChatServer *pUpdateThread = (CChatServer*)arg;
		if (NULL == pUpdateThread)
		{
			wprintf(L"[ChatServer :: UpdateThread]	Init Error\n");
			return false;
		}
		pUpdateThread->UpdateThread_Update();
		return true;
	}

	static unsigned int WINAPI HeartBeatThread(LPVOID arg)
	{
		CChatServer *pHeartBeatThread = (CChatServer*)arg;
		if (NULL == pHeartBeatThread)
		{
			wprintf(L"[ChatServer :: HeartBeatThread]	InitError\n");
			return false;
		}
		pHeartBeatThread->HeartBeatThread_Update();
		return true;
	}
	
private:
	map<unsigned __int64, PLAYER*> m_Playermap;
	list<unsigned __int64> m_Sector[SECTOR_Y_MAX][SECTOR_X_MAX];

	CLockFreeQueue<UPMSG*> m_UpdateMessageQ;
	CMemoryPool<UPMSG> *m_UpdateMessagePool;
	CMemoryPool<PLAYER> *m_PlayerPool;

	HANDLE	m_hThread[2];
	HANDLE	m_hHeartBeatThread;
	HANDLE	m_hUpdateThread;
	HANDLE	m_Event;

	unsigned __int64	m_iUpdateTPS;
	bool	m_bClose;
};

#endif _CHATSERVER_IOCP_CHATSERVER_H_
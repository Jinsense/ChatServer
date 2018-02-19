#ifndef _CHATSERVER_IOCP_NETSERVER_H_
#define _CHATSERVER_IOCP_NETSERVER_H_

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include "Packet.h"
#include "RingBuffer.h"
#include "MemoryPool.h"
#include "MemoryPool_TLS.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "Log.h"
#include "Dump.h"


#define		SERVERPORT				12001
#define		MAX_CLIENT_NUMBER		40000
#define		MAX_WORKER_THREAD		10
#define		TIMEOUT_TIME			30000

#define		MAX_WSABUF_NUMBER		300
#define		MAX_QUEUE_SIZE			4000

#define		SET_INDEX(Index, SessionKey)		Index = Index << 48; SessionKey = Index | SessionKey;
#define		GET_INDEX(Index, SessionKey)		Index = SessionKey >> 48;

struct st_SessionInfo
{
	st_SessionInfo() :
		iSessionKey(NULL) {}

	unsigned __int64 iSessionKey;
	in_addr		SessionIP;
	unsigned short	SessionPort;
};

struct st_IO_RELEASE_COMPARE
{
	__int64	iIOCount;
	__int64	iRelease;

	st_IO_RELEASE_COMPARE() :
		iIOCount(0),
		iRelease(false) {}
};

struct st_Session
{
	long				lSendFlag;
	long				lSendCount;
	unsigned __int64	iSessionKey;
	SOCKET				sock;
	OVERLAPPED			SendOver;
	OVERLAPPED			RecvOver;
	CRingBuffer			RecvQ;
	CRingBuffer			PacketQ;
	CLockFreeQueue<CPacket*> SendQ;
	st_SessionInfo		Info;
	st_IO_RELEASE_COMPARE	*Compare;

	st_Session() :
		RecvQ(MAX_QUEUE_SIZE),
		PacketQ(MAX_QUEUE_SIZE),
//		lIOCount(0),
		lSendFlag(false){}
};

class CNetServer
{
public:
	CNetServer();
	~CNetServer();

	void				Disconnect(unsigned __int64 iSessionKey);
	virtual void		OnClientJoin(st_SessionInfo Info) = 0;
	virtual void		OnClientLeave(unsigned __int64 iSessionKey) = 0;
	virtual void		OnConnectionRequest(WCHAR * pClientIP, int iPort) = 0;	
	virtual void		OnError(int iErrorCode, WCHAR *pError) = 0;
	virtual bool		OnRecv(unsigned __int64 iSessionKey, CPacket *pPacket) = 0;
	unsigned __int64	GetClientCount();

	bool				ServerStart(const WCHAR *pOpenIP, int iPort, int iMaxWorkerThread, 
								bool bNodelay, int iMaxSession);
	bool				ServerStop();
	bool				SendPacket(unsigned __int64 iSessionKey, CPacket *pPacket);
	bool				GetShutDownMode() { return m_bShutdown; }
	bool				GetWhiteIPMode() { return m_bWhiteIPMode; }
	bool				GetMonitorMode() { return m_bMonitorFlag; }
	bool				SetShutDownMode(bool bFlag);
	bool				SetWhiteIPMode(bool bFlag);
	bool				SetMonitorMode(bool bFlag);

	st_Session*			SessionAcquireLock(unsigned __int64 SessionKey);
	bool				SessionAcquireFree(st_Session *pSession);

private:
	bool				ServerInit();
	bool				ClientShutdown(st_Session *pSession);
	bool				ClientRelease(st_Session *pSession);

	static unsigned int WINAPI WorkerThread(LPVOID arg)
	{
		CNetServer *_pWorkerThread = (CNetServer *)arg;
		if (NULL == _pWorkerThread)
		{
			wprintf(L"[Server :: WorkerThread]	Init Error\n");
			return false;
		}
		_pWorkerThread->WorkerThread_Update();
		return true;
	}

	static unsigned int WINAPI AcceptThread(LPVOID arg)
	{
		CNetServer *_pAcceptThread = (CNetServer*)arg;
		if (NULL == _pAcceptThread)
		{
			wprintf(L"[Server :: AcceptThread]	Init Error\n");
			return false;
		}
		_pAcceptThread->AcceptThread_Update();
		return true;
	}

	static unsigned int WINAPI MonitorThread(LPVOID arg)
	{
		CNetServer *_pMonitorThread = (CNetServer*)arg;
		if (NULL == _pMonitorThread)
		{
			wprintf(L"[Server :: MonitorThread]	Init Error\n");
			return false;
		}
		_pMonitorThread->MonitorThread_Update();
		return true;
	}

	void				PutIndex(unsigned __int64 iIndex);
	void				WorkerThread_Update();
	void				AcceptThread_Update();
	virtual void		MonitorThread_Update() = 0;
	void				StartRecvPost(st_Session *pSession);
	void				RecvPost(st_Session *pSession);
	void				SendPost(st_Session *pSession);
	void				CompleteRecv(st_Session *pSession, DWORD dwTransfered);
	void				CompleteSend(st_Session *pSession, DWORD dwTransfered);
	unsigned __int64*	GetIndex();

private:
	CLockFreeStack<UINT64*>	SessionStack; 
//	st_IO_RELEASE_COMPARE	*pIOCompare;
	st_Session				*pSessionArray;
	SOCKET					m_listensock;
	CRITICAL_SECTION		m_SessionCS;

	HANDLE					m_hIOCP;
	HANDLE					m_hWorkerThread[100];
	HANDLE					m_hAcceptThread;
	HANDLE					m_hMonitorThread;
	HANDLE					m_hAllthread[200];
	
	bool					m_bWhiteIPMode;
	bool					m_bShutdown;
	
	unsigned __int64		m_iAllThreadCnt;
	unsigned __int64		*pIndex;
	unsigned __int64		m_iSessionKeyCnt;
public:
	unsigned __int64		m_iAcceptTPS;
	unsigned __int64		m_iAcceptTotal;
	unsigned __int64		m_iRecvPacketTPS;
	unsigned __int64		m_iSendPacketTPS;
	unsigned __int64		m_iConnectClient;
	bool					m_bMonitorFlag;

	CSystemLog				*m_Log;
	SRWLOCK					m_srw;
};

#endif _CHATSERVER_IOCP_NETSERVER_H_
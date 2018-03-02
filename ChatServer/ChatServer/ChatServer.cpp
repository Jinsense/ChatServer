#include <time.h>
#include <process.h>

//#include "LanClientManager.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LanClientManager.h"


CChatServer::CChatServer()
{
	m_bClose = false;
	m_iUpdateTPS = 0;
	m_hThread[0] = m_hHeartBeatThread;
	m_hThread[1] = m_hUpdateThread;
	m_hHeartBeatThread = (HANDLE)_beginthreadex(NULL, 0, &HeartBeatThread, (LPVOID)this, 0, NULL);
	m_hUpdateThread = (HANDLE)_beginthreadex(NULL, 0, &UpdateThread, (LPVOID)this, 0, NULL);
	m_Event = CreateEvent(NULL, false, false, NULL);

	m_LoginLanClient = new CLanClientManager;
	m_LoginLanClient->Constructor(this);
	m_PlayerPool = new CMemoryPool<PLAYER>();
	m_UpdateMessagePool = new CMemoryPool<UPMSG>();
}

CChatServer::~CChatServer()
{
	m_bClose = true;
	WaitForMultipleObjects(2, m_hThread, true, INFINITE);
	CloseHandle(m_hThread);
	CloseHandle(m_hHeartBeatThread);
	CloseHandle(m_hUpdateThread);

	delete m_PlayerPool;
	delete m_UpdateMessagePool;
}

void CChatServer::OnClientJoin(st_SessionInfo Info)
{
	UPMSG *pMsg = m_UpdateMessagePool->Alloc();
	pMsg->iMsgType = UPDATE_JOIN;
	pMsg->ClientInfo = Info;
	pMsg->pPacket = nullptr;

	m_UpdateMessageQ.Enqueue(pMsg);
	SetEvent(m_Event);
	return;
}

void CChatServer::OnClientLeave(unsigned __int64 iClientNo)
{
	st_SessionInfo Info;
	Info.iSessionKey = iClientNo;
	UPMSG *pMsg = m_UpdateMessagePool->Alloc();
	pMsg->iMsgType = UPDATE_LEAVE;
	pMsg->ClientInfo = Info;
	pMsg->pPacket = nullptr;

	m_UpdateMessageQ.Enqueue(pMsg);
	SetEvent(m_Event);
	return;
}

void CChatServer::OnConnectionRequest(WCHAR *pClientIP, int iPort)
{

}

void CChatServer::OnError(int iErrorCode, WCHAR *pError)
{
	
}

bool CChatServer::OnRecv(unsigned __int64 iClientNo, CPacket *pPacket)
{
	m_iRecvPacketTPS++;

	st_SessionInfo Info;
	Info.iSessionKey = iClientNo;
	UPMSG *pMsg = m_UpdateMessagePool->Alloc();
	pMsg->iMsgType = UPDATE_PACKET;
	pMsg->ClientInfo = Info;
	pMsg->pPacket = pPacket;

	pPacket->AddRef();
	m_UpdateMessageQ.Enqueue(pMsg);
	SetEvent(m_Event);
	return true;
}

void CChatServer::MonitorThread_Update()
{
	wprintf(L"\n");

	struct tm * _t = new struct tm;
	time_t _timer;

	_timer = time(NULL);
	localtime_s(_t, &_timer);

	int year = _t->tm_year + 1900;
	int month = _t->tm_mon + 1;
	int day = _t->tm_mday;
	int hour = _t->tm_hour;
	int min = _t->tm_min;
	int sec = _t->tm_sec;

	while (1)
	{
		Sleep(1000);
		_timer = time(NULL);
		localtime_s(_t, &_timer);
		if (true == m_bMonitorFlag)
		{
			wprintf(L"	[ServerStart : %d/%d/%d %d:%d:%d]\n\n", year, month, day, hour, min, sec);
			wprintf(L"	[%d/%d/%d %d:%d:%d]\n\n", _t->tm_year + 1900, _t->tm_mon + 1,
				_t->tm_mday, _t->tm_hour, _t->tm_min, _t->tm_sec);
			wprintf(L"	ConnectSession			:	%I64d	\n", m_iConnectClient);
			wprintf(L"	MemoryPool_AllocCount		:	%I64d	\n", CPacket::GetAllocPool());
//			wprintf(L"	MemoryPool_UseCount		:	%I64d	\n\n", CPacket::GetUsePool());

			wprintf(L"	UpdateThreadMSG_AllocCount	:	%d	\n", m_UpdateMessagePool->GetAllocCount());
			wprintf(L"	UpdateThreadQ_UseCount		:	%d	\n\n", m_UpdateMessageQ.GetUseCount());

			wprintf(L"	Player_AllocCount		:	%d	\n", m_PlayerPool->GetAllocCount());
			wprintf(L"	Player_UseCount			:	%d	\n\n", m_PlayerPool->GetUseCount());

			//	로그인세션키 - 미사용	
			wprintf(L"	LoginSessionKey			:	%d	\n\n", 0);
			wprintf(L"	Accept_Total			:	%I64d	\n", m_iAcceptTotal);
			wprintf(L"	Accept_TPS			:	%I64d	\n", m_iAcceptTPS);
			wprintf(L"	Update_TPS			:	%I64d	\n", m_iUpdateTPS);
			wprintf(L"	SendPacket_TPS			:	%I64d	\n", m_iSendPacketTPS);
			wprintf(L"	RecvPacket_TPS			:	%I64d	\n\n", m_iRecvPacketTPS);

			//	세션miss - 미사용
			wprintf(L"	SessionMiss			:	%d	\n", 0);
			//	세션notfound - 미사용
			wprintf(L"	SessionNotFound			:	%d	\n\n", 0);
		}
		m_iAcceptTPS = 0;
		m_iRecvPacketTPS = 0;
		m_iSendPacketTPS = 0;
		m_iUpdateTPS = 0;
	}
	delete _t;
}

PLAYER* CChatServer::FindPlayer(unsigned __int64 iClientNo)
{
	map<unsigned __int64, PLAYER*>::iterator iter;
	iter = m_Playermap.find(iClientNo);
	if (iter == m_Playermap.end())
		return nullptr;
	else
		return iter->second;
}

bool CChatServer::PacketProc(unsigned __int64 iClientNo, CPacket *pPacket)
{
	if (0 == pPacket->GetDataSize())
	{
		Disconnect(iClientNo);
		return false;
	}
	WORD Type;
	pPacket->PopData((char*)&Type, sizeof(WORD));

	switch (Type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
	{
		PLAYER *pPlayer = FindPlayer(iClientNo);
		if (nullptr == pPlayer || 152 != pPacket->GetDataSize())
		{
			CPacket *pNewPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_CHAT_RES_LOGIN;
			BYTE Status = 0;
			INT64 AccountNo = pPlayer->AccountNo;
			*pNewPacket << Type << Status << AccountNo;
			SendPacket(iClientNo, pNewPacket);
			pNewPacket->Free();
			Disconnect(iClientNo);
		}
		else
		{
			pPlayer->LastRecvPacket = GetTickCount64();

			*pPacket >> pPlayer->AccountNo;
			pPacket->PopData((char*)pPlayer->ID, sizeof(pPlayer->ID));
			pPacket->PopData((char*)pPlayer->Nickname, sizeof(pPlayer->Nickname));
			pPacket->PopData((char*)pPlayer->SessionKey, sizeof(pPlayer->SessionKey));

			CPacket *pNewPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_CHAT_RES_LOGIN;
			BYTE Status = 1;
			INT64 AccountNo = pPlayer->AccountNo;
			*pNewPacket << Type << Status << AccountNo;
			SendPacket(iClientNo, pNewPacket);
			pNewPacket->Free();
		}
	}
	break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
	{
		PLAYER *pPlayer = FindPlayer(iClientNo);
		if (nullptr == pPlayer || 12 != pPacket->GetDataSize())
			Disconnect(iClientNo);
		else
		{
			pPlayer->LastRecvPacket = GetTickCount64();

			INT64 AccountNo;
			WORD shX;
			WORD shY;
			*pPacket >> AccountNo >> shX >> shY;

			if (pPlayer->AccountNo != AccountNo)
			{
				m_Log->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM,
					const_cast<WCHAR*>(L"REQ_SECTOR_MOVE - AccountNo Wrong [Pakcet AccountNo : %d, My AccountNo : %d]"), AccountNo, pPlayer->AccountNo);
				Disconnect(iClientNo);
				break;
			}

			if (100 <= shX || 100 <= shY)
			{
				m_Log->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM,
					const_cast<WCHAR*>(L"REQ_SECTOR_MOVE - Pos Wrong [X : %d, Y : %d]"), shX, shY);
				Disconnect(iClientNo);
				break;
			}

			if(10000 != pPlayer->ClientPos.shX && 10000 != pPlayer->ClientPos.shY)
				DeleteSectorPlayer(pPlayer->ClientPos.shX, pPlayer->ClientPos.shY, iClientNo);

			pPlayer->ClientPos.shX = shX;
			pPlayer->ClientPos.shY = shY;
			InsertSectorPlayer(shX, shY, iClientNo);

			CPacket *pNewPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
			WORD SectorX = shX;
			WORD SectorY = shY;
			*pNewPacket << Type << AccountNo << shX << shY;
			SendPacket(iClientNo, pNewPacket);
			pNewPacket->Free();
		}
	}
	break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
	{
		PLAYER *pPlayer = FindPlayer(iClientNo);
		if (nullptr == pPlayer || pPacket->m_header.shLen != sizeof(WORD) + pPacket->GetDataSize())
		{
			Disconnect(iClientNo);
		}
		else
		{
			INT64 AccountNo;
			*pPacket >> AccountNo;
			if (pPlayer->AccountNo != AccountNo)
			{
				Disconnect(iClientNo);
				break;
			}
			pPlayer->LastRecvPacket = GetTickCount64();

			WORD Len;
			*pPacket >> Len;

			WORD Check = pPacket->m_header.shLen - 12;

			if (Len != Check)
			{
				Disconnect(iClientNo);
				break;
			}

			if (10000 < Len )
			{
				m_Log->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM,
					const_cast<WCHAR*>(L"CHAT_REQ_MESSAGE - Pakcet Len is wrong"));
				break;
			}

			WCHAR * pMsg = new WCHAR[Len / 2];			
			pPacket->PopData((char*)pMsg, Len);			

			CPacket *pNewPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;

			*pNewPacket << Type << pPlayer->AccountNo;
			pNewPacket->PushData((char*)pPlayer->ID, sizeof(pPlayer->ID));
			pNewPacket->PushData((char*)pPlayer->Nickname, sizeof(pPlayer->Nickname));
			*pNewPacket << Len;
			pNewPacket->PushData(pMsg, Len / 2);

//			SendPacket(iClientNo, pNewPacket);
			SendSectorAround(pPlayer->ClientPos.shX, pPlayer->ClientPos.shY, pNewPacket);
			pNewPacket->Free();

			delete[] pMsg;
		}
	}
	break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
	{
		PLAYER *pPlayer = FindPlayer(iClientNo);
		if (nullptr == pPlayer)
			Disconnect(iClientNo);
		else
			pPlayer->LastRecvPacket = GetTickCount64();
	}
	break;
	default:
		break;
	}
	return true;
}

bool CChatServer::InsertSectorPlayer(WORD shX, WORD shY, unsigned __int64 iClientNo)
{
	m_Sector[shY][shX].push_back(iClientNo);
	return true;
}

bool CChatServer::DeleteSectorPlayer(WORD shX, WORD shY, unsigned __int64 iClientNo)
{
	if (10000 == shX && 10000 == shY)
		return false;
	else
		m_Sector[shY][shX].remove(iClientNo);
	return true;
}

void CChatServer::GetSectorAround(WORD shX, WORD shY, SECTORAROUND *pSectorAround)
{
	short shCntX, shCntY;

	pSectorAround->iCount = 0;

	for (shCntY = -1; shCntY < 2; shCntY++)
	{
		if (shY + shCntY < 0 || shY + shCntY >= SECTOR_Y_MAX)
			continue;

		for (shCntX = -1; shCntX < 2; shCntX++)
		{
			if (shX + shCntX < 0 || shX + shCntX >= SECTOR_X_MAX)
				continue;

			pSectorAround->Around[pSectorAround->iCount].shX = shX + shCntX;
			pSectorAround->Around[pSectorAround->iCount].shY = shY + shCntY;
			pSectorAround->iCount++;
		}
	}
	return;
}

bool CChatServer::SendSector(WORD shX, WORD shY, CPacket *pPacket)
{
	list<unsigned __int64>::iterator iter;
	for (iter = m_Sector[shY][shX].begin(); iter != m_Sector[shY][shX].end(); iter++)
	{
		pPacket->AddRef();
		SendPacket(*iter, pPacket);
		pPacket->Free();
	}
	return true;
}

bool CChatServer::SendSectorAround(WORD shX, WORD shY, CPacket *pPacket)
{
	SECTORAROUND AroundSector;
	GetSectorAround(shX, shY, &AroundSector);
	pPacket->AddRef();
	for (int iCnt = 0; iCnt < AroundSector.iCount; iCnt++)
	{
		SendSector(AroundSector.Around[iCnt].shX, AroundSector.Around[iCnt].shY, pPacket);
	}
	pPacket->Free();
	return true;
}

bool CChatServer::BroadCast(CPacket *pPacket)
{
	map<unsigned __int64, PLAYER*>::iterator iter;
	for (iter = m_Playermap.begin(); iter != m_Playermap.end(); iter++)
	{
		SendPacket(iter->first, pPacket);
	}
	return true;
}

UPMSG* CChatServer::GetMessageQ()
{
	if (0 == m_UpdateMessageQ.GetUseCount())
		return nullptr;
	UPMSG *pMsg;
	m_UpdateMessageQ.Dequeue(pMsg);
	return pMsg;
}

void CChatServer::UpdateThread_Update()
{
	while (!m_bClose)
	{
		WaitForSingleObject(m_Event, INFINITE);
		while (0 < m_UpdateMessageQ.GetUseCount())
		{
			UPMSG *pMsg = GetMessageQ();
			if (nullptr == pMsg)
				continue;

			m_iUpdateTPS++;

			switch (pMsg->iMsgType)
			{
			case UPDATE_JOIN:
			{
				PLAYER *pPlayer = m_PlayerPool->Alloc();
				pPlayer->ClientNo = pMsg->ClientInfo.iSessionKey;
				pPlayer->ClientIP = pMsg->ClientInfo.SessionIP;
				pPlayer->ClientPort = pMsg->ClientInfo.SessionPort;
				pPlayer->LastRecvPacket = GetTickCount64();
				m_Playermap.insert({pPlayer->ClientNo, pPlayer});
			}
			break;
			case UPDATE_LEAVE:
			{
				PLAYER *pPlayer = FindPlayer(pMsg->ClientInfo.iSessionKey);
				if (nullptr == pPlayer)
				{
					m_Log->Log(const_cast<WCHAR*>(L"Debug"), LOG_SYSTEM,
						const_cast<WCHAR*>(L"UPDATE_LEAVE - Session Not Find [SessionKey : %d]"), pMsg->ClientInfo.iSessionKey);
					break;
				}
				if (pPlayer->ClientPos.shX != 10000 && pPlayer->ClientPos.shY != 10000)
					DeleteSectorPlayer(pPlayer->ClientPos.shX, pPlayer->ClientPos.shY, pPlayer->ClientNo);
				m_Playermap.erase(pMsg->ClientInfo.iSessionKey);
				m_PlayerPool->Free(pPlayer);
			}
			break;
			case UPDATE_PACKET:
			{
				PacketProc(pMsg->ClientInfo.iSessionKey, pMsg->pPacket);
				pMsg->pPacket->Free();
			}
			break;
			default:
				break;
			}
			m_UpdateMessagePool->Free(pMsg);
		}
	}
	return;
}

void CChatServer::HeartBeatThread_Update()
{
	while (!m_bClose)
	{
		Sleep(10000);
	//	2~5초 사이로 업데이트 메세지를 생성해서 큐에 넣는다.
	//	동기화(락을 없게 하기 위해)
	
	}
	return;
}

bool CChatServer::LoginServerConnect(char * ServerIP, int Port, bool NoDelay, int MaxWorkerThread)
{
	return m_LoginLanClient->Connect(ServerIP, Port, NoDelay, MaxWorkerThread);
}

bool CChatServer::LoginServerDisConnect()
{
	return m_LoginLanClient->Disconnect();
}
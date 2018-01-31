#include <WinSock2.h>
#include <windows.h>
#include <wchar.h>
#include <Ws2tcpip.h>
#include <process.h>
#include <time.h>
#include <iostream>

#include "NetServer.h"

using namespace std;

CNetServer::CNetServer()
{
	pIOCompare = nullptr;
	pSessionArray = nullptr;
	m_listensock = INVALID_SOCKET;
	
	m_bWhiteIPMode = false;
	m_bShutdown = false;
	m_bMonitorFlag = true;
	m_iAllThreadCnt = 0;
	pIndex = nullptr;
	m_iSessionKeyCnt = 1;
	m_iAcceptTPS = 0;
	m_iAcceptTotal = 0;
	m_iRecvPacketTPS = 0;
	m_iSendPacketTPS = 0;
	m_iConnectClient = 0;
}

CNetServer::~CNetServer()
{

}

bool CNetServer::ServerStart(const WCHAR *pOpenIP, int iPort, int iMaxWorkerThread,
					bool bNodelay, int iMaxSession)
{
	wprintf(L"[Server :: Server_Start]	Start\n");

	int _iRetval = 0;
	setlocale(LC_ALL, "Korean");
	InitializeCriticalSection(&m_SessionCS);

	CPacket::MemoryPoolInit();

	pIOCompare = (st_IO_RELEASE_COMPARE*)_aligned_malloc(sizeof(st_IO_RELEASE_COMPARE), 16);
	pIOCompare->iIOCount = 0;
	pIOCompare->iReleaseFlag = false;

	pSessionArray = new st_Session[iMaxSession];
	
	for (int i = 0; i < iMaxSession; i++)
	{
		pSessionArray[i].lIOCount = 0;
		pSessionArray[i].sock = INVALID_SOCKET;
		pSessionArray[i].lSendCount = 0;
		pSessionArray[i].iSessionKey = NULL;
		pSessionArray[i].lSendFlag = true;
		pSessionArray[i].bLoginFlag = false;
		pSessionArray[i].bRelease = false;
	}

	pIndex = new unsigned __int64[iMaxSession];
	for (unsigned __int64 i = 0; i < iMaxSession; i++)
	{
		pIndex[i] = i;
		SessionStack.Push(&pIndex[i]);
	}

	if (false == (_iRetval = ServerInit()))
	{
		return false;
	}

	struct sockaddr_in _server_addr;
	ZeroMemory(&_server_addr, sizeof(_server_addr));
	_server_addr.sin_family = AF_INET;
	InetPton(AF_INET, pOpenIP, &_server_addr.sin_addr);
	_server_addr.sin_port = htons(iPort);

	_iRetval = ::bind(m_listensock, (sockaddr *)&_server_addr, sizeof(_server_addr));
	if (_iRetval == SOCKET_ERROR)
	{
		return false;
	}

	_iRetval = listen(m_listensock, SOMAXCONN);
	if (_iRetval == SOCKET_ERROR)
	{
		return false;
	}

	m_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, &AcceptThread, 
					(LPVOID)this, 0, NULL);
	m_hAllthread[m_iAllThreadCnt++] = m_hAcceptThread;
	wprintf(L"[Server :: Server_Start]	AcceptThread Create\n");

	for (int i = 0; i < iMaxWorkerThread; i++)
	{
		m_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, 
						(LPVOID)this, 0, NULL);
		m_hAllthread[m_iAllThreadCnt++] = m_hWorkerThread[i];
	}
	wprintf(L"[Server :: Server_Start]	WorkerThread Create\n");

	m_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, &MonitorThread, 
						(LPVOID)this, 0, NULL);
	m_hAllthread[m_iAllThreadCnt++] = m_hMonitorThread;
	wprintf(L"[Server :: Server_Start]	MonitorThread Create\n");
	wprintf(L"[Server :: Server_Start]	Complete\n");
	return true;
}

bool CNetServer::ServerStop()
{
	wprintf(L"[Server :: Server_Stop]	Start\n");

	m_bWhiteIPMode = true;
	_aligned_free(pIOCompare);
	delete pSessionArray;
	delete pIndex;

	wprintf(L"([Server :: Server_Stop]	Complete\n");
	return true;
}

void CNetServer::Disconnect(unsigned __int64 iSessionKey)
{
	unsigned __int64  _iIndex = iSessionKey >> 48;
	st_Session *_pSession = &pSessionArray[_iIndex];

	InterlockedIncrement(&_pSession->lIOCount);

	if (true == _pSession->bRelease || iSessionKey != _pSession->iSessionKey)
	{
		if (0 == InterlockedDecrement(&_pSession->lIOCount))
			ClientRelease(_pSession);
		return;
	}
	shutdown(_pSession->sock, SD_BOTH);

	if (0 == InterlockedDecrement(&_pSession->lIOCount))
		ClientRelease(_pSession);

	return;
}

unsigned __int64 CNetServer::GetClientCount()
{
	return m_iConnectClient;
}

bool CNetServer::SendPacket(unsigned __int64 iSessionKey, CPacket *pPacket)
{
	unsigned __int64 _iIndex = GET_INDEX(_iIndex, iSessionKey);

	if( 1 == InterlockedIncrement(&pSessionArray[_iIndex].lIOCount))
	{
		if (0 == InterlockedDecrement(&pSessionArray[_iIndex].lIOCount))
		{
			ClientRelease(&pSessionArray[_iIndex]);
		}
		return false;
	}
	
	if (true == pSessionArray[_iIndex].bRelease)
	{
		if (0 == InterlockedDecrement(&pSessionArray[_iIndex].lIOCount))
		{
			ClientRelease(&pSessionArray[_iIndex]);
		}
		return false;
	}

	if (pSessionArray[_iIndex].iSessionKey == iSessionKey)
	{
		if (pSessionArray[_iIndex].bLoginFlag != true)
			return false;

		m_iSendPacketTPS++;
		pPacket->AddRef();
		pPacket->EnCode();
//		pPacket->SetHeader_CustomShort(pPacket->GetDataSize());
		pSessionArray[_iIndex].SendQ.Enqueue(pPacket);

		if (0 == InterlockedDecrement(&pSessionArray[_iIndex].lIOCount))
		{
			ClientRelease(&pSessionArray[_iIndex]);
			return false;
		}
		SendPost(&pSessionArray[_iIndex]);
		return true;
	}

	if (0 == InterlockedDecrement(&pSessionArray[_iIndex].lIOCount))
		ClientRelease(&pSessionArray[_iIndex]);
	return false;
}

bool CNetServer::ServerInit()
{
	WSADATA _Data;
	int _iRetval = WSAStartup(MAKEWORD(2, 2), &_Data);
	if (0 != _iRetval)
	{
		wprintf(L"[Server :: Server_Init]	WSAStartup Error\n");
		return false;
	}

	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	if (m_hIOCP == NULL)
	{
		wprintf(L"[Server :: Server_Init]	IOCP Init Error\n");
		return false;
	}

	m_listensock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
						NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listensock == INVALID_SOCKET)
	{
		wprintf(L"[Server :: Server_Init]	Listen Socket Init Error\n");
		return false;
	}
	wprintf(L"[Server :: Server_Init]		Complete\n");
	return true;
}

bool CNetServer::ClientShutdown(st_Session *pSession)
{
	int _iRetval;
	_iRetval = shutdown(pSession->sock, SD_BOTH);
	if (false == _iRetval)
	{
		return false;
	}
	return true;
}

bool CNetServer::ClientRelease(st_Session *pSession)
{
	st_IO_RELEASE_COMPARE _CheckFlag;
	_CheckFlag.iIOCount = pSession->lIOCount;
	_CheckFlag.iReleaseFlag = pSession->bRelease;

	if (FALSE == InterlockedCompareExchange128((LONG64*)pIOCompare, _CheckFlag.iIOCount, 
								_CheckFlag.iReleaseFlag, (LONG64*)&_CheckFlag))
		return FALSE;

	pSession->bRelease = TRUE;
	closesocket(pSession->sock);

	OnClientLeave(pSession->iSessionKey);

	while (0 < pSession->SendQ.GetUseCount())
	{
		CPacket *_pPacket;
		pSession->SendQ.Dequeue(&_pPacket);
		_pPacket->Free();
	}

	while (0 < pSession->PacketQ.GetUseSize())
	{
		CPacket *_pPacket;
		pSession->PacketQ.Peek((char*)&_pPacket,sizeof(CPacket*));
		_pPacket->Free();
		pSession->PacketQ.Dequeue(sizeof(CPacket*));
	}

	pSession->bLoginFlag = false;

	unsigned __int64 iSessionKey = pSession->iSessionKey;
	unsigned __int64 iIndex = GET_INDEX(iIndex, iSessionKey);

	InterlockedDecrement(&m_iConnectClient);
	PutIndex(iIndex);
	return true;
}

void CNetServer::WorkerThread_Update()
{
	DWORD _dwRetval;

	while (1)
	{
		OVERLAPPED * _pOver = NULL;
		st_Session * _pSession = NULL;
		DWORD _dwTrans = 0;

		_dwRetval = GetQueuedCompletionStatus(m_hIOCP, &_dwTrans, (PULONG_PTR)&_pSession,
									(LPWSAOVERLAPPED*)&_pOver, INFINITE);
		if (nullptr == _pOver)
		{
			if (nullptr == _pSession && 0 == _dwTrans)
			{
				PostQueuedCompletionStatus(m_hIOCP, 0, 0, 0);
			}
		}

		if (0 == _dwTrans)
		{
			shutdown(_pSession->sock, SD_BOTH);
		}
		else if (_pOver == &_pSession->RecvOver)
		{
			CompleteRecv(_pSession, _dwTrans);
		}
		else if (_pOver == &_pSession->SendOver)
		{
			CompleteSend(_pSession, _dwTrans);
		}

		if (0 >= (_dwRetval =InterlockedDecrement(&_pSession->lIOCount)))
		{
			if (0 == _dwRetval)
				ClientRelease(_pSession);
		}
	}
}

void CNetServer::AcceptThread_Update()
{
	DWORD _dwRetval = 0;
	SOCKADDR_IN _ClientAddr;
	
	while (1)
	{
		int addrSize = sizeof(_ClientAddr);
		SOCKET clientSock = accept(m_listensock, (SOCKADDR*)&_ClientAddr, &addrSize);
		if (INVALID_SOCKET == clientSock)
			break;

		InterlockedIncrement(&m_iAcceptTPS);
		InterlockedIncrement(&m_iAcceptTotal);

		OnConnectionRequest((WCHAR*)&_ClientAddr.sin_addr, _ClientAddr.sin_port);

		unsigned __int64 * _iSessionNum = GetIndex();
		if (_iSessionNum == nullptr)
		{
			closesocket(clientSock);
			continue;
		}

		if (pSessionArray[*_iSessionNum].bLoginFlag == TRUE)
		{
			ClientRelease(&pSessionArray[*_iSessionNum]);
			continue;
		}

		unsigned __int64 iIndex = *_iSessionNum;

		pSessionArray[*_iSessionNum].iSessionKey = m_iSessionKeyCnt++;
		SET_INDEX(iIndex, pSessionArray[*_iSessionNum].iSessionKey);
		pSessionArray[*_iSessionNum].sock = clientSock;
		pSessionArray[*_iSessionNum].RecvQ.Clear();
		pSessionArray[*_iSessionNum].PacketQ.Clear();
		pSessionArray[*_iSessionNum].lSendFlag = TRUE;
		pSessionArray[*_iSessionNum].lSendCount = 0;
		InterlockedIncrement(&m_iConnectClient);
		pSessionArray[*_iSessionNum].bLoginFlag = TRUE;
		pSessionArray[*_iSessionNum].Info.iSessionKey = 
			pSessionArray[*_iSessionNum].iSessionKey;
		pSessionArray[*_iSessionNum].bRelease = FALSE;

		InterlockedIncrement(&pSessionArray[*_iSessionNum].lIOCount);
		CreateIoCompletionPort((HANDLE)clientSock, m_hIOCP, 
						(ULONG_PTR)&pSessionArray[*_iSessionNum], 0);
		OnClientJoin(pSessionArray[*_iSessionNum].Info);
		StartRecvPost(&pSessionArray[*_iSessionNum]);
	}
}

//void CNetServer::MonitorThread_Update()
//{
//	wprintf(L"\n");
//
//	struct tm * _t = new struct tm;
//	time_t _timer;
//
//	while (1)
//	{
//		Sleep(1000);
//		_timer = time(NULL);
//		localtime_s(_t, &_timer);
//		if (m_bMonitorFlag == TRUE)
//		{
//			wprintf(L"[%d/%d/%d %d:%d:%d]\n", _t->tm_year + 1900, _t->tm_mon + 1, 
//				_t->tm_mday, _t->tm_hour, _t->tm_min, _t->tm_sec);
//			wprintf(L"[ConnectSession		:	%I64d]\n", m_iConnectClient);
//			wprintf(L"[Accept_TPS		:	%I64d]\n", m_iAcceptTPS);
//			wprintf(L"[Accept_Total		:	%I64d]\n", m_iAcceptTotal);
//			wprintf(L"[RecvPacket_TPS		:	%I64d]\n", m_iRecvPacketTPS);
//			wprintf(L"[SendPacket_TPS		:	%I64d]\n", m_iSendPacketTPS);
// 			wprintf(L"[MemoryPool_AllocCount	:	%I64d]\n", CPacket::GetAllocPool());
//			wprintf(L"[MemoryPool_UseCount	:	%I64d]\n\n", CPacket::GetUsePool());
//		}
//		m_iAcceptTPS = 0;
//		m_iRecvPacketTPS = 0;
//		m_iSendPacketTPS = 0;
//	}
//	delete _t;
//}

st_Session* CNetServer::SessionAcquireLock(unsigned __int64 iSessionKey)
{
	unsigned __int64 _iIndex = iSessionKey >> 48;
	st_Session *_pSession = &pSessionArray[_iIndex];
	InterlockedIncrement(&_pSession->lIOCount);

	return _pSession;
}

void CNetServer::SessionAcquireFree(st_Session *pSession)
{
	DWORD _dwRetval;
	if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
	{
		if (0 == _dwRetval)
			ClientRelease(pSession);
	}
}

void CNetServer::StartRecvPost(st_Session *pSession)
{
	DWORD _dwRetval = 0;
	DWORD _dwFlags = 0;
	ZeroMemory(&pSession->RecvOver, sizeof(pSession->RecvOver));

	WSABUF _Buf[2];
	DWORD _dwFreeSize = pSession->RecvQ.GetFreeSize();
	DWORD _dwNotBrokenPushSize = pSession->RecvQ.GetNotBrokenPushSize();
	if (0 == _dwFreeSize && 0 == _dwNotBrokenPushSize)
	{
		if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
		{
			if (0 == _dwRetval)
				ClientRelease(pSession);
		}
		else
			shutdown(pSession->sock, SD_BOTH);
		return;
	}
	int _iNumOfBuf = (_dwNotBrokenPushSize < _dwFreeSize) ? 2 : 1;

	_Buf[0].buf = pSession->RecvQ.GetWriteBufferPtr();
	_Buf[0].len = _dwNotBrokenPushSize;

	if (2 == _iNumOfBuf)
	{
		_Buf[1].buf = pSession->RecvQ.GetBufferPtr();
		_Buf[1].len = _dwFreeSize - _dwNotBrokenPushSize;
	}
	if (SOCKET_ERROR == WSARecv(pSession->sock, _Buf, _iNumOfBuf, 
							NULL, &_dwFlags, &pSession->RecvOver, NULL))
	{
		int _iLastError = WSAGetLastError();
		if (ERROR_IO_PENDING != _iLastError)
		{
			if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
			{
				if (0 == _dwRetval)
					ClientRelease(pSession);
			}
			else
				shutdown(pSession->sock, SD_BOTH);
		}
	}
}

void CNetServer::RecvPost(st_Session *pSession)
{
	InterlockedIncrement(&pSession->lIOCount);

	DWORD _dwRetval = 0;
	DWORD _dwFlags = 0;
	ZeroMemory(&pSession->RecvOver, sizeof(pSession->RecvOver));

	WSABUF _Buf[2];
	DWORD _dwFreeSize = pSession->RecvQ.GetFreeSize();
	DWORD _dwNotBrokenPushSize = pSession->RecvQ.GetNotBrokenPushSize();
	if (0 == _dwFreeSize && 0 == _dwNotBrokenPushSize)
	{
		if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
		{
			if (0 == _dwRetval)
				ClientRelease(pSession);
		}
		else
			shutdown(pSession->sock, SD_BOTH);
		return;
	}

	int _iNumOfBuf = (_dwNotBrokenPushSize < _dwFreeSize) ? 2 : 1;

	_Buf[0].buf = pSession->RecvQ.GetWriteBufferPtr();
	_Buf[0].len = _dwNotBrokenPushSize;

	if (2 == _iNumOfBuf)
	{
		_Buf[1].buf = pSession->RecvQ.GetBufferPtr();
		_Buf[1].len = _dwFreeSize - _dwNotBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(pSession->sock, _Buf, _iNumOfBuf, 
							NULL, &_dwFlags, &pSession->RecvOver, NULL))
	{
		int _iLastError = WSAGetLastError();
		if (ERROR_IO_PENDING != _iLastError)
		{
			if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
			{
				if (0 == _dwRetval)
					ClientRelease(pSession);
			}
			else
				shutdown(pSession->sock, SD_BOTH);
		}
	}
}

void CNetServer::SendPost(st_Session *pSession)
{
	DWORD _dwRetval;
	do 
	{
		if (false == InterlockedCompareExchange(&pSession->lSendFlag, false, true))		
			return;		

		if (0 == pSession->SendQ.GetUseCount())
		{
			InterlockedExchange(&pSession->lSendFlag, true);
			continue;
		}
		ZeroMemory(&pSession->SendOver, sizeof(pSession->SendOver));
		
		WSABUF _Buf[MAX_WSABUF_NUMBER];
		CPacket *_pPacket;
		long _lBufNum = 0;
		int _iUseSize = (pSession->SendQ.GetUseCount());
		if (_iUseSize > MAX_WSABUF_NUMBER)
		{
			_lBufNum = MAX_WSABUF_NUMBER;
			pSession->lSendCount = MAX_WSABUF_NUMBER;
			for (int i = 0; i < MAX_WSABUF_NUMBER; i++)
			{
				pSession->SendQ.Dequeue(&_pPacket);
				pSession->PacketQ.Enqueue((char*)&_pPacket, sizeof(CPacket*));
				_Buf[i].buf = _pPacket->GetBufferPtr();
				_Buf[i].len = _pPacket->GetPacketSize();
			}
		}
		else
		{
			_lBufNum = _iUseSize;
			pSession->lSendCount = _iUseSize;
			for (int i = 0; i < _iUseSize; i++)
			{	
				pSession->SendQ.Dequeue(&_pPacket);
				pSession->PacketQ.Enqueue((char*)&_pPacket, sizeof(CPacket*));
				_Buf[i].buf = _pPacket->GetBufferPtr();
				_Buf[i].len = _pPacket->GetPacketSize();
			}
		}
		InterlockedIncrement(&pSession->lIOCount);
		if (SOCKET_ERROR == WSASend(pSession->sock, _Buf, _lBufNum, 
								NULL, 0, &pSession->SendOver, NULL))
		{
			int _LastError = WSAGetLastError();
			if (ERROR_IO_PENDING != _LastError)
			{				  
				if (0 >= (_dwRetval = InterlockedDecrement(&pSession->lIOCount)))
				{
					if (0 == _dwRetval)
						ClientRelease(pSession);
				}
				else
					shutdown(pSession->sock, SD_BOTH);
			}
		}
	} while (0 != pSession->SendQ.GetUseCount());
}

void CNetServer::CompleteRecv(st_Session *pSession, DWORD dwTransfered)
{
	pSession->RecvQ.Enqueue(dwTransfered);
 
	while (0 < pSession->RecvQ.GetUseSize())
	{
		CPacket::st_PACKET_HEADER _Header;
		CPacket *_pPacket = CPacket::Alloc();
		
		pSession->RecvQ.Peek((char*)&_Header, sizeof(CPacket::st_PACKET_HEADER));

		pSession->RecvQ.Dequeue((char*)_pPacket->GetBufferPtr(), _Header.shLen + sizeof(CPacket::st_PACKET_HEADER));

		if (static_cast<int>(CPacket::en_PACKETDEFINE::PACKET_CODE) != _Header.byCode || 10000 < _Header.shLen)
		{
			shutdown(pSession->sock, SD_BOTH);
			_pPacket->Free();
			return;
		}

		_pPacket->PushData(_Header.shLen + sizeof(CPacket::st_PACKET_HEADER));

		if (false == _pPacket->DeCode((CPacket::st_PACKET_HEADER*)_pPacket->GetBufferPtr()))
		{
			shutdown(pSession->sock, SD_BOTH);
			_pPacket->Free();
			return;
		}
		
		_pPacket->PopData(sizeof(CPacket::st_PACKET_HEADER));

		if (false == OnRecv(pSession->iSessionKey, _pPacket))
		{
			_pPacket->Free();
			return;
		}

		_pPacket->Free();
	}
	RecvPost(pSession);
}

void CNetServer::CompleteSend(st_Session *pSession, DWORD dwTransfered)
{
	CPacket *_pPacket[MAX_WSABUF_NUMBER];
	pSession->PacketQ.Peek((char*)&_pPacket, sizeof(CPacket*) *pSession->lSendCount);
	for (int i = 0; i < pSession->lSendCount; i++)
	{
		_pPacket[i]->Free();
		pSession->PacketQ.Dequeue(sizeof(CPacket*));
	}

	InterlockedExchange(&pSession->lSendFlag, true);

	SendPost(pSession);
}

//bool CNetServer::OnRecv(st_Session *pSession, CPacket *pPacket)
//{
//	m_iRecvPacketTPS++;
//
//	__int64 _iData = 0;
//	*pPacket >> _iData;
//
//	CPacket *_pPacket = CPacket::Alloc();
//	*_pPacket << _iData;
//
//	bool retval = false;
//	if (pSession->bRelease == false)
//		retval = SendPacket(pSession->iSessionKey, _pPacket);
//	_pPacket->Free();
//
//	return retval;
//}

bool CNetServer::SetShutDownMode(bool bFlag)
{
	m_bShutdown = bFlag;
	PostQueuedCompletionStatus(m_hIOCP, 0, 0, 0);
	WaitForMultipleObjects(m_iAllThreadCnt, m_hAllthread, true, INFINITE);

	ServerStop();

	return m_bShutdown;
}

bool CNetServer::SetWhiteIPMode(bool bFlag)
{
	m_bWhiteIPMode = bFlag;
	return m_bWhiteIPMode;
}

bool CNetServer::SetMonitorMode(bool bFlag)
{
	m_bMonitorFlag = bFlag;
	return m_bMonitorFlag;
}

unsigned __int64* CNetServer::GetIndex()
{
	unsigned __int64 *_iIndex = nullptr;
	SessionStack.Pop(&_iIndex);
	return _iIndex;
}

void CNetServer::PutIndex(unsigned __int64 iIndex)
{
	SessionStack.Push(&pIndex[iIndex]);
}
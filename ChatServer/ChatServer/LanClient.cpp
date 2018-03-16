#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <windows.h>

#include "ChatServer.h"
#include "LanClient.h"


using namespace std;

CLanClient::CLanClient() :
	m_iRecvPacketTPS(NULL),
	m_iSendPacketTPS(NULL)
{
	m_Session = new LANSESSION;


	setlocale(LC_ALL, "Korean");

	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
}

CLanClient::~CLanClient()
{
	delete m_Session;
}

void CLanClient::Constructor(CChatServer *pChat)
{
	pChatServer = pChat;
	return;
}

void CLanClient::OnEnterJoinServer()
{
	//	서버와의 연결 성공 후
	CPacket *pPacket = CPacket::Alloc();
	m_Session->bConnect = true;

	WORD Type = en_PACKET_SS_LOGINSERVER_LOGIN;
	BYTE ServerType = dfSERVER_TYPE_CHAT;
	WCHAR ServerName[32] = L"ChatServer";

	*pPacket << Type << ServerType;
	pPacket->PushData(&ServerName[0], sizeof(ServerName));

	SendPacket(pPacket);
	pPacket->Free();
	return;
}

void CLanClient::OnLeaveServer()
{
	//	서버와의 연결이 끊어졌을 때
	m_Session->bConnect = false;
	return;
}

void CLanClient::OnLanRecv(CPacket *pPacket)
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

void CLanClient::OnLanSend(int SendSize)
{
	//	패킷 송신 완료 후

	return;
}

void CLanClient::OnWorkerThreadBegin()
{

}

void CLanClient::OnWorkerThreadEnd()
{

}

void CLanClient::OnError(int ErrorCode, WCHAR *pMsg)
{

}

bool CLanClient::Connect(WCHAR * ServerIP, int Port, bool bNoDelay, int MaxWorkerThread)
{
	wprintf(L"[Client :: ClientInit]	Start\n");

	m_Session->RecvQ.Clear();
	m_Session->PacketQ.Clear();
	m_Session->SendFlag = false;

	for (auto i = 0; i < MaxWorkerThread; i++)
	{
		m_hWorker_Thread[i] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, (LPVOID)this, 0, NULL);
	}

	WSADATA wsaData;
	int retval = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != retval)
	{
		wprintf(L"[Client :: Connect]	WSAStartup Error\n");
		//	로그
		g_CrashDump->Crash();
		return false;
	}

	m_Session->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_Session->sock == INVALID_SOCKET)
	{
		wprintf(L"[Client :: Connect]	WSASocket Error\n");
		//	로그
		g_CrashDump->Crash();
		return false;
	}

	struct sockaddr_in client_addr;
	ZeroMemory(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	InetPton(AF_INET, ServerIP, &client_addr.sin_addr.s_addr);

	client_addr.sin_port = htons(Port);
	setsockopt(m_Session->sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&bNoDelay, sizeof(bNoDelay));

	while (1)
	{
		retval = connect(m_Session->sock, (SOCKADDR*)&client_addr, sizeof(client_addr));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"[Client :: Connect]		Login_LanServer Not Connect\n");
			Sleep(1000);
			continue;
		}
		break;
	}

	CreateIoCompletionPort((HANDLE)m_Session->sock, m_hIOCP, (ULONG_PTR)this, 0);

	m_Session->bConnect = true;
	OnEnterJoinServer();
	wprintf(L"[Client :: Connect]		Complete\n");
	RecvPost();
	return true;
}

bool CLanClient::Disconnect()
{
	closesocket(m_Session->sock);

	while (0 < m_Session->SendQ.GetUseCount())
	{
		CPacket * pPacket;
		m_Session->SendQ.Dequeue(pPacket);
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
	}

	while (0 < m_Session->PacketQ.GetUseSize())
	{
		CPacket * pPacket;
		m_Session->PacketQ.Peek((char*)&pPacket, sizeof(CPacket*));
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
		m_Session->PacketQ.Dequeue(sizeof(CPacket*));
	}
	m_Session->bConnect = false;

	WaitForMultipleObjects(LANCLIENT_WORKERTHREAD, m_hWorker_Thread, false, INFINITE);

	for (auto iCnt = 0; iCnt < LANCLIENT_WORKERTHREAD; iCnt++)
	{
		CloseHandle(m_hWorker_Thread[iCnt]);
		m_hWorker_Thread[iCnt] = INVALID_HANDLE_VALUE;
	}

	WSACleanup();
	return true;
}

bool CLanClient::IsConnect()
{
	return m_Session->bConnect;
}

bool CLanClient::SendPacket(CPacket *pPacket)
{
	m_iSendPacketTPS++;
	pPacket->AddRef();
	pPacket->SetHeader_CustomShort(pPacket->GetDataSize());
	m_Session->SendQ.Enqueue(pPacket);
	SendPost();

	return true;
}

void CLanClient::WorkerThread_Update()
{
	DWORD retval;

	while (1)
	{
		//	초기화 필수
		OVERLAPPED * pOver = NULL;
		LANSESSION * pSession = NULL;
		DWORD Trans = 0;

		retval = GetQueuedCompletionStatus(m_hIOCP, &Trans, (PULONG_PTR)&pSession, (LPWSAOVERLAPPED*)&pOver, INFINITE);
		//		OnWorkerThreadBegin();

		if (nullptr == pOver)
		{
			if (FALSE == retval)
			{
				//	IOCP 자체 오류
				g_CrashDump->Crash();
			}
			else if (0 == Trans)
			{
				PostQueuedCompletionStatus(m_hIOCP, 0, 0, 0);
			}
			else
			{
				//	현재 구조에서 발생할수 없는 상황
				g_CrashDump->Crash();
			}
			break;
		}

		if (0 == Trans)
		{
			//			shutdown(sock, SD_BOTH);
			Disconnect();
		}
		else if (pOver == &m_Session->RecvOver)
		{
			CompleteRecv(Trans);
		}
		else if (pOver == &m_Session->SendOver)
		{
			CompleteSend(Trans);
		}

		// 		if (0 >= (retval = InterlockedDecrement(&IO_Count)))
		// 		{
		// 			if (0 == retval)
		// 				Disconnect();
		// 			else if (0 > retval)
		// 				g_CrashDump->Crash();
		// 		}
		//		OnWorkerThreadEnd();
	}

}

void CLanClient::CompleteRecv(DWORD dwTransfered)
{
	m_Session->RecvQ.Enqueue(dwTransfered);
	WORD _wPayloadSize = 0;

	while (LANCLIENT_HEADERSIZE == m_Session->RecvQ.Peek((char*)&_wPayloadSize, LANCLIENT_HEADERSIZE))
	{
		CPacket *_pPacket = CPacket::Alloc();
		if (m_Session->RecvQ.GetUseSize() < LANCLIENT_HEADERSIZE + _wPayloadSize)
			break;

		m_Session->RecvQ.Dequeue(LANCLIENT_HEADERSIZE);

		if (_pPacket->GetFreeSize() < _wPayloadSize)
		{
			Disconnect();
			return;
		}
		m_Session->RecvQ.Dequeue(_pPacket->GetWritePtr(), _wPayloadSize);
		_pPacket->PushData(_wPayloadSize + sizeof(CPacket::st_PACKET_HEADER));
		_pPacket->PopData(sizeof(CPacket::st_PACKET_HEADER));

		m_iRecvPacketTPS++;
		OnLanRecv(_pPacket);
		_pPacket->Free();
	}
	RecvPost();
}

void CLanClient::CompleteSend(DWORD dwTransfered)
{
	CPacket * pPacket[LANCLIENT_WSABUFNUM];
	m_Session->PacketQ.Peek((char*)&pPacket, sizeof(CPacket*) * m_Session->Send_Count);
	for (auto i = 0; i < m_Session->Send_Count; i++)
	{
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket[i]->Free();
		m_Session->PacketQ.Dequeue(sizeof(CPacket*));
	}

	InterlockedExchange(&m_Session->SendFlag, false);

	SendPost();
}

void CLanClient::RecvPost()
{
	DWORD flags = 0;
	ZeroMemory(&m_Session->RecvOver, sizeof(m_Session->RecvOver));

	WSABUF wsaBuf[2];
	DWORD freeSize = m_Session->RecvQ.GetFreeSize();
	DWORD notBrokenPushSize = m_Session->RecvQ.GetNotBrokenPushSize();
	if (0 == freeSize && 0 == notBrokenPushSize)
	{
		//	로그
		//	RecvQ가 다 차서 서버에서 연결을 끊음
		g_CrashDump->Crash();
		return;
	}

	int numOfBuf = (notBrokenPushSize < freeSize) ? 2 : 1;

	wsaBuf[0].buf = m_Session->RecvQ.GetWriteBufferPtr();		//	Dequeue는 rear를 건드리지 않으므로 안전
	wsaBuf[0].len = notBrokenPushSize;

	if (2 == numOfBuf)
	{
		wsaBuf[1].buf = m_Session->RecvQ.GetBufferPtr();
		wsaBuf[1].len = freeSize - notBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(m_Session->sock, wsaBuf, numOfBuf, NULL, &flags, &m_Session->RecvOver, NULL))
	{
		int lastError = WSAGetLastError();
		if (ERROR_IO_PENDING != lastError)
		{
			if (WSAENOBUFS == lastError)
			{
				//	로그
				//	메모리 한계 초과
				g_CrashDump->Crash();
			}
			Disconnect();
		}
	}
	return;
}

void CLanClient::SendPost()
{
	do
	{
		if (true == InterlockedCompareExchange(&m_Session->SendFlag, true, false))
			return;

		if (0 == m_Session->SendQ.GetUseCount())
		{
			InterlockedExchange(&m_Session->SendFlag, false);
			return;
		}

		WSABUF wsaBuf[LANCLIENT_WSABUFNUM];
		CPacket *pPacket;
		long BufNum = 0;
		int iUseSize = (m_Session->SendQ.GetUseCount());
		if (iUseSize > LANCLIENT_WSABUFNUM)
		{
			//	SendQ에 패킷이 100개 이상있을때, WSABUF에 100개만 넣는다.
			BufNum = LANCLIENT_WSABUFNUM;
			m_Session->Send_Count = LANCLIENT_WSABUFNUM;
			//	pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * MAX_WSABUF_NUMBER);
			for (auto i = 0; i < LANCLIENT_WSABUFNUM; i++)
			{
				m_Session->SendQ.Dequeue(pPacket);
				m_Session->PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}
		else
		{
			//	SendQ에 패킷이 100개 이하일 때 현재 패킷을 계산해서 넣는다
			BufNum = iUseSize;
			m_Session->Send_Count = iUseSize;
			//			pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * iUseSize);
			for (auto i = 0; i < iUseSize; i++)
			{
				m_Session->SendQ.Dequeue(pPacket);
				m_Session->PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}
		ZeroMemory(&m_Session->SendOver, sizeof(m_Session->SendOver));
		if (SOCKET_ERROR == WSASend(m_Session->sock, wsaBuf, BufNum, NULL, 0, &m_Session->SendOver, NULL))
		{
			int lastError = WSAGetLastError();
			if (ERROR_IO_PENDING != lastError)
			{
				if (WSAENOBUFS == lastError)
				{
					//	로그
					//	메모리 한계 초과
				}
				Disconnect();
				break;
			}
		}
	} while (0 != m_Session->SendQ.GetUseCount());
}
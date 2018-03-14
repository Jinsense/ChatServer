#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <windows.h>

#include "LanClient.h"

using namespace std;

CLanClient::CLanClient() :
	RecvQ(LANCLIENT_QUEUESIZE),
	PacketQ(LANCLIENT_QUEUESIZE),
	SendFlag(false),
	m_iRecvPacketTPS(NULL),
	m_iSendPacketTPS(NULL)
{
	setlocale(LC_ALL, "Korean");

	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
}

CLanClient::~CLanClient()
{

}

bool CLanClient::Connect(WCHAR * ServerIP, int Port, bool bNoDelay, int MaxWorkerThread)
{
	wprintf(L"[Client :: ClientInit]	Start\n");

	RecvQ.Clear();
	PacketQ.Clear();
	SendFlag = false;

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

	sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
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
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&bNoDelay, sizeof(bNoDelay));

	while (1)
	{
		retval = connect(sock, (SOCKADDR*)&client_addr, sizeof(client_addr));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"[Client :: Connect]		Login_LanServer Not Connect\n");
			Sleep(1000);
			continue;
		}
		break;
	}

	CreateIoCompletionPort((HANDLE)sock, m_hIOCP, (ULONG_PTR)this, 0);

	bConnect = true;
	OnEnterJoinServer();
	wprintf(L"[Client :: Connect]		Complete\n");
	return true;
}

bool CLanClient::Disconnect()
{
	closesocket(sock);

	while (0 < SendQ.GetUseCount())
	{
		CPacket * pPacket;
		SendQ.Dequeue(pPacket);
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
	}

	while (0 < PacketQ.GetUseSize())
	{
		CPacket * pPacket;
		PacketQ.Peek((char*)&pPacket, sizeof(CPacket*));
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
		PacketQ.Dequeue(sizeof(CPacket*));
	}
	bConnect = false;

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
	return bConnect;
}

bool CLanClient::SendPacket(CPacket *pPacket)
{
	m_iSendPacketTPS++;
	pPacket->AddRef();
	pPacket->SetHeader_CustomShort(pPacket->GetDataSize());
	SendQ.Enqueue(pPacket);
	SendPost();

	return true;
}

void CLanClient::WorkerThread_Update()
{
	DWORD retval;

	while (bConnect)
	{
		//	초기화 필수
		OVERLAPPED * pOver = NULL;
		DWORD Trans = 0;

		retval = GetQueuedCompletionStatus(m_hIOCP, &Trans, (PULONG_PTR)this, (LPWSAOVERLAPPED*)&pOver, INFINITE);
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
		else if (pOver == &RecvOver)
		{
			CompleteRecv(Trans);
		}
		else if (pOver == &SendOver)
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
	RecvQ.Enqueue(dwTransfered);
	WORD _wPayloadSize = 0;

	while (LANCLIENT_HEADERSIZE == RecvQ.Peek((char*)&_wPayloadSize, LANCLIENT_HEADERSIZE))
	{
		CPacket *_pPacket = CPacket::Alloc();
		if (RecvQ.GetUseSize() < LANCLIENT_HEADERSIZE + _wPayloadSize)
			break;

		RecvQ.Dequeue(LANCLIENT_HEADERSIZE);

		if (_pPacket->GetFreeSize() < _wPayloadSize)
		{
			Disconnect();
			return;
		}
		RecvQ.Dequeue(_pPacket->GetWritePtr(), _wPayloadSize);
		_pPacket->PushData(_wPayloadSize);
		_pPacket->PopData(sizeof(CPacket::st_PACKET_HEADER));

		m_iRecvPacketTPS++;
		OnRecv(_pPacket);
		_pPacket->Free();
	}
	RecvPost();
}

void CLanClient::CompleteSend(DWORD dwTransfered)
{
	CPacket * pPacket[LANCLIENT_WSABUFNUM];
	PacketQ.Peek((char*)&pPacket, sizeof(CPacket*) * Send_Count);
	for (auto i = 0; i < Send_Count; i++)
	{
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket[i]->Free();
		PacketQ.Dequeue(sizeof(CPacket*));
	}

	InterlockedExchange(&SendFlag, false);

	SendPost();
}

void CLanClient::RecvPost()
{
	DWORD flags = 0;
	ZeroMemory(&RecvOver, sizeof(RecvOver));

	WSABUF wsaBuf[2];
	DWORD freeSize = RecvQ.GetFreeSize();
	DWORD notBrokenPushSize = RecvQ.GetNotBrokenPushSize();
	if (0 == freeSize && 0 == notBrokenPushSize)
	{
		//	로그
		//	RecvQ가 다 차서 서버에서 연결을 끊음
		g_CrashDump->Crash();
		return;
	}

	int numOfBuf = (notBrokenPushSize < freeSize) ? 2 : 1;

	wsaBuf[0].buf = RecvQ.GetWriteBufferPtr();		//	Dequeue는 rear를 건드리지 않으므로 안전
	wsaBuf[0].len = notBrokenPushSize;

	if (2 == numOfBuf)
	{
		wsaBuf[1].buf = RecvQ.GetBufferPtr();
		wsaBuf[1].len = freeSize - notBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(sock, wsaBuf, numOfBuf, NULL, &flags, &RecvOver, NULL))
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
		if (true == InterlockedCompareExchange(&SendFlag, true, false))
			return;

		if (0 == SendQ.GetUseCount())
		{
			InterlockedExchange(&SendFlag, false);
			return;
		}

		WSABUF wsaBuf[LANCLIENT_WSABUFNUM];
		CPacket *pPacket;
		long BufNum = 0;
		int iUseSize = (SendQ.GetUseCount());
		if (iUseSize > LANCLIENT_WSABUFNUM)
		{
			//	SendQ에 패킷이 100개 이상있을때, WSABUF에 100개만 넣는다.
			BufNum = LANCLIENT_WSABUFNUM;
			Send_Count = LANCLIENT_WSABUFNUM;
			//	pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * MAX_WSABUF_NUMBER);
			for (auto i = 0; i < LANCLIENT_WSABUFNUM; i++)
			{
				SendQ.Dequeue(pPacket);
				PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}
		else
		{
			//	SendQ에 패킷이 100개 이하일 때 현재 패킷을 계산해서 넣는다
			BufNum = iUseSize;
			Send_Count = iUseSize;
			//			pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * iUseSize);
			for (auto i = 0; i < iUseSize; i++)
			{
				SendQ.Dequeue(pPacket);
				PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}
		ZeroMemory(&SendOver, sizeof(SendOver));
		if (SOCKET_ERROR == WSASend(sock, wsaBuf, BufNum, NULL, 0, &SendOver, NULL))
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
	} while (0 != SendQ.GetUseCount());
}
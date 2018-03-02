#ifndef _CHATSERVER_CLIENT_LANCLIENT_H_
#define _CHATSERVER_CLIENT_LANCLIENT_H_

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include "Packet.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"

#define LANCLIENT_WORKERTHREAD	2
#define LANCLIENT_WSABUFNUM		300
#define LANCLIENT_QUEUESIZE		20000

#define LANCLIENT_HEADERSIZE	2



class CLanClient
{
public: 
	CLanClient();
	~CLanClient();

	bool Connect(char * ServerIP, int Port, bool NoDelay, int MaxWorkerThread);	//   ���ε� IP, ����IP / ��Ŀ������ �� / ���ۿɼ�
	bool Disconnect();
	bool IsConnect();
	bool SendPacket(CPacket *pPacket);

	virtual void OnEnterJoinServer() = 0;		//	�������� ���� ���� ��
	virtual void OnLeaveServer() = 0;			//	�������� ������ �������� ��

	virtual void OnRecv(CPacket *pPacket) = 0;	//	�ϳ��� ��Ŷ ���� �Ϸ� ��
	virtual void OnSend(int SendSize) = 0;		//	��Ŷ �۽� �Ϸ� ��

	virtual void OnWorkerThreadBegin() = 0;
	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int ErrorCode, WCHAR *pMsg) = 0;

private:
	static unsigned int WINAPI WorkerThread(LPVOID arg)
	{
		CLanClient * pWorkerThread = (CLanClient *)arg;
		if (pWorkerThread == NULL)
		{
			wprintf(L"[Client :: WorkerThread]	Init Error\n");
			return FALSE;
		}

		pWorkerThread->WorkerThread_Update();
		return TRUE;
	}
	void WorkerThread_Update();
	void RecvPost();
	void SendPost();
	void CompleteRecv(DWORD dwTransfered);
	void CompleteSend(DWORD dwTransfered);
public:


private:
	long					SendFlag;
	long					Send_Count;
	bool					bConnect;

	long					m_iRecvPacketTPS;
	long					m_iSendPacketTPS;

	OVERLAPPED				SendOver, RecvOver;
	CRingBuffer				RecvQ, PacketQ;
	CLockFreeQueue<CPacket*> SendQ;
	SOCKET					sock;

	HANDLE					m_hIOCP;
	HANDLE					m_hWorker_Thread[10];

};

#endif _CHATSERVER_CLIENT_LANCLIENT_H_

#ifndef _CHATSERVER_NETWORK_RINGBUFFER_H_
#define _CHATSERVER_NETWORK_RINGBUFFER_H_


class CRingBuffer
{
public:
	CRingBuffer();
	CRingBuffer(int iBufferSize);
	~CRingBuffer();
public:
	void	Initialize(int iBufferSize);
	void	Clear();
	char*	GetBufferPtr() { return m_pBuffer; }
	char*	GetWriteBufferPtr() { return &m_pBuffer[m_iRear]; }
	char*	GetReadBufferPtr() { return &m_pBuffer[m_iFront]; }
	int		GetBufferSize();
	int		GetFreeSize();
	int		GetUseSize();
	int		GetNotBrokenPushSize();
	int		GetNotBrokenPopSize();
	int		Enqueue(const char *pData, int iDataSize);
	int		Dequeue(char *pData, int iDataSize);
	int		Enqueue(int iDataSize);
	int		Dequeue(int iDataSize);
	int		Peek(char *pData, int iDataSize);

private:
	char	*m_pBuffer;
	int		m_iBufferSize;
	int		m_iFront;
	int		m_iRear;

	SRWLOCK	m_srw;
};

#endif _CHATSERVER_NETWORK_RINGBUFFER_H_
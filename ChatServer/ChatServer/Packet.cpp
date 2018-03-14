#include "Packet.h"

#include <Windows.h>

//CMemoryPool<CPacket>* CPacket::m_pMemoryPool = NULL;
CMemoryPoolTLS<CPacket>* CPacket::m_pMemoryPool = NULL;


CPacket::CPacket() : 
	m_iBufferSize(static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE)), 
	m_pEndPos(m_chBuffer + static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE))
{
	ZeroMemory(&m_chBuffer, static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE));
	m_iDataSize = 0;
//	m_pWritePos = m_chBuffer;
	m_pReadPos = m_chBuffer;
	m_pWritePos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
//	m_pReadPos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	m_lHeaderSetFlag = false;
	m_iRefCount = 0;
}

CPacket::~CPacket()
{
}

void CPacket::Clear()
{
	ZeroMemory(&m_chBuffer, static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE));
	m_iDataSize = 0;
//	m_pWritePos = m_chBuffer;
	m_pReadPos = m_chBuffer;
	m_pWritePos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
//	m_pReadPos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	m_lHeaderSetFlag = false;
	m_iRefCount = 0;
	m_iBufferSize = static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE);
 	m_pEndPos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE);
}

CPacket * CPacket::Alloc()
{
	CPacket *_pPacket = m_pMemoryPool->Alloc();
	_pPacket->Clear();
	_pPacket->AddRef();
	return _pPacket;
}

void CPacket::Free()
{
	__int64 Count = InterlockedDecrement64(&m_iRefCount);
	if (0 >= Count)
	{
		if (0 > Count)
			g_CrashDump->Crash();
		else
			m_pMemoryPool->Free(this);
	}
}

void CPacket::MemoryPoolInit()
{
	if (m_pMemoryPool == nullptr)
		m_pMemoryPool = new CMemoryPoolTLS<CPacket>();
}

void CPacket::AddRef()
{
	InterlockedIncrement64(&m_iRefCount);
}

void CPacket::PushData(WCHAR* pSrc, int iSize)
{
	int Size = iSize * 2;
	if (m_pEndPos - m_pWritePos < Size)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR),
			Size, int(m_pEndPos - m_pWritePos));*/
	memcpy_s(m_pWritePos, Size, pSrc, Size);
	m_pWritePos += Size;
	m_iDataSize += Size;
}

void CPacket::PopData(WCHAR* pDest, int iSize)
{
	int Size = iSize * 2;
	if (m_iDataSize < Size)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::POP_ERR),
			Size, m_iDataSize);*/
	memcpy_s(pDest, Size, m_pReadPos, Size);
	m_pReadPos += Size;
	m_iDataSize -= Size;
}

void CPacket::PushData(char *pSrc, int iSize)
{
	if (m_pEndPos - m_pWritePos < iSize)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR), 
						iSize, int(m_pEndPos - m_pWritePos));*/
	memcpy_s(m_pWritePos, iSize, pSrc, iSize);
	m_pWritePos += iSize;
	m_iDataSize += iSize;
}

void CPacket::PopData(char *pDest, int iSize)
{
	if (m_iDataSize < iSize)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::POP_ERR), 
						iSize, m_iDataSize);*/
	memcpy_s(pDest, iSize, m_pReadPos, iSize);
	m_pReadPos += iSize;
	m_iDataSize -= iSize;
}

void CPacket::PushData(int iSize)
{
	if (m_pEndPos - m_pWritePos < iSize)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR), 
						iSize, int(m_pEndPos - m_pWritePos));*/
	m_pWritePos += iSize;
	m_iDataSize += iSize;
}

void CPacket::PopData(int iSize)
{
	if (m_iDataSize < iSize)
		g_CrashDump->Crash();
		/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR), 
						iSize, m_iDataSize);*/
	m_pReadPos += iSize;
	m_iDataSize -= iSize;
}

void CPacket::SetHeader(char *pHeader)
{
	if (true == InterlockedCompareExchange(&m_lHeaderSetFlag, true, false))
		return;
	memcpy_s(m_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE), 
		pHeader, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));
}

void CPacket::SetHeader_CustomHeader(char *pHeader, int iCustomHeaderSize)
{
	if (true == InterlockedCompareExchange(&m_lHeaderSetFlag, true, false))
		return;
	int iSize = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) - iCustomHeaderSize;
	memcpy_s(&m_chBuffer[iSize], static_cast<int>(en_PACKETDEFINE::HEADER_SIZE), 
		pHeader, iCustomHeaderSize);

	m_pReadPos += static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) - iCustomHeaderSize;
	return;
}

void CPacket::SetHeader_CustomShort(unsigned short shHeader)
{
	if (true == InterlockedCompareExchange(&m_lHeaderSetFlag, true, false))
		return;
	memcpy_s(&m_chBuffer[3], static_cast<int>(en_PACKETDEFINE::SHORT_HEADER_SIZE), 
		&shHeader, static_cast<int>(en_PACKETDEFINE::SHORT_HEADER_SIZE));
	
	m_pReadPos += 3;
	return;
}

void CPacket::EnCode()
{
	if (true == InterlockedCompareExchange(&m_lHeaderSetFlag, true, false))
		return;

	st_PACKET_HEADER Header;
	Header.shLen = m_iDataSize;

	int iCheckSum = 0;
	BYTE *pPtr = (BYTE*)&m_chBuffer[5];

	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE); 
		iCnt < m_iDataSize + sizeof(st_PACKET_HEADER); iCnt++)
	{
		iCheckSum += *pPtr;
		pPtr++;
	}
	Header.CheckSum = (BYTE)(iCheckSum % 256);
	Header.CheckSum = Header.CheckSum ^ Header.RandKey ^ _Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^ 
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < m_iDataSize + sizeof(st_PACKET_HEADER); iCnt++)
		m_chBuffer[iCnt] = m_chBuffer[iCnt] ^ Header.RandKey ^ _Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);
	Header.RandKey = Header.RandKey ^ _Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^ 
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);

	memcpy_s(&m_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE), 
		&Header, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));

	return;
}

bool CPacket::DeCode(st_PACKET_HEADER * pInHeader)
{
	if (nullptr == pInHeader)
		memcpy_s(&pInHeader, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE), 
			m_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));

	if (pInHeader->byCode != _Config.PACKET_CODE)
//		static_cast<int>(en_PACKETDEFINE::PACKET_CODE))
		return false;

	pInHeader->RandKey = pInHeader->RandKey ^ _Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);
	pInHeader->CheckSum = pInHeader->CheckSum ^ pInHeader->RandKey ^_Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^
//		static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < pInHeader->shLen + sizeof(st_PACKET_HEADER); iCnt++)
	{
		m_chBuffer[iCnt] = m_chBuffer[iCnt] ^ pInHeader->RandKey ^ _Config.PACKET_KEY1 ^ _Config.PACKET_KEY2;
//			static_cast<int>(en_PACKETDEFINE::PACKET_KEY1) ^
//			static_cast<int>(en_PACKETDEFINE::PACKET_KEY2);
	}
	int iCheckSum = 0;
	BYTE *pPtr = (BYTE*)&m_chBuffer[5];
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) + pInHeader->shLen; iCnt++)
	{
		iCheckSum += *pPtr;
		pPtr++;
	}
	iCheckSum = (BYTE)(iCheckSum % 256);

	if (iCheckSum != pInHeader->CheckSum)
		return false;

	if (pInHeader->shLen >= static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE))
		return false;

	return true;
}

CPacket& CPacket::operator=(CPacket& Packet)
{
	m_iDataSize = Packet.m_iDataSize;
	m_pWritePos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) + 
		m_iDataSize;
	m_pReadPos = m_chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	memcpy_s(m_chBuffer, m_iDataSize, Packet.m_pReadPos, m_iDataSize);

	return *this;
}

CPacket& CPacket::operator<<(char Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned char Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(short Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned short Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(int Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned int Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(long Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned long Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(float Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(__int64 Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(double Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (char& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned char& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (short& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned short& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (int& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned int& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (long& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned long& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (float& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (__int64& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (double& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}
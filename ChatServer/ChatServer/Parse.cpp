#include <windows.h>
#include "Parse.h"

CINIParse::CINIParse()
{
	m_chpBuffer = new char[eBUFFER_SIZE];
	Initial();
}

CINIParse::~CINIParse()
{
	delete[] m_chpBuffer;
}

void CINIParse::Initial()
{
	memset(m_chpBuffer, 0, eBUFFER_SIZE);
	m_iLoadSize = 0;

	m_iBufferAreaStart = -1;
	m_iBufferAreaEnd = -1;

	m_iBufferFocusPos = 0;

	m_bProvideAreaMode = false;
}

bool CINIParse::LoadFile(WCHAR *szFileName)
{
	HANDLE hFile;
	DWORD dwRead;

	if (0 != m_iLoadSize)
		Initial();

	hFile = CreateFile(szFileName, GENERIC_READ, NULL, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	m_iLoadSize = GetFileSize(hFile, NULL);

	if (eBUFFER_SIZE < m_iLoadSize)
	{
		CloseHandle(hFile);
		Initial();
		return false;
	}

	ReadFile(hFile, m_chpBuffer, m_iLoadSize, &dwRead, NULL);

	if (dwRead != (DWORD)m_iLoadSize)
	{
		CloseHandle(hFile);
		Initial();
		return false;
	}

	CloseHandle(hFile);

	return true;
}

bool CINIParse::ProvideArea(const char *szAreaName)
{
	char *chpBuff, chBuff[256];
	int iLength;
	bool bAreaFlag = false;

	m_iBufferAreaStart = -1;
	m_iBufferAreaEnd = -1;
	m_iBufferFocusPos = 0;

	m_bProvideAreaMode = true;

	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chBuff, 0, 256);
		memcpy(chBuff, chpBuff, iLength);

		if (chBuff[0] == ':')
		{
			if (0 == strcmp(chBuff + 1, szAreaName))
			{
				if (GetNextWord(&chpBuff, &iLength))
				{
					memset(chBuff, 0, 256);
					memcpy(chBuff, chpBuff, iLength);

					if (chpBuff[0] == '{')
					{
						if (!SkipNoneCommand())
						{
							m_bProvideAreaMode = false;
							return false;
						}
						m_iBufferAreaStart = m_iBufferFocusPos;
						bAreaFlag = true;
					}
				}
				else
				{
					m_bProvideAreaMode = false;
					return false;
				}
			}
		}
		else if (bAreaFlag && chBuff[0] == '}')
		{
			m_iBufferAreaEnd = m_iBufferFocusPos - 1;
			m_bProvideAreaMode = false;
			return true;
		}
	}
	m_bProvideAreaMode = false;
	return false;
}

bool CINIParse::GetValue(const char *szName, char *szValue, int *ipBuffSize)
{
	char *chpBuff, chBuff[256];
	int iLength;

	m_iBufferFocusPos = m_iBufferAreaStart;
	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chBuff, 0, 256);
		memcpy(chBuff, chpBuff, iLength);

		if (0 == strcmp(szName, chBuff))
		{
			if (GetNextWord(&chpBuff, &iLength))
			{
				memset(chBuff, 0, 256);
				memcpy(chBuff, chpBuff, iLength);

				if (0 == strcmp(chBuff, "="))
				{
					if (GetNextWord(&chpBuff, &iLength))
					{
						if (*ipBuffSize <= iLength)
							return false;

						memset(szValue, 0, *ipBuffSize);
						memcpy(szValue, chpBuff, iLength);

						*ipBuffSize = iLength;
						return true;
					}
					return false;
				}
				else if (0 == strcmp(chBuff, "=="))
				{

				}
				else if (0 == strcmp(chBuff, "!="))
				{

				}
				//	기타 등등...
			}
			return false;
		}
	}
	return false;
}

bool CINIParse::GetValue(const char *szName, int *ipValue)
{
	char *chpBuff, chBuff[256];
	int iLength;

	m_iBufferFocusPos = m_iBufferAreaStart;

	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chBuff, 0, 256);
		memcpy(chBuff, chpBuff, iLength);
		if (0 == strcmp(szName, chBuff))
		{
			if (GetNextWord(&chpBuff, &iLength))
			{
				memset(chBuff, 0, 256);
				memcpy(chBuff, chpBuff, iLength);

				if (0 == strcmp(chBuff, "="))
				{
					if (GetNextWord(&chpBuff, &iLength))
					{
						memset(chBuff, 0, 256);
						memcpy(chBuff, chpBuff, iLength);

						*ipValue = atoi(chBuff);
						return true;
					}
					return false;
				}
				//	기타 등등...
			}
			return false;
		}
	}
	return false;
}

bool CINIParse::GetValue(const char *szName, float *fpValue)
{
	char *chpBuff, chBuff[256];
	int iLength;
	double dValue;

	m_iBufferFocusPos = m_iBufferAreaStart;

	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chBuff, 0, 256);
		memcpy(chBuff, chpBuff, iLength);

		if (0 == strcmp(szName, chBuff))
		{
			if (GetNextWord(&chpBuff, &iLength))
			{
				memset(chBuff, 0, 256);
				memcpy(chBuff, chpBuff, iLength);

				if (0 == strcmp(chBuff, "="))
				{
					if (GetNextWord(&chpBuff, &iLength))
					{
						memset(chBuff, 0, 256);
						memcpy(chBuff, chpBuff, iLength);

						dValue = atof(chBuff);
						*fpValue = (float)dValue;
						return true;
					}
					return false;
				}
			}
			return false;
		}
	}
	return false;
}

void CINIParse::UTF8toUTF16(const char *szText, WCHAR *szBuf, int iBufLen)
{
	int iRe = MultiByteToWideChar(CP_UTF8, 0, szText, strlen(szText), szBuf, iBufLen);
	if (iRe < iBufLen)
		szBuf[iRe] = L'\0';
	return;
}

void CINIParse::UTF16toUTF8(WCHAR *szText, char *szBuf, int iBufLen)
{
	int iRe = WideCharToMultiByte(CP_UTF8, 0, szText, lstrlenW(szText), szBuf, iBufLen, NULL, NULL);
	return;
}

bool CINIParse::SkipNoneCommand()
{
	char *chpBuffer;
	chpBuffer = m_chpBuffer + m_iBufferFocusPos;

	while (1)
	{
		if (m_iBufferFocusPos > m_iLoadSize ||
			(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
			return false;

		if (*chpBuffer == 0x20 || *chpBuffer == 0x0d ||
			*chpBuffer == 0x0a || *chpBuffer == 0x09 || *chpBuffer == 0x08)
		{
			*chpBuffer = 0x20;
			++chpBuffer;
			++m_iBufferFocusPos;
		}
		else if (*chpBuffer == '/' && *(chpBuffer + 1) == '/')
		{
			while (*chpBuffer != 0x0d)
			{
				*chpBuffer = 0x20;
				++m_iBufferFocusPos;
				++chpBuffer;

				if (m_iBufferFocusPos > m_iLoadSize ||
					(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
				{
					return false;
				}
			}
		}
		else if (*chpBuffer == '/' && *(chpBuffer + 1) == '*')
		{
			while (!(*chpBuffer == '*' && *(chpBuffer + 1) == '/'))
			{
				*chpBuffer = 0x20;
				++m_iBufferFocusPos;
				++chpBuffer;

				if (m_iBufferFocusPos > m_iLoadSize ||
					(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
				{
					return false;
				}
			}
			*chpBuffer = 0x20;
			++chpBuffer;
			*chpBuffer = 0x20;
			++chpBuffer;
			m_iBufferFocusPos += 2;
		}
		else
		{
			break;
		}
	}
	return true;
}

bool CINIParse::GetNextWord(char **chppBuffer, int *ipLength)
{
	char *chpBufferTemp;
	if (!SkipNoneCommand()) return false;

	if (m_iBufferFocusPos > m_iLoadSize ||
		(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
		return false;

	chpBufferTemp = *chppBuffer = m_chpBuffer + m_iBufferFocusPos;
	*ipLength = 0;

	if (**chppBuffer == '"')
	{
		if (GetStringWord(chppBuffer, ipLength))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	while (1)
	{
		//	단어의 기준
		//	컴마		: ','
		//	마침표		: '.'
		//	따옴표		: '"'
		//	스페이스	: 0x20
		//	백스페이스	: 0x08
		//	탭			: 0x09
		//	라인 피드	: 0x0a
		//	캐리지 리턴	: 0x0d
		//	공백		: 0x20
		if (**chppBuffer == 0x20 || **chppBuffer == '"' ||
			**chppBuffer == ',' || **chppBuffer == 0x08 ||
			**chppBuffer == 0x09 || **chppBuffer == 0x0a ||
			**chppBuffer == 0x0d)
		{
			break;
		}
		m_iBufferFocusPos++;
		(*chppBuffer)++;
		(*ipLength)++;

		if (m_iBufferFocusPos > m_iLoadSize ||
			(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
		{
			break;
		}
	}
	*chppBuffer = chpBufferTemp;
	if (*ipLength == 0)
		return false;
	return true;
}

bool CINIParse::GetStringWord(char **chppBuffer, int *ipLength)
{
	char *chpBufferTemp;
	chpBufferTemp = *chppBuffer = m_chpBuffer + m_iBufferFocusPos;
	*ipLength = 0;

	if (**chppBuffer != '"')
	{
		return false;
	}

	m_iBufferFocusPos++;
	(*chppBuffer)++;
	chpBufferTemp++;

	while (1)
	{
		//	단어의 기준
		//	따옴표			: '"'
		//	라인 피드		: 0x0a
		//	캐리지 리턴		: 0x0d
		if (**chppBuffer == '"' || **chppBuffer == 0x0a || **chppBuffer == 0x0d)
		{
			m_iBufferFocusPos++;
			break;
		}
		m_iBufferFocusPos++;
		(*chppBuffer)++;
		(*ipLength)++;

		if (m_iBufferFocusPos > m_iLoadSize ||
			(!m_bProvideAreaMode && m_iBufferFocusPos > m_iBufferAreaEnd))
			return false;
	}
	*chppBuffer = chpBufferTemp;

	if (*ipLength == 0)
		return true;
	return true;
}
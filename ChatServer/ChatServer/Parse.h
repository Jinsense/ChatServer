#ifndef _CHATSERVER_PARSE_PARSE_H_
#define _CHATSERVER_PARSE_PARSE_H_

#include <Windows.h>

class CINIParse
{
public:
	enum eNUM_INI_PARSE
	{
		eBUFFER_SIZE = 512000,
	};

	CINIParse();
	~CINIParse();

	void	Initial();
	bool	LoadFile(WCHAR *szFileName);
	bool	ProvideArea(const char *szAreaName);
	bool	GetValue(const char *szName, char *szValue, int *ipBuffSize);
	bool	GetValue(const char *szName, int *ipValue);
	bool	GetValue(const char *szName, float *fpValue);

protected:
	bool	SkipNoneCommand();
	bool	GetNextWord(char **chppBuffer, int *ipLength);
	bool	GetStringWord(char **chppBuffer, int *ipLength);

protected:
	char	*m_chpBuffer;
	int		m_iLoadSize;
	int		m_iBufferAreaStart;
	int		m_iBufferAreaEnd;
	int		m_iBufferFocusPos;
	bool	m_bProvideAreaMode;
};

#endif _CHATSERVER_PARSE_PARSE_H_
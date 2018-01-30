#ifndef _CHATSERVER_LOG_LOG_H_
#define _CHATSERVER_LOG_LOG_H_

#include <direct.h>
#include <wchar.h>
#include <windows.h>
#include <iostream>
#include <strsafe.h>
#include <string>
#include <winerror.h>

enum en_LOG_LEVEL
{
	LOG_DEBUG = 0,
	LOG_WARNING,
	LOG_ERROR,
	LOG_SYSTEM,
};

class CSystemLog
{
private:

	CSystemLog(en_LOG_LEVEL LogLevel);
	~CSystemLog();


public:

	//------------------------------------------------------
	// 싱글톤 클래스, 
	//------------------------------------------------------
	static CSystemLog *GetInstance(en_LOG_LEVEL LogLevel = LOG_DEBUG)
	{
		static CSystemLog Log(LogLevel);
		return &Log;
	}

	//------------------------------------------------------
	// 외부에서 로그레벨 제어
	//------------------------------------------------------
	void SetLogLevel(en_LOG_LEVEL LogLevel) { _SaveLogLevel = LogLevel; }

	//------------------------------------------------------
	// 로그 경로 지정.
	//------------------------------------------------------
	void SetLogDirectory(WCHAR *szDirectory)
	{
		_wmkdir(szDirectory);
		wsprintf(_SaveDirectory, L"%s\\", szDirectory);
	}

	//------------------------------------------------------
	// 실제 로그 남기는 함수.
	//------------------------------------------------------
	void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);

	//------------------------------------------------------
	// BYTE 바이너리를 헥사로 로그 출력
	//------------------------------------------------------
	void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);

	//------------------------------------------------------
	// SessionKey 64개 출력 전용. 이는 문자열이 아니라서 마지막에 널이 없음.
	//------------------------------------------------------
	void LogSessionKey(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pSessionKey);

private:

	unsigned long	_LogNo;
	SRWLOCK			_srwLock;

	en_LOG_LEVEL	_SaveLogLevel;
	WCHAR			_SaveDirectory[25];
};

#endif _CHATSERVER_LOG_LOG_H_
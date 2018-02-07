#include "Log.h"

CSystemLog::CSystemLog(en_LOG_LEVEL LogLevel)
{
	_SaveLogLevel = LogLevel;
	InitializeSRWLock(&_srwLock);
	_LogNo = 0;

}

CSystemLog::~CSystemLog()
{

}

void CSystemLog::Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...)
{

	if (_SaveLogLevel <= LogLevel)
	{
		HRESULT hResult;
		WCHAR szFileName[256];
		WCHAR szInError[256];
		WCHAR szInMessage[512];
		WCHAR szBuf[1024];
		InterlockedIncrement(&_LogNo);

		AcquireSRWLockExclusive(&_srwLock);

		SYSTEMTIME st;
		GetLocalTime(&st);


		hResult = StringCchPrintf(szFileName, 256, L"%d%02d %s.txt", st.wYear, st.wMonth, szType);

		if (FAILED(hResult))
		{
			if (hResult == STRSAFE_E_INVALID_PARAMETER)
			{
				//	길이가 길어서 로그가 짤렸을 경우
				//	로그가 짤렸다는 로그를 추가로 남기고
				//	어떤 로그가 짤렸는지도 남겨줌, 똑같이 저장하면 로그가 짤리므로
				//	길이를 잘라서 넣어줌
			}
		}

		va_list va;
		va_start(va, szStringFormat);
		hResult = StringCchVPrintf(szInMessage, 512, szStringFormat, va);
		va_end(va);

		if (FAILED(hResult))
		{

		}

		hResult = StringCchPrintf(szBuf, 1024, L"[%02d/%02d/%d %02d:%02d:%02d:%08d] %s\n",
			st.wMonth, st.wDay, st.wYear,
			st.wHour, st.wMinute, st.wSecond, _LogNo, szInMessage);

		if (FAILED(hResult))
		{

		}

		//	파일을 열고 닫는 사이 쓰는 과정을 최소로 줄이자.
		FILE * pFile;
		_wfopen_s(&pFile, szFileName, L"a, ccs=UTF-16LE");
		fwrite(szBuf, sizeof(WCHAR), wcslen(szBuf), pFile);
		fclose(pFile);
		ReleaseSRWLockExclusive(&_srwLock);
	}
}

void CSystemLog::LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen)
{

}

void CSystemLog::LogSessionKey(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pSessionKey)
{


}
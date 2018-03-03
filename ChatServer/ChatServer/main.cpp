#include <conio.h>

#include "Config.h"
#include "ChatServer.h"



CConfig _Config;

int main()
{
	bool res = _Config.Set();
	if (false == res)
	{
		wprintf(L"[Server :: Main]	Config Error\n");
		return 0;
	}

	int _In;

	CChatServer _ChatServer;
	SYSTEM_INFO _SysInfo;

	GetSystemInfo(&_SysInfo);
		
	if (false == _ChatServer.ServerStart(_Config.BIND_IP, _Config.BIND_PORT,
				_Config.WORKER_THREAD, true, 
				_Config.CLIENT_MAX))
	{
		wprintf(L"[Main :: Server_Start] Error\n");
		return 0;
	}

	if (false == _ChatServer.LoginServerConnect(_Config.LOGIN_SERVER_IP, _Config.LOGIN_SERVER_PORT,
		true, 2))
	{
		wprintf(L"[Main :: LanClientConnect] Connect Error\n");
		return 0;
	}

	while (!_ChatServer.GetShutDownMode())
	{
		_In = _getch();
		switch (_In)
		{
		case 'q': case 'Q':
		{
			_ChatServer.SetShutDownMode(true);
			wprintf(L"[Main] 서버를 종료합니다.\n");
			_getch();
			break;
		}
		case 'm': case 'M':
		{
			if (false == _ChatServer.GetMonitorMode())
			{
				_ChatServer.SetMonitorMode(true);
				wprintf(L"[Main] MonitorMode Start\n");
			}
			else
			{
				_ChatServer.SetMonitorMode(false);
				wprintf(L"[Main] MonitorMode Stop\n");
			}
		}
		}
	}
	return 0;
}
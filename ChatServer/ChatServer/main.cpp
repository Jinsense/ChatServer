#include "Config.h"
#include "ChatServer.h"

#include <conio.h>

CConfig _Config;

int main()
{
	bool res = _Config.Set();
	if (false == res)
	{
		wprintf(L"[Server :: Main]	Config Error\n");
		return 0;
	}

	int _Retval;
	int _In;

	CChatServer _Server;
	SYSTEM_INFO _SysInfo;

	GetSystemInfo(&_SysInfo);
	
	if ((_Retval = _Server.ServerStart(_Config.BIND_IP, _Config.BIND_PORT,
				_Config.WORKER_THREAD, true, 
				_Config.CLIENT_MAX)) == false)
	{
		wprintf(L"[Server :: Server_Start] Error\n");
		return 0;
	}

	while (!_Server.GetShutDownMode())
	{
		_In = _getch();
		switch (_In)
		{
		case 'q': case 'Q':
		{
			_Server.SetShutDownMode(true);
			wprintf(L"[Main] 서버를 종료합니다.\n");
			_getch();
			break;
		}
		case 'm': case 'M':
		{
			if (false == _Server.GetMonitorMode())
			{
				_Server.SetMonitorMode(true);
				wprintf(L"[Main] MonitorMode Start\n");
			}
			else
			{
				_Server.SetMonitorMode(false);
				wprintf(L"[Main] MonitorMode Stop\n");
			}
		}
		}
	}
	return 0;
}
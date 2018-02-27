#ifndef _CHATSERVER_PARSE_CONFIG_H_
#define _CHATSERVER_PARSE_CONFIG_H_

#include "Parse.h"

class CConfig
{
	enum eNumConfig
	{
		eNUM_BUF = 100,
	};
public:
	CConfig();
	~CConfig();

	bool Set();

public:
	int PACKET_CODE;
	int PACKET_KEY1;
	int PACKET_KEY2;

	char BIND_IP[20];
	int BIND_IP_SIZE;
	int BIND_PORT;

	char LOGIN_SERVER_IP[20];
	int LOGIN_IP_SIZE;
	int LOGIN_SERVER_PORT;

	char MONITORING_SERVER_IP[20];
	int MONITORING_IP_SIZE;
	int MONITORING_SERVER_PORT;

	int WORKER_THREAD;
	int CLIENT_MAX;
	int TIMEOUT_TIME;

	CINIParse _Parse;
};

#endif _CHATSERVER_PARSE_CONFIG_H_

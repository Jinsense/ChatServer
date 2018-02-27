#include <windows.h>

#include "Config.h"

CConfig::CConfig()
{
	PACKET_CODE = NULL;
	PACKET_KEY1 = NULL;
	PACKET_KEY2 = NULL;

	ZeroMemory(&BIND_IP, sizeof(BIND_IP));
	BIND_IP_SIZE = eNUM_BUF;
	BIND_PORT = NULL;

	ZeroMemory(&LOGIN_SERVER_IP, sizeof(LOGIN_SERVER_IP));
	LOGIN_IP_SIZE = eNUM_BUF;
	LOGIN_SERVER_PORT = NULL;

	ZeroMemory(&MONITORING_SERVER_IP, sizeof(MONITORING_SERVER_IP));
	MONITORING_IP_SIZE = eNUM_BUF;
	MONITORING_SERVER_PORT = NULL;

	WORKER_THREAD = NULL;
	CLIENT_MAX = NULL;
	TIMEOUT_TIME = NULL;
}

CConfig::~CConfig()
{

}

bool CConfig::Set()
{
	bool res = true;
	res = _Parse.LoadFile("ChatServer_Config.ini");
	if (false == res)
		return false;
	_Parse.ProvideArea("PACKET");
	_Parse.GetValue("PACKET_CODE", &PACKET_CODE);
	_Parse.GetValue("PACKET_KEY1", &PACKET_KEY1);
	_Parse.GetValue("PACKET_KEY2", &PACKET_KEY2);

	_Parse.ProvideArea("CLIENT_BIND");
	_Parse.GetValue("BIND_IP", &BIND_IP[0], &BIND_IP_SIZE);
	_Parse.GetValue("BIND_PORT", &BIND_PORT);

	_Parse.ProvideArea("LOGIN_SERVER");
	_Parse.GetValue("LOGIN_SERVER_IP", &LOGIN_SERVER_IP[0], &LOGIN_IP_SIZE);
	_Parse.GetValue("LOGIN_SERVER_PORT", &LOGIN_SERVER_PORT);

	_Parse.ProvideArea("MONITORING_SERVER");
	_Parse.GetValue("MONITORING_SERVER_IP", &MONITORING_SERVER_IP[0], &MONITORING_IP_SIZE);
	_Parse.GetValue("MONITORING_SERVER_PORT", &MONITORING_SERVER_PORT);

	_Parse.ProvideArea("CHAT_SERVER");
	_Parse.GetValue("WORKER_THREAD ", &WORKER_THREAD);
	_Parse.GetValue("CLIENT_MAX ", &CLIENT_MAX);
	_Parse.GetValue("TIMEOUT_TIME ", &TIMEOUT_TIME);

	return true;
}
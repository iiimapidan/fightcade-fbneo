#pragma once

#include "Singleton.h"
#include "hv/TcpClient.h"

using namespace hv;

#pragma comment(lib, "hv_static.lib")
#pragma comment(lib, "hv.lib")


class NetCode {
public:
	NetCode();
	~NetCode();


	bool init();

private:
	bool connectServer();


private:
	TcpClient _client;
};

typedef SingletonClass<NetCode> NetCodeManager;
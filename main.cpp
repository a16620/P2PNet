#include "P2P.h"
#include <iostream>
#include<uuids.h>
#pragma comment(lib, "Rpcrt4.lib")

int main() {
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);

	char ip[20];
	cout << "ip:";
	ULONG addr;
	cin >> ip;
	if (ip[0] != 'n') {
		addr = inet_addr(ip);
	}
	else {
		addr = INADDR_NONE;
	}
	NetRouter r(addr);

	r.Start();
	while (true)
	{
		cin >> ip;
		if (ip[0] = 's')
			break;
	}
	r.Stop();

	WSACleanup();
	return 0;
}


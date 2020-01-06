#include "P2P.h"
#include <iostream>
#include<uuids.h>
#pragma comment(lib, "Rpcrt4.lib")

int main() {
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	NetRouter r;

	r.Start();
	getchar();
	r.Stop();

	WSACleanup();
	return 0;
}


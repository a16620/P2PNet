#pragma once
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <memory>
#include "Buffer.h"
using namespace std;

class SocketRAII {
private:
	SOCKET m_socket;
public:
	SocketRAII(SOCKET socket);
	SocketRAII(SocketRAII&& socket) noexcept;
	~SocketRAII();
	void reset(SOCKET socket = INVALID_SOCKET);
	SocketRAII& operator=(SocketRAII&& other) noexcept;
	SOCKET getHandle() const;
};

class SocketException
{
public:
	enum E {
		DISCONN,
		REFUSE,
		BUFFER
	};
private:
	int ecode;
public:
	SocketException(int code) : ecode(code) {};
	int GetErrorCode() const { return ecode; }
};

class TcpCommunicator
{
private:
	SocketRAII m_socket;
public:
	TcpCommunicator(SocketRAII&& socket);
	~TcpCommunicator();

	unique_ptr<CBuffer> Receive();
	auto Send(const CBuffer& buffer);
	void reset(SocketRAII&& socket);
	auto getHandle() {
		return m_socket.getHandle();
	}


	void Shutdown(int how);
	void Close();
};

class TcpListener
{
private:
	SocketRAII m_socket;
	sockaddr_in m_addr;
public:
	TcpListener(SocketRAII&&);
	~TcpListener();
	void SetAddress(const ULONG& address, int port);
	void Listen(int backlog);
	void Bind();
	SocketRAII Accept();
	void Shutdown(int how);
	void Close();
};

auto make_tcpSocket() {
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		throw SocketException(0);
	return s;
}

void Bind(SOCKET s, const ULONG& address, int port) {
	sockaddr_in addr;
	ZeroMemory(&addr, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = htons(port);
	bind(s, (sockaddr*)&addr, sizeof(sockaddr_in));

}
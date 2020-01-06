#include "Network.h"

SocketRAII::SocketRAII(SOCKET socket) : m_socket(socket)
{
}

SocketRAII::SocketRAII(SocketRAII&& socket) noexcept
{
	m_socket = socket.m_socket;
	socket.m_socket = INVALID_SOCKET;
}

SocketRAII::~SocketRAII()
{
	if (m_socket != INVALID_SOCKET)
		closesocket(m_socket);
}

void SocketRAII::reset(SOCKET socket)
{
	if (m_socket != INVALID_SOCKET)
		closesocket(m_socket);
	m_socket = socket;
}

void SocketRAII::release()
{
	shutdown(m_socket, SD_BOTH);
	reset(INVALID_SOCKET);
}

SocketRAII& SocketRAII::operator=(SocketRAII&& other) noexcept
{
	m_socket = other.m_socket;
	other.m_socket = INVALID_SOCKET;
	return *this;
}

SOCKET SocketRAII::getHandle() const
{
	return m_socket;
}

TcpCommunicator::TcpCommunicator(SOCKET socket) : m_socket(socket)
{
}

TcpCommunicator::~TcpCommunicator()
{
	Close();
}

unique_ptr<CBuffer> TcpCommunicator::Receive()
{
	long len = 0, t = 0;
	do {
		int t_;
		t_ = recv(m_socket, ((char*)&len) + t, 4 - t, 0);
		if (t_ == -1)
		{
			throw SocketException(SocketException::E::DISCONN);
		}
		else
			t += t_;
	} while (t < 4);

	len = ntohl(len);
	auto buffer = make_unique<CBuffer>(len);
	t = 0;
	do {
		int t_;
		t_ = recv(m_socket, ((char*)buffer->getBuffer()) + t, len - t, 0);
		if (t_ == -1)
		{
			buffer.release();
			throw SocketException(SocketException::E::DISCONN);
		}
		else
			t += t_;
	} while (t < len);

	return buffer;
}

auto TcpCommunicator::Send(const CBuffer& buffer)
{
	int sz = buffer.size();
	if (sz == 0)
	{
		throw SocketException(SocketException::E::BUFFER);
	}
	long _sz = htonl(sz);
	int r = 0;
	do {
		auto res = send(m_socket, (char*)&_sz, 4, 0);
		if (res == -1)
			throw SocketException(SocketException::E::DISCONN);
		else
			r += res;
	} while (r < 4);

	r = 0;
	do
	{
		int r_ = send(m_socket, buffer.getBuffer() + r, sz - r, 0);
		if (r_ == -1)
			throw SocketException(SocketException::E::DISCONN);
		r += r_;
	} while (r < sz);

	return;
}

void TcpCommunicator::reset(SOCKET socket)
{
	m_socket = socket;
}

void TcpCommunicator::Shutdown(int how)
{
	shutdown(m_socket, how);
}

void TcpCommunicator::Close()
{
	closesocket(m_socket);
}

TcpListener::TcpListener(SOCKET socket) : m_socket(socket)
{
	ZeroMemory(&m_addr, sizeof(sockaddr_in));
	m_addr.sin_family = AF_INET;
	int opt = 1;
	setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
}

TcpListener::~TcpListener()
{
	Close();
}

void TcpListener::SetAddress(const ULONG& address, int port)
{
	m_addr.sin_addr.s_addr = htonl(address);
	m_addr.sin_port = htons(port);
}

void TcpListener::Listen(int backlog)
{
	listen(m_socket, backlog);
}

void TcpListener::Bind()
{
	bind(m_socket, (sockaddr*)&m_addr, sizeof(sockaddr_in));
}

SOCKET TcpListener::Accept()
{
	sockaddr_in addr;
	int sz = sizeof(sockaddr_in);
	return accept(m_socket, (sockaddr*)&addr, &sz);
}

SOCKET TcpListener::Accept(ULONG* addr)
{
	sockaddr_in _addr;
	int sz = sizeof(sockaddr_in);
	auto s = accept(m_socket, (sockaddr*)&_addr, &sz);
	*addr = _addr.sin_addr.S_un.S_addr;
	return s;
}

void TcpListener::Shutdown(int how)
{
	shutdown(m_socket, how);
}

void TcpListener::Close()
{
	closesocket(m_socket);
}

void Bind(SOCKET s, const ULONG& address, int port)
{
	sockaddr_in addr;
	ZeroMemory(&addr, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = htons(port);
	bind(s, (sockaddr*)&addr, sizeof(sockaddr_in));
}

void PerfectSend(SOCKET s, CBuffer& buffer)
{
	int sz = buffer.size();
	if (sz == 0)
	{
		throw SocketException(SocketException::E::BUFFER);
	}
	long _sz = htonl(sz);
	int r = 0;
	do {
		auto res = send(s, (char*)&_sz, 4, 0);
		if (res == -1)
			throw SocketException(SocketException::E::DISCONN);
		else
			r += res;
	} while (r < 4);

	r = 0;
	do
	{
		int r_ = send(s, buffer.getBuffer() + r, sz - r, 0);
		if (r_ == -1)
			throw SocketException(SocketException::E::DISCONN);
		r += r_;
	} while (r < sz);
}

SOCKET make_tcpSocket() {
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (s == INVALID_SOCKET)
		throw SocketException(0);
	return s;
}
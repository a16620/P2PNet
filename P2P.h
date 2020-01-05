#pragma once
#include "Network.h"
#include <thread>
#include <future>
#include <atomic>
#include <list>
#include <queue>
using namespace std;

#define NT_FLAG_IP_REQ 0b01
#define NT_FLAG_IP_RES 0b10

constexpr int ttl_default = 30;
constexpr int port_router = 511;
/*
	Router
	-���� ������ (������ 1)-�񵿱� �۾�
		�̹� ����Ʈ�� ��ȭ�� ��� ����Ʈ ����� �ϳ��� ������ �佺�Ѵ�
	-��ε� ĳ���� (������ 2)
		-TTLȮ��
		-����
	-���� ���� (������ 2)
		-�ֱ������� ������ ���� ���� Ȯ�� (��->����)
		-�ֺ� ���� ���ؼ� �ڽ� �� 2 �̻� ���̳��� �ڽ��� ��带 ��忡�� ����
			-��忡�� ���� ��û�� ������ ���� ���� ���� �� ���� ����
			-���� ���� ���� ������ ��û�� �ּ� �� �ϳ��� �����Ѵ�
*/
class NetRouter {
	atomic_bool isRunning;
	atomic_int16_t cntNeighbor;

	thread listening;
	SocketRAII listener;

	SequentArrayList<WSAEVENT, WSA_MAXIMUM_WAIT_EVENTS> m_arrEvt;
	SequentArrayList<SocketRAII, WSA_MAXIMUM_WAIT_EVENTS> m_arrCon;
	SequentArrayList<RecyclerBuffer<100>, WSA_MAXIMUM_WAIT_EVENTS> m_arrBuf;

	mutex m_Bmutex, m_Smutex;
	queue<CBuffer> m_buffer;
	
private:
	void Routing();
	void EndRouting();
	void Listening();
	void EndListening();
	void Tasking();
	void Refresh();
	void Replace();

	void AddEvent(SOCKET s, WSAEVENT e);
public:
	NetRouter();
	bool Join();
	void End();
};

auto getConnectedSocket(const ULONG& ta) {
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ta;
	addr.sin_port = htons(port_router);
	while (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in)));
	return s;
}

class NetHost {
	
public:

};

struct NetPacketHeader {
	BYTE priority : 2;
	BYTE ttl : 6;
	BYTE flag;
	ULONG from;
};

class PacketFactory {
public:
	static constexpr auto szHeader = sizeof(NetPacketHeader);

	static auto NetPacketBuffer(const NetPacketHeader& header, const CBuffer& content) {
		auto buffer = make_unique<CBuffer>(static_cast<int>(szHeader + content.size()));
		memcpy(buffer->getBuffer(), &header, szHeader);
		memcpy(buffer->getBuffer() + szHeader, content.getBuffer(), content.size());
		return buffer;
	}

	static auto NetPacketIP(const NetPacketHeader& header, const ULONG& ip) {
		auto buffer = make_unique<CBuffer>(static_cast<int>(szHeader + sizeof(ULONG)));
		memcpy(buffer->getBuffer(), &header, szHeader);
		memcpy(buffer->getBuffer() + szHeader, &ip, sizeof(ULONG));
		return buffer;
	}
};
#pragma once
#include "Network.h"
#include "Utils.h"
#include <uuids.h>
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
constexpr int NetMaxChild = min(WSA_MAXIMUM_WAIT_EVENTS, 5);
constexpr int NetTotalCount = NetMaxChild+2;
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
private:
	atomic_bool isRunning;
	atomic_uint8_t cntChild;
	

	thread routing, tasking, subtasking;

	TcpListener listener;
	SOCKET parent;
	ULONG parentid;
	ULONG rep_addr;

	AtomicMinimum<BYTE, ULONG> orderer;

	struct SOCKETIN {
		int needSize = -1;
		BYTE cntChild = NetMaxChild;
		SOCKET socket;
		ULONG id;
		RecyclerBuffer<256> buffer;
	};

	mutex  m_modifyLock;
	SequentArrayList<WSAEVENT, NetTotalCount> m_arrEvt;
	SequentArrayList<SOCKETIN, NetTotalCount> m_arrIn;

	struct PacketPair {
		ULONG id;
		CBuffer buffer;
	};

	mutex m_packetLock;
	queue<PacketPair> m_packetBuffer;
	
	mutex m_outputLock;
	condition_variable cv;
	atomic_bool newdata;
	queue<PacketPair> m_outputBuffer;

private:
	void Stacking();
	void Tasking();
	void SubTasking();
	void ClearStacking();
	void StopListening();

	void CalculateOptimalNode();

	void ConnectToParent(ULONG addr);
	void BroadCastToNodes(CBuffer& packet, const ULONG& filter);

	void OutputQueue(PacketPair&& packet);

	void AddEvent(SOCKET s, WSAEVENT e, ULONG addr);
	void AddListenEvent(SOCKET s, WSAEVENT e, ULONG addr);
	void RemoveEvent(int idx);
public:
	NetRouter(ULONG _parent);
	void Start();
	void Stop();
};

class NetHost {
	
public:

};

enum class NetPacketProtocol : BYTE {
	REPLACE_ADDR,
	QUERY,
	CHILDCOUNT
};

enum class NetPacketFlag : BYTE {
	CONN,
	DISCONN,
	REFUSE
};

struct NetPacketHeader {
	NetPacketProtocol protocol;
	NetPacketFlag flag;
};

class PacketFactory {
public:
	static constexpr auto szHeader = sizeof(NetPacketHeader);

	static auto NetPacketBuffer(const NetPacketHeader& header, const CBuffer& content) {
		CBuffer buffer(static_cast<int>(szHeader + content.size()));
		memcpy(buffer.getBuffer(), &header, szHeader);
		memcpy(buffer.getBuffer() + szHeader, content.getBuffer(), content.size());
		return buffer;
	}

	static auto NetPacketIP(ULONG ip, NetPacketFlag flag=NetPacketFlag::CONN) {
		CBuffer buffer(static_cast<int>(szHeader + sizeof(ULONG)));
		static NetPacketHeader header{NetPacketProtocol::REPLACE_ADDR, NetPacketFlag::CONN};
		header.flag = flag;
		memcpy(buffer.getBuffer(), &header, szHeader);
		memcpy(buffer.getBuffer() + szHeader, &ip, sizeof(ULONG));
		return buffer;
	}

	static auto NetPacketChildCnt(BYTE cnt) {
		CBuffer buffer(static_cast<int>(szHeader + 1));
		static constexpr NetPacketHeader header{ NetPacketProtocol::CHILDCOUNT, NetPacketFlag::CONN };
		memcpy(buffer.getBuffer(), &header, szHeader);
		*reinterpret_cast<BYTE*>(buffer.getBuffer() + szHeader) = cnt;
		return buffer;
	}
};
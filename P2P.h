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
	-소켓 리스닝 (스레드 1)-비동기 작업
		이미 리스트가 포화인 경우 리스트 요소중 하나로 연결을 토스한다
	-브로드 캐스팅 (스레드 2)
		-TTL확인
		-전송
	-연결 관리 (스레드 2)
		-주기적으로 끊어진 소켓 유뮤 확인 (참->제거)
		-주변 노드와 비교해서 자식 수 2 이상 차이나는 자신의 노드를 노드에게 이전
			-노드에게 이전 요청을 보내고 수락 문자 받은 후 연결 해제
			-수락 노드는 연결 해제후 요청의 주소 중 하나로 연결한다
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
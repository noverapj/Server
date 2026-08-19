#pragma once

#include <mutex>
#include <condition_variable>

typedef Singleton<LSPacketQueue> S_Queue;
typedef Singleton<IOCPHandler> S_Iocp;
typedef Singleton<ioConfiguration> S_Conf;
typedef Singleton<LSLogic> S_Logic;
typedef Singleton<ioLoginSvrBind> S_LoginServer;
typedef Singleton<ClientsManager> S_ClientManager;
typedef Singleton<ioLoginServerState> S_ioLoginServerState;
typedef Singleton<ioTestClass> S_Test;

struct ServiceStopSignal {
	std::mutex mtx;
	std::condition_variable cv;
	bool stopped = false;
	void stop() { std::lock_guard<std::mutex> lk(mtx); stopped = true; cv.notify_all(); }
	void wait() { std::unique_lock<std::mutex> lk(mtx); cv.wait(lk, [this] { return stopped; }); }
};
typedef Singleton<ServiceStopSignal> S_IO;

typedef Singleton<ServerConnectorMgr> S_ServerConnectMgr;
typedef Singleton<OpMemPool> S_OPMemPool;
typedef Singleton<LSTimerMgr> S_TimerMgr;
typedef Singleton<ioProcessChecker> S_IOProcess;
typedef Singleton<ioPacketChecker> S_IOPacket;
typedef Singleton<ConnectAssist> S_ConnectAssist;
typedef Singleton<ServerInfoManager> S_ServerInfoMgr;
typedef Singleton<UserInfoManager> S_UserInfoMgr;

#define  g_Queue()			 S_Queue::instance()
#define  g_Iocp()			 S_Iocp::instance()
#define  g_Config()			 S_Conf::instance()
#define  g_Logic()			 S_Logic::instance()
#define  g_LoginServerInfo()	 S_LoginServer::instance()
#define  g_ClientMgr()		 S_ClientManager::instance()
#define  g_State()			 S_ioLoginServerState::instance()
#define  g_ServerConnectMgr()  S_ServerConnectMgr::instance()
#define  g_OPMemPool()		 S_OPMemPool::instance()
#define  g_TimerMgr()		 S_TimerMgr::instance()
#define  g_ProcessChecker()	 S_IOProcess::instance()
#define  g_PacketChecker()	 S_IOPacket::instance()
#define  g_ConnectAssist()	 S_ConnectAssist::instance()
#define  g_ServerInfoMgr()	 S_ServerInfoMgr::instance()
#define  g_UserInfoMgr()       S_UserInfoMgr::instance()
 



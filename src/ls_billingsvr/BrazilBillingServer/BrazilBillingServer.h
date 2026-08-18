#ifndef __BrazilBillingServer_h__
#define __BrazilBillingServer_h__

class CConnectNode;

class BrazilBillingServer : public CConnectNode
{
protected:
	enum 
	{ 
		//UPDATE_TIME = 180000, // 3분 connect() 함수 딜레이 때문
		UPDATE_TIME = 60000, // 3분 connect() 함수 딜레이 때문
	};

protected:
	static BrazilBillingServer *sg_Instance;
	DWORD  m_dwCurrentTime;
	bool   m_bBRAZILSendAlive;
	bool   m_bConnect;
	char   m_szServerIP[MAX_PATH];
	int    m_iBRAZILPort;
	
public:
	static BrazilBillingServer &GetInstance();
	static void ReleaseInstance();

public:
	virtual void SessionClose( BOOL safely=TRUE );
	virtual void DispatchReceive(CPacket& packet, DWORD bytesTransferred);
	virtual bool SendMessage( CPacket &rkPacket );
	virtual void ReceivePacket( CPacket &packet );
	virtual void PacketParsing( CPacket &packet );
	virtual bool AfterCreate();


public:
	virtual void OnCreate();       //초기화
	virtual void OnDestroy();
	virtual bool CheckNS( CPacket &rkPacket );	
	virtual int  GetConnectType();


public:
	bool m_bDisconn;
	
public:
	bool ConnectTo( bool bStart );
	bool SendInitPacket( bool );

protected:
	void InitData();

public:
	void ProcessTime();

public:
	void OnAlive( SP2Packet &rkPacket );
	void OnBalance( SP2Packet &rkPacket );
	void OnBuy( SP2Packet &rkPacket );
	void OnCancel( SP2Packet &rkPacket );
	void OnClose( SP2Packet &rkPacket );
	void OnHealthCheck( SP2Packet &rkPacket );
	void OnCheckPremium2( SP2Packet &rkPacket );
	void OnGSConnect( SP2Packet &rkPacket );

protected:
	BrazilBillingServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize );
	virtual ~BrazilBillingServer();
};
#define g_BrazilBillingServer BrazilBillingServer::GetInstance()
#endif // __BrazilBillingServer_h__
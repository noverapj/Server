#ifndef __BrazilAuthServer_h__
#define __BrazilAuthServer_h__

class CConnectNode;

class BrazilAuthServer : public CConnectNode, public nsGSS::GsAuthBaseClient
{
protected:
	enum 
	{ 
		UPDATE_TIME = 10000, // 3분 connect() 함수 딜레이 때문
	};

protected:
	static BrazilAuthServer *sg_Instance;
	DWORD  m_dwCurrentTime;
	bool   m_bSendAlive;

	
public:
	static BrazilAuthServer &GetInstance();
	static void ReleaseInstance();

public:
	virtual void SessionClose( BOOL safely=TRUE );
	bool OnSendAuth( char * szToken, DWORD dwKey  );


public:
	virtual void OnCreate();       //초기화
	virtual void OnDestroy();
	virtual bool CheckNS( CPacket &rkPacket );	
	virtual int  GetConnectType();
	virtual void PacketParsing( CPacket &packet );

public:
	
	
public:
	bool ConnectTo( bool bStart );

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

	VOID recvTokenResult(UINT _memberId, CHAR* _userId, INT _result, INT _identity);
	VOID recvKickUser(UINT _memberId, INT _reason);
	VOID recvLogoutUserResult(UINT _memberId, BOOL _retType);
	
protected:
	BrazilAuthServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize );
	virtual ~BrazilAuthServer();
};
#define g_BrazilAuthServer BrazilAuthServer::GetInstance()
#endif // __BrazilAuthServer_h__
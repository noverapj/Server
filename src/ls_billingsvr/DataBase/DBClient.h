// DBClient.h: interface for the DBClient class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DBCLIENT_H__8BBCE6AF_B8A6_4D7C_A9FA_B2B4B32491CA__INCLUDED_)
#define AFX_DBCLIENT_H__8BBCE6AF_B8A6_4D7C_A9FA_B2B4B32491CA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../QueryData/QueryData.h"
#include "../util/cSerialize.h"

//DB AGENT MSG TYPE
// GET : Select , SET : Insert , DEL : Delete , UPD : Update
#define DBAGENT_PHILLIPPINE_AUTOLOGIN				0x0005
#define DBAGENT_PHILLIPPINE_LOGIN					0x0010
#define DBAGENT_GAME_PINGPONG						0x0999




//작업 방식
#define _INSERTDB       0
#define _DELETEDB       1
#define _SELECTDB       2
#define _UPDATEDB       3   
#define _SELECTEX1DB    4 

//결과 행동
#define _RESULT_CHECK   0
#define _RESULT_NAUGHT  1
#define _RESULT_DESTROY 2



class CConnectNode;
class DBClient : public CConnectNode 
{
private:
	static DBClient *sg_Instance;
	DWORD	m_dwCurrentTime;
	int		m_iClassPriceTime;
	int		m_iDBAgentThreadID;
	bool	m_bOnceRun;
protected:
	ioHashString	m_DBAgentIP;
	int				m_iDBAgentPort;

	cSerialize		m_FT;
	vVALUETYPE		m_VT;
	CQueryData		m_Query;

public:
	static DBClient &GetInstance();
	static void ReleaseInstance();

private:
	ValueType GetValueType(VariableType nType,int len);

public:
	inline ioHashString &GetDBAgentIP(){ return m_DBAgentIP; }
	inline int GetDBAgentPort(){ return m_iDBAgentPort; }

public:
	virtual void SessionClose( BOOL safely=TRUE );
	virtual bool SendMessage( CPacket &rkPacket );
	virtual void ReceivePacket( CPacket &packet );
	virtual void PacketParsing( CPacket &packet );

public:
	virtual void OnCreate();       //초기화
	virtual void OnDestroy();
	virtual bool CheckNS( CPacket &rkPacket );	
	virtual int  GetConnectType();

public:
	bool ConnectTo();

private:
	void OnPing();

public:
	void ProcessTime();

public:
	int Reset(const int iQueryID);

	void OnClose( SP2Packet &packet );

	void OnPhilippineAutoLogin( const ioData &rData );
	void OnPhilippineLogin( const ioData &rData );

private:
	DBClient( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize );
	virtual ~DBClient();
};

#define g_DBClient DBClient::GetInstance()

#endif // !defined(AFX_DBCLIENT_H__8BBCE6AF_B8A6_4D7C_A9FA_B2B4B32491CA__INCLUDED_)
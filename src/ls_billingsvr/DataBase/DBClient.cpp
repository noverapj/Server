// DBClient.cpp: implementation of the DBClient class.
//
//////////////////////////////////////////////////////////////////////

#include "../stdafx.h"
#include "DBClient.h"

#include "../MainProcess.h"
#include "../ioProcessChecker.h"
#include "../QueryData/QueryResultData.h"
#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../Network/ioPacketQueue.h"
#include "../EtcHelpFunc.h"
#include "../ThreadPool/ioThreadPool.h"

#include <strsafe.h>

extern CLog LOG;

DBClient *DBClient::sg_Instance = NULL;

extern BOOL tokenize(const std::string str, const std::string& delimiters, std::vector<std::string>& tokens);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DBClient::DBClient( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	m_dwCurrentTime   = 0;
	m_iClassPriceTime = 0;
	m_iDBAgentThreadID= 0;
	m_iDBAgentPort    = 0;
	m_bOnceRun		  = false;
}

DBClient::~DBClient()
{
}

DBClient &DBClient::GetInstance()
{
	if(sg_Instance == NULL)
	{
		ioINILoader kLoader( "ls_config_billingsvr.ini" );
		kLoader.SetTitle( "Server Session Buffer" );
		int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
		int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
		sg_Instance = new DBClient( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize );
	}
	return *sg_Instance;
}

void DBClient::ReleaseInstance()
{		
	SAFEDELETE(sg_Instance);
}


bool DBClient::ConnectTo()
{
	ioINILoader kLoader( "../global_define.ini" );
	kLoader.SetTitle( "DBA_AUTH" );

	char szValue[MAX_PATH];
	kLoader.LoadString( "1", "", szValue, MAX_PATH );
	if( strcmp( szValue, "" ) == 0 )
	{
		LOG.PrintTimeAndLog( 0, "DBClient::ConnectTo IP is empty" );
		return false;
	}

	std::string values = szValue;
	std::string delimiter = ":";
	std::vector<std::string> tokens;
	tokenize(values, delimiter, tokens);
	if(tokens.size() != 2) return false;

	m_DBAgentIP		= tokens[0].c_str();
	m_iDBAgentPort	= atoi(tokens[1].c_str());

	SOCKET socket = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( socket == INVALID_SOCKET )
	{
		LOG.PrintTimeAndLog( 0, "DBClient::ConnectTo socket %d[%s:%d]", GetLastError(), m_DBAgentIP.c_str(), m_iDBAgentPort );
		return false;
	}

	sockaddr_in serv_addr;
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr( m_DBAgentIP.c_str() );
	serv_addr.sin_port			= htons( m_iDBAgentPort );
	if( ::connect( socket, (sockaddr*)&serv_addr, sizeof(serv_addr) ) != 0 )
	{
		LOG.PrintTimeAndLog( 0, "DBClient::ConnectTo connect %d[%s:%d]", GetLastError(), m_DBAgentIP.c_str(), m_iDBAgentPort );
		return false;
	}

	g_iocp.AddHandleToIOCP( (HANDLE)socket, (DWORD)this );
	CConnectNode::SetSocket( socket );

	kLoader.SetTitle( "ClassPrice" );
	m_iClassPriceTime = kLoader.LoadInt( "LoadTime", 10 );

	OnCreate();	
	AfterCreate();
	LOG.PrintTimeAndLog( 0, "OnConnect (IP:%s PORT:%d RESULT:%d)", m_DBAgentIP.c_str(), m_iDBAgentPort, 0 );
	return true;
}


void DBClient::OnCreate()
{
	CConnectNode::OnCreate();

	m_dwCurrentTime = TIMEGETTIME();
}

void DBClient::OnDestroy()
{
	CConnectNode::OnDestroy();
	LOG.PrintTimeAndLog( 0, "DBClient : Disconnect" );
}

ValueType DBClient::GetValueType(VariableType nType,int len)
{
	ValueType vt;
	vt.type = nType;
	vt.size = len;
	return vt;
}

bool DBClient::SendMessage( CPacket &rkPacket )
{
	g_ProcessChecker.DBServerSendMessage( rkPacket.GetBufferSize() );
	if( !CConnectNode::SendMessage( rkPacket, TRUE ) )
	{
		LOG.PrintTimeAndLog(0, "DBClient send failed : packet(%d), error(%lu)", rkPacket.GetPacketID(), GetLastError());
		return false;
	}
	return true;
}

bool DBClient::CheckNS( CPacket &rkPacket )
{
	return true;             //네트웍 감시 필요없다.
}

int DBClient::GetConnectType()
{
	return CONNECT_TYPE_AUTHDB_SERVER;
}

void DBClient::SessionClose( BOOL safely )
{
	if(IsActive())
	{
		CPacket packet(ITPK_CLOSE_SESSION);
		ReceivePacket( packet );
	}
}


void DBClient::ReceivePacket( CPacket &packet )
{
	switch( packet.GetPacketID() )
	{
	case DTPK_QUERY:
		g_RecvQueue.InsertQueue( (DWORD)this, packet, PK_QUEUE_QUERY );
		break;
	default:
		break;
	}
}

void DBClient::PacketParsing( CPacket &packet )
{
	switch(packet.GetPacketID())
	{
	case ITPK_CLOSE_SESSION:
		OnClose( (SP2Packet&)packet );
		break;
	}	
}

void DBClient::OnClose( SP2Packet &packet )
{
	OnDestroy();
}

void DBClient::ProcessTime()
{
	if(TIMEGETTIME() - m_dwCurrentTime < 60000) 
		return; // 1분 확인

	FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 초 이상 걸리면로그 남김

	if( !IsActive() )
	{
		if( g_App.IsTestMode() )
			ConnectTo();
		else
			::PostQuitMessage( 0 );
	}

	OnPing();
	m_dwCurrentTime = TIMEGETTIME();
}

int DBClient::Reset(const int iQueryID)
{
	m_FT.Reset();
	m_VT.clear();
	m_Query.Clear();

	return iQueryID;
}


void DBClient::OnPing()
{
#ifdef __OHTG_LOCAL_PHILIPPINE__
	const int queryId = Reset(1); // 실제로는 존재하지 않는 쿼리

	DWORD dwCurrent = TIMEGETTIME();

	m_Query.SetReturnData( &dwCurrent, sizeof(DWORD) );

	cSerialize v_FT;
	vVALUETYPE v_VT;

	m_Query.SetData( 
		++m_iDBAgentThreadID, 
		_RESULT_CHECK, 
		DBAGENT_GAME_PINGPONG, 
		_UPDATEDB,
		queryId, 
		m_FT, 
		m_VT );

	SP2Packet kPacket( DTPK_QUERY );
	if( kPacket.Write(m_Query) )
	{
		SendMessage( kPacket );
	}
#endif //__OHTG_LOCAL_PHILIPPINE__
}


//////////////////////////////////////////////////////////////////////////
// SEND
//////////////////////////////////////////////////////////////////////////

void DBClient::OnPhilippineAutoLogin( const ioData &rData)
{
#ifdef __OHTG_LOCAL_PHILIPPINE__
	const int queryId = 300; // 실제로는 존재하지 않는 쿼리

	INT64 i64AccountIndex = 0;
	int iIP = inet_addr( rData.GetUserIP().c_str() );
	cSerialize v_FT;
	vVALUETYPE v_VT;

	v_FT.Write( (short)rData.GetServerNo());
	v_FT.Write( rData.GetPrivateID().c_str(), rData.GetPrivateID().Length(), TRUE );
	v_FT.Write( rData.GetEncodePW().c_str(), rData.GetEncodePW().Length(), TRUE );
	v_FT.Write( iIP );
	v_VT.push_back( GetValueType(vLONG, sizeof(LONG) ) );				//Return;

	int iServerPort = rData.GetServerPort();
	CQueryData query_data;
	query_data.SetReturnData( rData.GetServerIP().c_str(), IP_NUM_PLUS_ONE );
	query_data.SetReturnData( &iServerPort, sizeof(INT) );
	query_data.SetReturnData( rData.GetBillingGUID().c_str(), USER_GUID_NUM_PLUS_ONE );
	query_data.SetReturnData( rData.GetPrivateID().c_str(), ID_NUM_PLUS_ONE );
	query_data.SetData( 
		++m_iDBAgentThreadID, 
		_RESULT_CHECK, 
		DBAGENT_PHILLIPPINE_AUTOLOGIN, 
		_UPDATEDB,
		queryId, 
		v_FT, 
		v_VT );

	LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetServerIP().c_str(), rData.GetEncodePW().c_str(), rData.GetEncodePW().Length());
	SP2Packet kPacket( DTPK_QUERY );
	if( kPacket.Write(query_data) )
	{
		LOG.PrintTimeAndLog(0, "Test %s pServerNode == NULL:%s:%s:%s", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetServerIP().c_str());
		SendMessage( kPacket );
	}
#endif //__OHTG_LOCAL_PHILIPPINE__
}

void DBClient::OnPhilippineLogin( const ioData &rData)
{
#ifdef __OHTG_LOCAL_PHILIPPINE__
	const int queryId = 300; // 실제로는 존재하지 않는 쿼리

	INT64 i64AccountIndex = 0;
	int iIP = inet_addr( rData.GetUserIP().c_str() );
	cSerialize v_FT;
	vVALUETYPE v_VT;

	v_FT.Write( (short)rData.GetServerNo());
	v_FT.Write( rData.GetPrivateID().c_str(), rData.GetPrivateID().Length(), TRUE );
	v_FT.Write( rData.GetEncodePW().c_str(), rData.GetEncodePW().Length(), TRUE );
	v_FT.Write( iIP );
	v_VT.push_back( GetValueType(vLONG, sizeof(LONG) ) );				//Return;

	int iServerPort = rData.GetServerPort();
	CQueryData query_data;
	query_data.SetReturnData( rData.GetServerIP().c_str(), IP_NUM_PLUS_ONE );
	query_data.SetReturnData( &iServerPort, sizeof(INT) );
	query_data.SetReturnData( rData.GetBillingGUID().c_str(), USER_GUID_NUM_PLUS_ONE );
	query_data.SetReturnData( rData.GetPrivateID().c_str(), ID_NUM_PLUS_ONE );
	query_data.SetData( 
		++m_iDBAgentThreadID, 
		_RESULT_CHECK, 
		DBAGENT_PHILLIPPINE_LOGIN, 
		_UPDATEDB,
		queryId, 
		v_FT, 
		v_VT );

	LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetServerIP().c_str(), rData.GetEncodePW().c_str(), rData.GetEncodePW().Length());
	SP2Packet kPacket( DTPK_QUERY );
	if( kPacket.Write(query_data) )
	{
		SendMessage( kPacket );
	}
#endif //__OHTG_LOCAL_PHILIPPINE__
}

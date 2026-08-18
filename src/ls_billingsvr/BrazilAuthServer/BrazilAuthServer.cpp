#include "../stdafx.h"
#include "../MainProcess.h"
#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../Network/ioPacketQueue.h"
#include "./BrazilAuthServer.h"
#include "../Util/Ringbuffer.h"
#include "../channeling/iochannelingnodemanager.h"
#include "../channeling/iochannelingnodeWemadeBuy.h"
#include "../database/logdbclient.h"
#include "../Local/ioLocalManager.h"
#include "../Local/ioLocalParent.h"

extern CLog LOG;


BrazilAuthServer *BrazilAuthServer::sg_Instance = NULL;
BrazilAuthServer::BrazilAuthServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	InitData();
}

BrazilAuthServer::~BrazilAuthServer()
{	
}

BrazilAuthServer &BrazilAuthServer::GetInstance()
{
	if(sg_Instance == NULL)
	{
		ioINILoader kLoader( "ls_config_billingsvr.ini" );
		kLoader.SetTitle( "BrazilBillingServer Buffer" );
		int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
		int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
		sg_Instance = new BrazilAuthServer( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize );
	}
	return *sg_Instance;
}

void BrazilAuthServer::ReleaseInstance()
{		
	SAFEDELETE( sg_Instance );
}

bool BrazilAuthServer::ConnectTo( bool bStart )
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "NETWORK" );

	char szServerIP[MAX_PATH]="";
	kLoader.LoadString( "BrazilAuthServerIP", "", szServerIP, MAX_PATH );

	int iSSPort = kLoader.LoadInt( "BrazilAuthServerPORT", 9000 );

	
	if (!startClient(szServerIP, iSSPort))  // (5) 접속 (시작시 단 1회만 호출)
	{		
		return -1;
	}
	printf("OnConnect (IP:%s PORT:%d RESULT:%d)", szServerIP, iSSPort);
	//LOG.PrintTimeAndLog( 0, "OnConnect (IP:%s PORT:%d )", szServerIP, iSSPort );
	return true;
}

void BrazilAuthServer::InitData()
{
	m_dwCurrentTime = 0;
	m_bSendAlive    = false;
	SetActive(false);
}

void BrazilAuthServer::OnCreate()
{
	InitData();
	m_dwCurrentTime = TIMEGETTIME();
}

void BrazilAuthServer::OnDestroy()
{
	CConnectNode::OnDestroy();
	
	endClient();	//GAuthClient 종료

	LOG.PrintTimeAndLog( 0, "Disconnect BrazilBillingServer!" );
}

bool BrazilAuthServer::CheckNS( CPacket &rkPacket )
{
	return true;             //네트웍 감시 필요없다.
}

int BrazilAuthServer::GetConnectType()
{
	return CONNECT_TYPE_BRAZIL_AUTH_SERVER;
}

//Alive Check
void BrazilAuthServer::ProcessTime()
{	
	if( g_App.IsWantExit() ) return;

}

void BrazilAuthServer::SessionClose(BOOL safely)
{

	LOG.PrintTimeAndLog( 0, "BrazilAuthServer::SessionClose"  );
	CPacket packet(BTBRAUTH_CLOSE);
	ReceivePacket( packet );
}

void BrazilAuthServer::OnClose( SP2Packet &rkPacket )
{
	LOG.PrintTimeAndLog( 0, "BrazilAuthServer::OnClose"  );
	OnDestroy();
}

void BrazilAuthServer::PacketParsing( CPacket &packet )
{
	SP2Packet &kPacket = (SP2Packet&)packet;

	switch( kPacket.GetPacketID() )
	{
	case BTBRAUTH_CLOSE:
		OnClose( kPacket );
		break;
	default:
		LOG.PrintTimeAndLog( 0, "BrazilAuthServer::PacketParsing 알수없는 패킷 : 0x%x", kPacket.GetPacketID() );
		break;
	}
}


bool BrazilAuthServer::OnSendAuth(  char* szToken, DWORD dwKey  )
{
	if( szToken == NULL )
		return false;
	if( !isAlive() )
	{
		
		reConnect();
		LOG.PrintTimeAndLog(0, "%s ReConnect", __FUNCTION__ );
	}
	sendAuthToken( szToken, dwKey );
	return true;
}

VOID BrazilAuthServer::recvTokenResult(UINT _memberId, CHAR* _userId, INT _result, INT _identity)
{
	printf("인증 응답 return : %d, key : %d\n",_result, _identity );
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnReceiveBrazilAuth( _memberId, _userId, _result, _identity );
	
}
VOID BrazilAuthServer::recvKickUser(UINT _memberId, INT _reason)
{

}
VOID BrazilAuthServer::recvLogoutUserResult(UINT _memberId, BOOL _retType)
{

}
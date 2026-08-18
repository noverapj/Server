#include "../stdafx.h"
#include "../MainProcess.h"
#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../Network/ioPacketQueue.h"
#include "./USAuthServer.h"
#include "../Util/Ringbuffer.h"
#include "../channeling/iochannelingnodemanager.h"
#include "../channeling/iochannelingnodeWemadeBuy.h"
#include "../database/logdbclient.h"
#include "../Local/ioLocalManager.h"
#include "../Local/ioLocalParent.h"

extern CLog LOG;


USAuthServer *USAuthServer::sg_Instance = NULL;
USAuthServer::USAuthServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	InitData();
}

USAuthServer::~USAuthServer()
{	
}

USAuthServer &USAuthServer::GetInstance()
{
	if(sg_Instance == NULL)
	{
		ioINILoader kLoader( "ls_config_billingsvr.ini" );
		kLoader.SetTitle( "USBillingServer Buffer" );
		int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
		int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
		sg_Instance = new USAuthServer( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize );
	}
	return *sg_Instance;
}

void USAuthServer::ReleaseInstance()
{		
	SAFEDELETE( sg_Instance );
}

bool USAuthServer::ConnectTo( bool bStart )
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "NETWORK" );

	char szServerIP[MAX_PATH]="";
	kLoader.LoadString( "USAuthServerIP", "", szServerIP, MAX_PATH );

	int iSSPort = kLoader.LoadInt( "USAuthServerPORT", 9000 );

	
	if (!startClient(szServerIP, iSSPort))  // (5) 접속 (시작시 단 1회만 호출)
	{		
		return -1;
	}
	printf("OnConnect (IP:%s PORT:%d RESULT:%d)", szServerIP, iSSPort);
	//LOG.PrintTimeAndLog( 0, "OnConnect (IP:%s PORT:%d )", szServerIP, iSSPort );
	return true;
}

void USAuthServer::InitData()
{
	m_dwCurrentTime = 0;
	m_bSendAlive    = false;
	SetActive(false);
}

void USAuthServer::OnCreate()
{
	InitData();
	m_dwCurrentTime = TIMEGETTIME();
}

void USAuthServer::OnDestroy()
{
	CConnectNode::OnDestroy();
	
	endClient();	//GAuthClient 종료

	LOG.PrintTimeAndLog( 0, "Disconnect USBillingServer!" );
}

bool USAuthServer::CheckNS( CPacket &rkPacket )
{
	return true;             //네트웍 감시 필요없다.
}

int USAuthServer::GetConnectType()
{
	return CONNECT_TYPE_US_AUTH_SERVER;
}

//Alive Check
void USAuthServer::ProcessTime()
{	
	if( g_App.IsWantExit() ) return;

}

void USAuthServer::SessionClose(BOOL safely)
{

	LOG.PrintTimeAndLog( 0, "USAuthServer::SessionClose"  );
	CPacket packet(BTUAUTH_CLOSE);
	ReceivePacket( packet );
}

void USAuthServer::OnClose( SP2Packet &rkPacket )
{
	LOG.PrintTimeAndLog( 0, "USAuthServer::OnClose"  );
	OnDestroy();
}

void USAuthServer::PacketParsing( CPacket &packet )
{
	SP2Packet &kPacket = (SP2Packet&)packet;

	switch( kPacket.GetPacketID() )
	{
	case BTUAUTH_CLOSE:
		OnClose( kPacket );
		break;
	default:
		LOG.PrintTimeAndLog( 0, "USAuthServer::PacketParsing 알수없는 패킷 : 0x%x", kPacket.GetPacketID() );
		break;
	}
}


bool USAuthServer::OnSendAuth(  char* szToken, DWORD dwKey  )
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

VOID USAuthServer::recvTokenResult(UINT _memberId, CHAR* _userId, INT _result, INT _identity)
{
	printf("인증 응답 return : %d, key : %d\n",_result, _identity );
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnReceiveUSAuth( _memberId, _userId, _result, _identity );
	
}
VOID USAuthServer::recvKickUser(UINT _memberId, INT _reason)
{

}
VOID USAuthServer::recvLogoutUserResult(UINT _memberId, BOOL _retType)
{

}
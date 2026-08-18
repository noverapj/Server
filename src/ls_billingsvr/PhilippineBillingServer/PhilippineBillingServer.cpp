#include "../stdafx.h"
#include "../MainProcess.h"
#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../Network/ioPacketQueue.h"
#include "./PhilippineBillingServer.h"
#include "../Util/Ringbuffer.h"
#include "../channeling/iochannelingnodemanager.h"
#include "../channeling/iochannelingnodeWemadeBuy.h"
#include "../database/logdbclient.h"
#include "../Local/ioLocalManager.h"
#include "../Local/ioLocalParent.h"

extern CLog LOG;


PhilippineBillingServer *PhilippineBillingServer::sg_Instance = NULL;
PhilippineBillingServer::PhilippineBillingServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	InitData();
}

PhilippineBillingServer::~PhilippineBillingServer()
{	
}

PhilippineBillingServer &PhilippineBillingServer::GetInstance()
{
	if(sg_Instance == NULL)
	{
		ioINILoader kLoader( "ls_config_billingsvr.ini" );
		kLoader.SetTitle( "PhilippineBillingServer Buffer" );
		int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
		int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
		sg_Instance = new PhilippineBillingServer( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize );
	}
	return *sg_Instance;
}

void PhilippineBillingServer::ReleaseInstance()
{		
	SAFEDELETE( sg_Instance );
}

bool PhilippineBillingServer::ConnectTo( bool bStart )
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "NETWORK" );

	char szServerIP[MAX_PATH]="";
	kLoader.LoadString( "PhilippineBillingServerIP", "", m_szServerIP, MAX_PATH );

	m_iPHLPort = kLoader.LoadInt( "PhilippineBillingServerPORT", 9000 );


	if(m_iPHLPort == -1)
	{
		LOG.PrintTimeAndLog(0,"Error %s Port Is NULL ",__FUNCTION__);
		return false;
	}

	SOCKET socket = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( socket == INVALID_SOCKET )
	{
		//LOG.PrintTimeAndLog( 0, "%s fail socket %d[%s:%d]", __FUNCTION__, GetLastError(), m_szServerIP, m_iPHLPort );
		return false;
	}
	sockaddr_in serv_addr;
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr( m_szServerIP );
	serv_addr.sin_port			= htons( m_iPHLPort );

	int retval = ::connect( socket, (sockaddr*)&serv_addr, sizeof(serv_addr) );
	if( retval != 0 ) 
	{
		DWORD dwError = GetLastError();
		if( dwError != WSAEWOULDBLOCK )
		{
			LOG.PrintTimeAndLog( 0, "%s fail connect Errcode(%d)[%s:%d]", __FUNCTION__,dwError, m_szServerIP, m_iPHLPort );
			bool reuse = true;
			::setsockopt(GetSocket(),SOL_SOCKET,SO_REUSEADDR,(TCHAR*)&reuse,sizeof(reuse));
			closesocket(socket);
			return false;
		}
	}
	// block
	CConnectNode::SetSocket( socket );

	if( m_bDisconn )
	{
		OnCreate();
		AfterCreate();
	}
	m_bConnect = true;
	m_bDisconn = false;
	return true;
	
}

void PhilippineBillingServer::InitData()
{
	m_dwCurrentTime = 0;
	m_bPHLSendAlive    = false;
	SetActive(false);
	m_iPHLPort = 0;
	m_bConnect        = false;
	m_bDisconn = false;
}

bool PhilippineBillingServer::AfterCreate()
{
	bool state = true;

	g_iocp.AddHandleToIOCP( (HANDLE)GetSocket(), (DWORD)this );
	
	LOG.PrintTimeAndLog( 0, "PhilippineBillingServer::AfterCreate (IP:%s PORT:%d RESULT:%d)", m_szServerIP, m_iPHLPort, 0 );

	state = CConnectNode::AfterCreate();

	m_bDisconn = false;

	return state;
}

bool PhilippineBillingServer::SendInitPacket( bool bFirst )
{
	
	m_bDisconn = false;

	return true;
}

void PhilippineBillingServer::OnCreate()
{
	//InitData();
	
	CConnectNode::OnCreate();

	m_dwCurrentTime = TIMEGETTIME();
}

void PhilippineBillingServer::OnDestroy()
{
	m_bDisconn = true;
	CConnectNode::OnDestroy();
	LOG.PrintTimeAndLog( 0, "Disconnect PhilippineBillingServer!" );
}

bool PhilippineBillingServer::SendMessage( CPacket &rkPacket )
{
	if( m_socket == INVALID_SOCKET ) return false;

	CPacket SendPacket( rkPacket.GetBuffer(), rkPacket.GetBufferSize() );
	return CConnectNode::SendMessage( SendPacket.GetBuffer() + SendPacket.GetCurPos(), SendPacket.GetBufferSize() - SendPacket.GetCurPos(), TRUE );
}

bool PhilippineBillingServer::CheckNS( CPacket &rkPacket )
{
	return true;             //네트웍 감시 필요없다.
}

int PhilippineBillingServer::GetConnectType()
{
	return CONNECT_TYPE_PHL_BILLING_SERVER;
}

//Alive Check

void PhilippineBillingServer::ProcessTime()
{	
	if( g_App.IsWantExit() ) return;

	if( TIMEGETTIME() - m_dwCurrentTime > UPDATE_TIME )       
	{
//		if( !IsActive() )
//		{
		if( m_bConnect && m_bDisconn )
		{
			ConnectTo( false );
			LOG.PrintTimeAndLog( 0, "%s Connect", __FUNCTION__ );
		}
		//}
		else
		{	
			GTX_PHL_PK_HEALTH_CHECK kInfo;
			SP2Packet kPacket( BTPHBTPK_ALIVE_REQUEST );
//			kInfo.Htonl();
			kPacket << kInfo;
			SendMessage( kPacket );
//			LOG.PrintTimeAndLog( 0, "%s Send ALIVE Check", __FUNCTION__ );
			if( m_bPHLSendAlive )
			{
				LOG.PrintTimeAndLog( 0, "%s PhilippineBillingServer No Answer.", __FUNCTION__  );
				g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_NO_ANSWER,"Billing server no answer." );
				//OnDestroy();
				m_bPHLSendAlive = false; // 초기화
			}
			else
			{
				m_bPHLSendAlive = true;
			}
		}
		m_dwCurrentTime = TIMEGETTIME();
	}
}
		
	


void PhilippineBillingServer::SessionClose(BOOL safely)
{
	LOG.PrintTimeAndLog( 0, "PhilippineBillingServer::SessionClose"  );
	m_bDisconn = true;
	CPacket packet(BTPHBTPK_CLOSE);
	ReceivePacket( packet );

}

void PhilippineBillingServer::DispatchReceive(CPacket& packet, DWORD bytesTransferred)
{
	m_recvIO.AddBytesTransferred( bytesTransferred );

	int loopCount = 0;

	while( m_recvIO.GetBytesTransferred() > 0 )
	{
		if( m_recvIO.GetBytesTransferred() < 4 ) // 2: Length, 2:Type
		{
			break;
		}

		WORD  wPacketSize  = 0;
		WORD  wType        = 0;
		DWORD dwPacketID   = 0;

		m_recvIO.GetBuffer( &wPacketSize, sizeof(WORD), 0); //kyg int형으로 오는거같은데 왜 기존코드에선 word로 받았을까 고민 
		m_recvIO.GetBuffer( &wType, sizeof(WORD), 2);

//		wPacketSize = ntohs( wPacketSize );
//		wType       = ntohs( wType );

		enum { ALIVE = 90, BALANCE = 10, BUY = 20, CANCEL = 21, };
		if( wType == ALIVE )
		{
			dwPacketID = BTPHBTPK_ALIVE_RESULT; 
		}
		else if( wType == BALANCE )
		{
			dwPacketID = BTPHBTPK_BALANCE_RESULT;
		}
		else if( wType == BUY )
		{
			dwPacketID = BTPHBTPK_BUY_RESULT;
		}
		else if( wType == CANCEL )
		{
			dwPacketID = BTPHBTPK_CANCEL_RESULT;
		}
		else
		{
			LOG.PrintTimeAndLog( 0, "%s Error Code : %d", __FUNCTION__, wType );
			break;
		}

		SP2Packet RecvPacket( dwPacketID );
		RecvPacket.SetDataAdd( (char*)m_recvIO.GetBuffer(), min( wPacketSize, m_recvIO.GetBytesTransferred() ), true );
		//

		if( (RecvPacket.IsValidPacket() == true) && (m_recvIO.GetBytesTransferred() >= wPacketSize) )
		{
			if( !CheckNS( RecvPacket ) ) return;

			ReceivePacket( RecvPacket );

			m_recvIO.AfterReceive( wPacketSize );
		}
		else 
			break;

		if(loopCount > 200)
		{
			LOG.PrintTimeAndLog(0,"Error %s LoopCountOver(%d:%d)",wPacketSize,bytesTransferred);
			m_recvIO.AfterReceive(bytesTransferred);
			break;
		}
	}

	WaitForPacketReceive();
}

void PhilippineBillingServer::OnClose( SP2Packet &rkPacket )
{
	OnDestroy();
	LOG.PrintTimeAndLog( 0, "Disconnect PhilippineBillingServer Server!" );
}

void PhilippineBillingServer::ReceivePacket( CPacket &packet )
{
	g_RecvQueue.InsertQueue( (DWORD)this, packet, PK_QUEUE_SESSION );
}

void PhilippineBillingServer::PacketParsing( CPacket &packet )
{
	SP2Packet &kPacket = (SP2Packet&)packet;

	switch( kPacket.GetPacketID() )
	{
	case BTPHBTPK_ALIVE_RESULT:
		OnAlive( kPacket );
		break;
	case BTPHBTPK_BALANCE_RESULT:
		OnBalance( kPacket );
		break;
	case BTPHBTPK_BUY_RESULT:
		OnBuy( kPacket );
		break;
	case BTPHBTPK_CANCEL_RESULT:
		OnCancel( kPacket );
		break;
	case BTPHBTPK_CLOSE:
		OnClose( kPacket );
		break;
	default:
		LOG.PrintTimeAndLog( 0, "PhilippineBillingServer::PacketParsing 알수없는 패킷 : 0x%x", kPacket.GetPacketID() );
		break;
	}
}

#if 1
void PhilippineBillingServer::OnAlive( SP2Packet &rkPacket )
{
	GTX_PHL_PK_HEALTH_CHECK kResult;
	rkPacket >> kResult;
//	kResult.Ntohl();
	m_bPHLSendAlive = false; // toggle로 보낸 send 값을 정상 처리함.
	//LOG.PrintTimeAndLog( 0, "%s:%d:%d", __FUNCTION__, kResult.ReqType, kResult.RetCode );
}

void PhilippineBillingServer::OnBalance( SP2Packet &rkPacket )
{
	GTX_PHL_PK_GETBALANCE kResult;
	rkPacket >> kResult;
//	kResult.Ntohl();
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnRecieveGetCash( kResult );
}

void PhilippineBillingServer::OnBuy( SP2Packet &rkPacket )
{
	GTX_PHL_PK_PURCHASEITEM kResult;
	rkPacket >> kResult;
//	kResult.Ntohl();
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnRecieveOutputCash( kResult );
}

void PhilippineBillingServer::OnCancel( SP2Packet &rkPacket )
{
	GTX_PHL_PK_CNLPURCHASE kResult;
	rkPacket >> kResult;
//	kResult.Ntohl();
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnRecieveCancelCash( kResult );
	//이미 유저가 없는 상태이므로 유저에게 알릴 필요 없음
	LOG.PrintTimeAndLog( 0, "%s Receive PHLUID:%d, PHLID:%s,chargeNO:%s,RealCash:%d, BonusCash:%d, CanceledCash:%d, returnCode:%d,retMsg:%s", 
		__FUNCTION__, kResult.UserNo, kResult.UserID, kResult.ChargeNo, kResult.RealCash, kResult.BonusCash, kResult.CanceledCashAmt, kResult.RetCode, kResult.RetMsg );
}
#endif

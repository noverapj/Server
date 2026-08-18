#include "../stdafx.h"
#include "../MainProcess.h"
#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../Network/ioPacketQueue.h"
#include "./BrazilBillingServer.h"
#include "../Util/Ringbuffer.h"
#include "../channeling/iochannelingnodemanager.h"
#include "../channeling/iochannelingnodeWemadeBuy.h"
#include "../database/logdbclient.h"
#include "../Local/ioLocalManager.h"
#include "../Local/ioLocalParent.h"

extern CLog LOG;


BrazilBillingServer *BrazilBillingServer::sg_Instance = NULL;
BrazilBillingServer::BrazilBillingServer( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	InitData();
}

BrazilBillingServer::~BrazilBillingServer()
{	
}

BrazilBillingServer &BrazilBillingServer::GetInstance()
{
	if(sg_Instance == NULL)
	{
		ioINILoader kLoader( "ls_config_billingsvr.ini" );
		kLoader.SetTitle( "BrazilBillingServer Buffer" );
		int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
		int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
		sg_Instance = new BrazilBillingServer( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize );
	}
	return *sg_Instance;
}

void BrazilBillingServer::ReleaseInstance()
{		
	SAFEDELETE( sg_Instance );
}

bool BrazilBillingServer::ConnectTo( bool bStart )
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "NETWORK" );

	char szServerIP[MAX_PATH]="";
	kLoader.LoadString( "BrazilBillingServerIP", "", m_szServerIP, MAX_PATH );

	m_iBRAZILPort = kLoader.LoadInt( "BrazilBillingServerPORT", 9000 );


	if(m_iBRAZILPort == -1)
	{
		LOG.PrintTimeAndLog(0,"Error %s Port Is NULL ",__FUNCTION__);
		return false;
	}

	SOCKET socket = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( socket == INVALID_SOCKET )
	{
		//LOG.PrintTimeAndLog( 0, "%s fail socket %d[%s:%d]", __FUNCTION__, GetLastError(), m_szServerIP, m_iBRAZILPort );
		return false;
	}
	sockaddr_in serv_addr;
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr( m_szServerIP );
	serv_addr.sin_port			= htons( m_iBRAZILPort );

	int retval = ::connect( socket, (sockaddr*)&serv_addr, sizeof(serv_addr) );
	if( retval != 0 ) 
	{
		DWORD dwError = GetLastError();
		if( dwError != WSAEWOULDBLOCK )
		{
			LOG.PrintTimeAndLog( 0, "%s fail connect Errcode(%d)[%s:%d]", __FUNCTION__,dwError, m_szServerIP, m_iBRAZILPort );
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

void BrazilBillingServer::InitData()
{
	m_dwCurrentTime = 0;
	m_bBRAZILSendAlive    = false;
	SetActive(false);
	m_iBRAZILPort = 0;
	m_bConnect        = false;
	m_bDisconn = false;
}

bool BrazilBillingServer::AfterCreate()
{
	bool state = true;

	g_iocp.AddHandleToIOCP( (HANDLE)GetSocket(), (DWORD)this );
	
	LOG.PrintTimeAndLog( 0, "BrazilBillingServer::AfterCreate (IP:%s PORT:%d RESULT:%d)", m_szServerIP, m_iBRAZILPort, 0 );

	state = CConnectNode::AfterCreate();

	m_bDisconn = false;

	return state;
}

bool BrazilBillingServer::SendInitPacket( bool bFirst )
{
	
	m_bDisconn = false;

	return true;
}

void BrazilBillingServer::OnCreate()
{
	//InitData();
	
	CConnectNode::OnCreate();

	m_dwCurrentTime = TIMEGETTIME();
}

void BrazilBillingServer::OnDestroy()
{
	m_bDisconn = true;
	CConnectNode::OnDestroy();
	LOG.PrintTimeAndLog( 0, "Disconnect BrazilBillingServer!" );
}

bool BrazilBillingServer::SendMessage( CPacket &rkPacket )
{
	if( m_socket == INVALID_SOCKET ) return false;

	CPacket SendPacket( rkPacket.GetBuffer(), rkPacket.GetBufferSize() );
	return CConnectNode::SendMessage( SendPacket.GetBuffer() + SendPacket.GetCurPos(), SendPacket.GetBufferSize() - SendPacket.GetCurPos(), TRUE );
}

bool BrazilBillingServer::CheckNS( CPacket &rkPacket )
{
	return true;             //네트웍 감시 필요없다.
}

int BrazilBillingServer::GetConnectType()
{
	return CONNECT_TYPE_BRAZIL_BILLING_SERVER;
}

//Alive Check

void BrazilBillingServer::ProcessTime()
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
			GTX_BRAZIL_PK_HEALTH_CHECK kInfo;
			SP2Packet kPacket( BTBRBTPK_ALIVE_REQUEST );
			kInfo.Htonl();
			kPacket << kInfo;
			
			SendMessage( kPacket );
			LOG.PrintTimeAndLog( 0, "%s Send ALIVE Check", __FUNCTION__ );
			if( m_bBRAZILSendAlive )
			{
				LOG.PrintTimeAndLog( 0, "%s BrazilBillingServer No Answer.", __FUNCTION__  );
				g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_NO_ANSWER,"Billing server no answer." );
				//OnDestroy();
				m_bBRAZILSendAlive = false; // 초기화
			}
			else
			{
				m_bBRAZILSendAlive = true;
			}
		}
		m_dwCurrentTime = TIMEGETTIME();
	}
}
		
	


void BrazilBillingServer::SessionClose(BOOL safely)
{
	LOG.PrintTimeAndLog( 0, "BrazilBillingServer::SessionClose"  );
	m_bDisconn = true;
	CPacket packet(BTBRBTPK_CLOSE);
	ReceivePacket( packet );

}

void BrazilBillingServer::DispatchReceive(CPacket& packet, DWORD bytesTransferred)
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

		wPacketSize = ntohs( wPacketSize );
		wType       = ntohs( wType );

		enum { ALIVE = 40, BALANCE = 10, BUY = 20, CANCEL = 21, };
		if( wType == ALIVE )
		{
			dwPacketID = BTBRBTPK_ALIVE_RESULT; 
		}
		else if( wType == BALANCE )
		{
			dwPacketID = BTBRBTPK_BALANCE_RESULT;
		}
		else if( wType == BUY )
		{
			dwPacketID = BTBRBTPK_BUY_RESULT;
		}
		else if( wType == CANCEL )
		{
			dwPacketID = BTBRBTPK_CANCEL_RESULT;
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

void BrazilBillingServer::OnClose( SP2Packet &rkPacket )
{
	OnDestroy();
	LOG.PrintTimeAndLog( 0, "Disconnect BrazilBillingServer Server!" );
}

void BrazilBillingServer::ReceivePacket( CPacket &packet )
{
	g_RecvQueue.InsertQueue( (DWORD)this, packet, PK_QUEUE_SESSION );
}

void BrazilBillingServer::PacketParsing( CPacket &packet )
{
	SP2Packet &kPacket = (SP2Packet&)packet;

	switch( kPacket.GetPacketID() )
	{
	case BTBRBTPK_ALIVE_RESULT:
		OnAlive( kPacket );
		break;
	case BTBRBTPK_BALANCE_RESULT:
		OnBalance( kPacket );
		break;
	case BTBRBTPK_BUY_RESULT:
		OnBuy( kPacket );
		break;
	case BTBRBTPK_CANCEL_RESULT:
		OnCancel( kPacket );
		break;
	case BTBRBTPK_CLOSE:
		OnClose( kPacket );
		break;
	default:
		LOG.PrintTimeAndLog( 0, "BrazilBillingServer::PacketParsing 알수없는 패킷 : 0x%x", kPacket.GetPacketID() );
		break;
	}
}

#if 1
void BrazilBillingServer::OnAlive( SP2Packet &rkPacket )
{
	GTX_BRAZIL_PK_HEALTH_CHECK kResult;
	rkPacket >> kResult;
	kResult.Ntohl();
	m_bBRAZILSendAlive = false; // toggle로 보낸 send 값을 정상 처리함.
	//LOG.PrintTimeAndLog( 0, "%s:%d:%d", __FUNCTION__, kResult.ReqType, kResult.RetCode );
}

void BrazilBillingServer::OnBalance( SP2Packet &rkPacket )
{
	GTX_BRAZIL_PK_GETBALANCE kResult;
	rkPacket >> kResult;
	kResult.Ntohl();
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnRecieveGetCash( kResult );
}

void BrazilBillingServer::OnBuy( SP2Packet &rkPacket )
{
	GTX_BRAZIL_PK_PURCHASEITEM kResult;
	rkPacket >> kResult;
	kResult.Ntohl();
	ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
	if( pLocal )
		pLocal->OnRecieveOutputCash( kResult );
}

void BrazilBillingServer::OnCancel( SP2Packet &rkPacket )
{
	GTX_BRAZIL_PK_CNLPURCHASE kResult;
	rkPacket >> kResult;
	kResult.Ntohl();
	//이미 유저가 없는 상태이므로 유저에게 알릴 필요 없음
	LOG.PrintTimeAndLog( 0, "%s Receive BRAZILUID:%d, BRAZILID:%s,chargeNO:%s,RealCash:%d, BonusCash:%d, CanceledCash:%d, returnCode:%d,retMsg:%s", 
		__FUNCTION__, kResult.UserNo, kResult.UserID, kResult.ChargeNo, kResult.RealCash, kResult.BonusCash, kResult.CanceledCashAmt, kResult.RetCode, kResult.RetMsg );
}
#endif

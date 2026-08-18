#include "../stdafx.h"

#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../MainProcess.h"
#include "ServerNodeManager.h"
#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"

extern CLog LOG;

ServerNodeManager *ServerNodeManager::sg_Instance = NULL;

ServerNodeManager::ServerNodeManager() : m_dwCurrentTime(0)
{
	m_iDestroyCount = 0;
}

ServerNodeManager::~ServerNodeManager()
{
	m_vServerNode.clear();
}

ServerNodeManager &ServerNodeManager::GetInstance()
{
	if( !sg_Instance )
		sg_Instance = new ServerNodeManager;

	return *sg_Instance;
}

void ServerNodeManager::ReleaseInstance()
{
	SAFEDELETE( sg_Instance );
}

void ServerNodeManager::InitMemoryPool()
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "Server Session Buffer" );
	int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
	int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
	kLoader.SetTitle( "MemoryPool" );
	int iMaxServer = kLoader.LoadInt( "server_pool", 10 );

	m_MemNode.CreatePool(0, iMaxServer, FALSE);
	for(int i = 0;i < iMaxServer * 2; i++) //kyg ¸Æ½º°ª Á¶Á¤ 
	{
		m_MemNode.Push( new ServerNode( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize ) );
	}
}

void ServerNodeManager::ReleaseMemoryPool()
{
	vServerNode_iter iter, iEnd;
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		ServerNode *pServerNode = *iter;
		pServerNode->OnDestroy();
		m_MemNode.Push( pServerNode );
	}	
	m_vServerNode.clear();
	m_MemNode.DestroyPool();
}

ServerNode *ServerNodeManager::CreateServerNode(SOCKET s)
{
	ServerNode *newNode = m_MemNode.Pop(); //kyg pop 
	if( !newNode )
	{
		LOG.PrintTimeAndLog(0,"[error][goods]ServerNodeManager::CreateServerNode MemPool Zero!");
		return NULL;
	}

//	g_iocp.AddHandleToIOCP((HANDLE)s,(DWORD)newNode);

	newNode->SetSocket(s);
	newNode->OnCreate();



	return newNode;
}

void ServerNodeManager::AddServerNode( ServerNode *pNewNode )
{
	m_vServerNode.push_back( pNewNode );
}

void ServerNodeManager::RemoveNode( ServerNode *pServerNode )
{
	vServerNode_iter iter = std::find( m_vServerNode.begin(), m_vServerNode.end(), pServerNode );
	if( iter != m_vServerNode.end() )
	{
		ServerNode *pServerNode = *iter;
		 
		m_vServerNode.erase( iter );
		m_MemNode.Push( pServerNode );
		m_iDestroyCount ++;
	}	

}

void ServerNodeManager::ProcessServerNode()
{
	if( m_dwCurrentTime == 0 )
		m_dwCurrentTime = TIMEGETTIME();

	if( TIMEGETTIME() - m_dwCurrentTime < 60000 ) return;

	m_dwCurrentTime = TIMEGETTIME();
}

void ServerNodeManager::ServerNode_SendBufferFlush()
{
	if( m_vServerNode.empty() == false )
	{
		vector< ServerNode* >::iterator	iter	= m_vServerNode.begin();
		vector< ServerNode* >::iterator	iterEnd	= m_vServerNode.end();

		for( iter ; iter != iterEnd ; ++iter )
		{
			ServerNode* pServerNode = (*iter);

			if( ! pServerNode->IsActive() )
				continue;
			if( pServerNode->GetSocket() == INVALID_SOCKET )
				continue;

			pServerNode->FlushSendBuffer();
		}
	}
}

bool ServerNodeManager::SendMessageIP( ioHashString &rszIP, int iClientPort, SP2Packet &rkPacket )
{
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		if( item->GetIP() == rszIP && item->GetClientPort() == iClientPort )
		{
			bool rtVal = false;

			rtVal = item->SendMessage( rkPacket );

			if(rtVal == false)
			{
				LOG.PrintTimeAndLog(0,"SendMessageIP Error(%s:%d) PacketID(0x%x) lassError %d",rszIP.c_str(),iClientPort,rkPacket.GetPacketID(),GetLastError());
			}

			return rtVal;
		}		
	}

	LOG.PrintTimeAndLog(0,"SendMessageIP ServerNode Not Fund(%s:%d) PacketID(0x%x) ",rszIP.c_str(),iClientPort,rkPacket.GetPacketID());

	return false;
}

void ServerNodeManager::SendLoginInfo()
{
	for(int i=0; i< (int)m_vServerNode.size(); ++i)
	{
		ServerNode* node = reinterpret_cast<ServerNode*>(m_vServerNode[i]);
		if(node)
			node->SendRequestLoginInfo();
	}
}

#ifdef __OHTG_LOCAL_PHILIPPINE__
bool ServerNodeManager::GlobalQueryParse( SP2Packet &rkPacket )
{
	CQueryResultData query_data;
	rkPacket >> query_data;

	switch(query_data.GetMsgType())
	{
	case DBAGENT_GAME_PINGPONG:
		OnResultPingPong( &query_data );
		return true;
	case DBAGENT_PHILLIPPINE_AUTOLOGIN:
		OnResultPhillippineAutoLogin( &query_data );
		return true;
	case DBAGENT_PHILLIPPINE_LOGIN:
		OnResultPhillippineLogin( &query_data );
		return true;
	}

	return false;
}


void ServerNodeManager::OnResultPingPong( CQueryResultData *query_data )
{
	DWORD dwLastPing;
	query_data->GetValue( dwLastPing, sizeof(DWORD) );

	DWORD dwElapse = (TIMEGETTIME() - dwLastPing) / 2;
}


void ServerNodeManager::OnResultPhillippineAutoLogin( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTradeInfoList Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	char szServerIP[IP_NUM_PLUS_ONE]="";
	char szBillingGUID[USER_GUID_NUM_PLUS_ONE]="";
	char szPrivateID[USER_GUID_NUM_PLUS_ONE]="";
	char szAccountIndex[USER_GUID_NUM_PLUS_ONE]="";
	
	int iClientPort=0;
	int iAccountIndex = 0;
	PACKET_GUARD_VOID( query_data->GetValue( szServerIP, IP_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( iClientPort, sizeof(int) ) );
	PACKET_GUARD_VOID( query_data->GetValue( szBillingGUID, USER_GUID_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( szPrivateID, ID_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( iAccountIndex, sizeof(int) ) );

	sprintf_s(szAccountIndex,sizeof(szAccountIndex),"%d",iAccountIndex);
	
	int iReturnValue = BILLING_LOGIN_RESULT_SUCCESS;
	SP2Packet kPacket( BSTPK_AUTOUPGRADE_LOGIN_RESULT );
	kPacket << szPrivateID;
	if(iAccountIndex == 0)
	{
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		kPacket << false;
	}
	else
	{
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szAccountIndex;
		kPacket << szPrivateID;
	}

	g_ServerNodeManager.SendMessageIP( (ioHashString) szServerIP,  iClientPort, kPacket );
	LOG.PrintTimeAndLog(0, "Test %s pServerNode == NULL:%s:%s:%s:%s:%d", __FUNCTION__, szBillingGUID, szPrivateID, szServerIP, szAccountIndex, iClientPort );
	return;
}


void ServerNodeManager::OnResultPhillippineLogin( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTradeInfoList Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	char szServerIP[IP_NUM_PLUS_ONE]="";
	char szBillingGUID[USER_GUID_NUM_PLUS_ONE]="";
	char szPrivateID[ID_NUM_PLUS_ONE]="";
	char szAccountIndex[ID_NUM_PLUS_ONE]="";
	
	int iClientPort=0;
	int iAccountIndex = 0;
	PACKET_GUARD_VOID( query_data->GetValue( szServerIP, IP_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( iClientPort, sizeof(int) ) );
	PACKET_GUARD_VOID( query_data->GetValue( szBillingGUID, USER_GUID_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( szPrivateID, ID_NUM_PLUS_ONE ) );
	PACKET_GUARD_VOID( query_data->GetValue( iAccountIndex, sizeof(int) ) );

	sprintf_s(szAccountIndex,sizeof(szAccountIndex),"%d",iAccountIndex);

	SP2Packet kPacket( BSTPK_LOGIN_RESULT );
	kPacket << szPrivateID;
	if(iAccountIndex == 0)
	{
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		kPacket << false;
	}
	else
	{
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szAccountIndex;
		kPacket << szPrivateID;
	}

	g_ServerNodeManager.SendMessageIP( (ioHashString) szServerIP,  iClientPort, kPacket );

	LOG.PrintTimeAndLog(0, "Test %s Complete.MsgType:%s:%s:%s", __FUNCTION__, szBillingGUID, szPrivateID, szAccountIndex);

	return;
}

#endif //__OHTG_LOCAL_PHILIPPINE__

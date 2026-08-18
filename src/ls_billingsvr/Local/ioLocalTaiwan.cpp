#include "../stdafx.h"
#include "./iolocaltaiwan.h"
#include "../DataBase/LogDBClient.h"
#include "../MainProcess.h"
#include "../NodeInfo/ServerNode.h"
#include "../Channeling/ioChannelingNodeParent.h"
#include "../ThreadPool/ioThreadPool.h"
#include "../NodeInfo/ServerNodeManager.h"
#include "../EtcHelpFunc.h"
#include "../xml/ioxmldocument.h"
#include "../xml/ioxmlelement.h"
#include "../Util/cJSON.h"
#include "LS_RestAPI/ioRestAPI.h"
#include "../Channeling\ioChannelingNodeManager.h"
#include "../Channeling\ioChannelingNodeParent.h"


#define VALUFE_REUTRN_SUCCESS 1

extern CLog LOG;


ioLocalTaiwan::ioLocalTaiwan(void)
{
	char szTemp[MAX_PATH]="";
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle("TAIWAN");

	kLoader.LoadString( "GamonKey", "", szTemp, sizeof( szTemp ) );
	m_sKey = szTemp;

	kLoader.LoadString( "ValofeServiceCode", "", szTemp, sizeof( szTemp ) );
	m_sServiceCode = szTemp;

	kLoader.LoadString( "GamonLoginURL", "", szTemp, sizeof( szTemp ) );
	m_sLoginURL = szTemp;

	kLoader.LoadString( "ValofeGetURL", "", szTemp, sizeof( szTemp ) );
	m_sGetURL = szTemp;

	kLoader.LoadString( "ValofeProductListURL", "", szTemp, sizeof( szTemp ) );
	m_sProductListURL = szTemp;
	
	kLoader.LoadString( "ValofePurchaseItemURL", "", szTemp, sizeof( szTemp ) );
	m_sPurchaseItemURL = szTemp;

	kLoader.LoadString( "ValofePurchaseGiftURL", "", szTemp, sizeof( szTemp ) );
	m_sPurchaseGiftURL = szTemp;

	m_ProductInfoMap.clear();
}

ioLocalTaiwan::~ioLocalTaiwan(void)
{
}

ioLocalManager::LocalType ioLocalTaiwan::GetType()
{
	return ioLocalManager::LCT_TAIWAN;
}

BOOL ioLocalTaiwan::ProductInit()
{
	int Page_index = 0;
	int row_per_page = 100;
	int MaxPageCount = 20;
	int CurrentCount = 0;
	
	char szReturnData[PRODEUCT_BUFF_SIZE]="";
	ZeroMemory( szReturnData, PRODEUCT_BUFF_SIZE );
	char szPostData[MAX_PATH*2]="";

	int iProductLength = 100;
	while(0 != iProductLength)
	{
		Page_index +=1;
		StringCbPrintf( szPostData, sizeof( szPostData ), "service_code=%s&channeling_type=0&page_index=%d&row_per_page=%d", m_sServiceCode.c_str(), Page_index, row_per_page);

		ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
		if( !Winhttp.GetResultData( m_sProductListURL.c_str(), szPostData, szReturnData, PRODEUCT_BUFF_SIZE ) )
		{
	//		LOG.PrintTimeAndLog(0,"[error][valofe] getcash ioRestAPI Error :%s %s %s %d", szReturnData, szErrString.c_str(), szUserID.c_str() ,GetLastError() );
			return FALSE;
		}
	
		try 
		{

			cJSON *pJson = NULL;
			pJson=cJSON_Parse(szReturnData);

			if( pJson == NULL )
				return FALSE;

			BOOL bState =FALSE;

			for (BYTE i = 0; i < cJSON_GetArraySize(pJson); i++)
			{
				cJSON *pObject = cJSON_GetArrayItem( pJson, i);

				if( pObject && pObject->string )
				{
					if(strcmp("Result", pObject->string) == 0)
					{
						if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
							bState = TRUE;
					}
					else if(strcmp("total_product_count", pObject->string) == 0)
					{
						if(bState == TRUE)
						{
							MaxPageCount = pObject->valueint;
						}
					}
					else if(strcmp("product_array_length", pObject->string) == 0)
					{
						if(bState == TRUE)
						{
							CurrentCount = pObject->valueint;
						}
					}
					else if( strcmp("product_list", pObject->string) == 0 )
					{ 
						int product_no = 0;
						int product_id = 0;
						cJSON* name = NULL;  cJSON* index = NULL;

						iProductLength = cJSON_GetArraySize(pObject);
						for (BYTE J = 0; J < iProductLength; J++)
						{
							cJSON *pChildList = cJSON_GetArrayItem( pObject, J);

							for (BYTE K = 0; K < cJSON_GetArraySize(pChildList); K++)
							{
								cJSON *pChild = cJSON_GetArrayItem( pChildList, K);
								if( pChild && pChild->string )
								{
									if(strcmp("product_no", pChild->string) == 0)
									{
										product_no = pChild->valueint;
									}
									else if(strcmp("product_id", pChild->string) == 0)
									{
										product_id = atoi(pChild->valuestring);
									}
								}
							}

							ProductInfoMap::const_iterator iter = m_ProductInfoMap.find( product_id );
							if( iter != m_ProductInfoMap.end() )
							{
								int iPD = product_id;
							}

							m_ProductInfoMap.insert( ProductInfoMap::value_type( product_id, product_no ) );
							LOG.PrintTimeAndLog( 0, "[Product] ProductID:%d - ProductNo:%d", product_id, product_no);
						}
					}
				}
			}
			cJSON_Delete(pJson);
		}

		catch ( ... )
		{
			LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash Json ParseError %s", szReturnData );
			return FALSE;
		}
	}

	return FALSE;
}


void ioLocalTaiwan::OnAutoupgradeLogin( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType   = 0;
	ioHashString szBillingGUID;
	ioHashString szPrivateID;
	ioHashString sEncodePW;
	ioHashString szPublicIP;
	DWORD        dwReturnMsgType;
	ioHashString szMacAddress;

	rkPacket >> szBillingGUID >> szPrivateID >> sEncodePW >> szPublicIP >> dwReturnMsgType; // 공통
	rkPacket >> szMacAddress >> iChannelingType; // 대만

	if( !pServerNode )
	{
		LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s:%s:%s:%d", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str(), sEncodePW.c_str(), szPublicIP.c_str(), szMacAddress.c_str(), iChannelingType );
		return;
	}

	ioData kData;
	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetPrivateID( szPrivateID );
	kData.SetEncodePW( sEncodePW );
	kData.SetUserIP( szPublicIP );
	kData.SetReturnMsgType( dwReturnMsgType );
	kData.SetUserNo( szMacAddress );
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_AUTOUPGRADELOGIN );
	g_ThreadPool.SetData( kData );
}


void ioLocalTaiwan::OnOTP( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szUserNo;
	ioHashString szPrivateID;
	ioHashString sVaildCode;
	ioHashString szPublicIP;
	DWORD        dwReturnMsgType;
	int          iChannelingType   = 0;

	rkPacket >> szUserNo >> szPrivateID >> sVaildCode >> szPublicIP  >> dwReturnMsgType >> iChannelingType;

	if( !pServerNode )
	{
		LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s", __FUNCTION__, szUserNo.c_str(), sVaildCode.c_str(), szPublicIP.c_str());
		return;
	}
	
	ioData kData;
	kData.SetUserNo(szUserNo);
	kData.SetChannelingType( iChannelingType );
	kData.SetEncodePW( sVaildCode );
	kData.SetUserIP( szPublicIP );
	kData.SetReturnMsgType( dwReturnMsgType );
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_OTP );
	g_ThreadPool.SetData( kData );

}

void ioLocalTaiwan::OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szBillingGUID;
	ioHashString szPrivateID;
	ioHashString sEncodePW;
	ioHashString szPublicIP;
	DWORD        dwReturnMsgType;
	ioHashString szMacAddress;

	rkPacket >> szBillingGUID >> szPrivateID >> sEncodePW >> szPublicIP >> dwReturnMsgType; // 공통
	rkPacket >> szMacAddress; // 대만

	if( !pServerNode )
	{
		LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s:%s:%s", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str(), sEncodePW.c_str(), szPublicIP.c_str(), szMacAddress.c_str() );
		return;
	}

	ioData kData;
	kData.SetBillingGUID( szBillingGUID );
	kData.SetPrivateID( szPrivateID );
	kData.SetEncodePW( sEncodePW );
	kData.SetUserIP( szPublicIP );
	kData.SetReturnMsgType( dwReturnMsgType );
	kData.SetUserNo( szMacAddress );
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_LOGIN );

	SP2Packet kPacket( dwReturnMsgType );
	kPacket << szPrivateID;
	kPacket << szBillingGUID;
	kPacket << BILLING_LOGIN_RESULT_SUCCESS;
	kPacket << szPrivateID;
	g_ServerNodeManager.SendMessageIP( (ioHashString) kData.GetServerIP(),  kData.GetServerPort(), kPacket );
	LOG.PrintTimeAndLog(0, "%s Complete.MsgType:%x:%s:%s", __FUNCTION__, kData.GetReturnMsgType(), kData.GetBillingGUID().c_str(), kData.GetPrivateID().c_str());
}


void ioLocalTaiwan::_OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType   = 0;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex       = 0;
	ioHashString szPrivateID;
	ioHashString szPublicID;
	bool         bSetUserMouse     = false;
	ioHashString szNexonID;
	ioHashString szNexonNo;
	ioData kData;

	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> szPrivateID >> szPublicID >> bSetUserMouse; // 공통사항
	rkPacket >> szNexonID; // For Nexon
	rkPacket >> szNexonNo; // For Nexon


	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPrivateID( szPrivateID );
	kData.SetPublicID( szPublicID );
	kData.SetSetUserMouse( bSetUserMouse );
	kData.SetUserNo( szNexonID );
	kData.SetNexonUserNo( szNexonNo );

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_GET_CASH );

	g_ThreadPool.SetData( kData );
}

void ioLocalTaiwan::_OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName )
{
	int          iChannelingType = 0;
	ioHashString szBillingGUID;
	int          iReturnItemPrice = 0;
	DWORD        dwUserIndex = 0;
	ioHashString szPublicID;
	ioHashString szPrivateID;
	ioHashString szUserIP;
	int          iPayAmt  = 0;
	int          iType    = 0;
	ioHashString szNexonID;
	ioHashString szNexonNo;
	int			 iBonusCashSize	= 0;
	int			iGameServerPort = 0;

	DWORD        dwReUserIndex = 0;
	ioHashString szRePublicID;
	ioHashString szRePrivateID;

	ioData kData;

	rkPacket >> iChannelingType >> szBillingGUID >> iReturnItemPrice >> dwUserIndex >> szPublicID >> szPrivateID >> szUserIP >> iPayAmt >> iBonusCashSize;

	for( int i = 0; i < iBonusCashSize; i++ )
	{
		int iIndex		= 0;
		int iBonusCash	= 0;

		rkPacket >> iIndex >> iBonusCash;

		kData.AddBonusCashInfoForConsume(iIndex, iBonusCash);
	}

	rkPacket >> iType; // 공통사항
	
	int iItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i < MAX_ITEM_VALUE ; i++)
		iItemValueList[i] = 0;
	ioChannelingNodeParent::GetItemValueList( rkPacket, iType, iItemValueList );
	
	rkPacket >> iGameServerPort; // wemadebuy

	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetExp( iReturnItemPrice );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPublicID( szPublicID );
	kData.SetPrivateID( szPrivateID );
	kData.SetUserIP( szUserIP );
	kData.SetItemPayAmt( iPayAmt );
	kData.SetItemType( iType );
	kData.SetItemValueList( iItemValueList );
	kData.SetUserNo( szNexonID );
	kData.SetGoodsNo( dwGoodsNo );
	kData.SetGoodsName( rszGoodsName );
	kData.SetNexonUserNo( szNexonNo );

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}

	if( iType == OUTPUT_CASH_PRESENT)
	{
		rkPacket >> szRePrivateID; // For Nexon No
		rkPacket >> szRePublicID; // For Nexon No
		rkPacket >> dwReUserIndex; // For Nexon No

		kData.SetServerNo(iGameServerPort);
		kData.SetRecvUserIndex(dwReUserIndex);
		kData.SetReceivePrivateID(szRePrivateID);
		kData.SetReceivePublicID(szRePublicID);
		kData.SetEmpty( false );
		kData.SetProcessType( ioData::PT_PRESENT );
	}
	else
	{
		kData.SetEmpty( false );
		kData.SetProcessType( ioData::PT_OUTPUT_CASH );
	}

	rkPacket >> szNexonNo;
	kData.SetNexonUserNo( szNexonNo );
	g_ThreadPool.SetData( kData );
}

void ioLocalTaiwan::_OnSubscriptionRetract( ServerNode* pServerNode, SP2Packet& rkPacket ) //kyg 이루틴 타는걸로도 테스트 해봐야함 
{
	int          iChannelingType = 0;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex = 0;
	ioHashString szPublicID;
	ioHashString szPrivateID;
	DWORD		 dwIndex = 0;
	ioHashString szChargeNo;
	ioHashString szNexonID;
	ioHashString szNexonNo;
	int cancelGold = 0;

	rkPacket >> iChannelingType;
	rkPacket >> szBillingGUID;
	rkPacket >> dwUserIndex;
	rkPacket >> szPublicID;
	rkPacket >> szPrivateID;
	rkPacket >> dwIndex; 
	//위에까지 공통인자 
	rkPacket >> szChargeNo;
	rkPacket >> szNexonID;
	rkPacket >> szNexonNo;
	rkPacket >> cancelGold;

	LOG.PrintTimeAndLog(0,"ioLocalTaiwan::_OnSubscriptionRetract cancelGold : %s %d",szNexonNo.c_str(), cancelGold);

	ioData kData;

	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPublicID( szPublicID );
	kData.SetPrivateID( szPrivateID );
	kData.SetUserNo( szNexonID );
	kData.SetChargeNo( szChargeNo );
	kData.SetIndex( dwIndex );
	kData.SetItemPayAmt(cancelGold);
	kData.SetNexonUserNo( szNexonNo );

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_SUBSCRIPTION_RETRACT );

	g_ThreadPool.SetData( kData );
}

void ioLocalTaiwan::ThreadAutoUpgradeLogin( const ioData &rData, LoginManager &rLoginMgr )
{
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog(0, "%s LogOut:%s:%s:%s", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetEncodePW().c_str() );
		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		kPacket << false;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}

	if( rData.GetChannelingType() == CNT_HAPPYTUK )
	{
		ioChannelingNodeParent *pNode = g_ChannelingMgr.GetNode( (ChannelingType) rData.GetChannelingType() );
		if( pNode )
		{
			pNode->ThreadAutoUpgradeLogin( rData );
		}
	}
	else
	{
		LOG.PrintTimeAndLog(0, "%s Complete.MsgType:%x:%s:%s", __FUNCTION__, rData.GetReturnMsgType(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str());

		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << rData.GetPrivateID();;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}
}


void ioLocalTaiwan::ThreadOTP( const ioData &rData )
{
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog(0, "%s LogOut:%s:%s:%s", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetEncodePW().c_str() );
		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_OTP_RESULT_FAIL;
		kPacket << false;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}

	if( rData.GetChannelingType() == CNT_HAPPYTUK )
	{
		ioChannelingNodeParent *pNode = g_ChannelingMgr.GetNode( (ChannelingType) rData.GetChannelingType() );
		if( pNode )
		{
			pNode->ThreadAutoUpgradeLogin( rData );
		}
	}
	else
	{
		LOG.PrintTimeAndLog(0, "%s Complete.MsgType:%x:%s:%s", __FUNCTION__, rData.GetReturnMsgType(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str());

		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_OTP_RESULT_SUCCESS;
		kPacket << rData.GetPrivateID();;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}
}


void ioLocalTaiwan::ThreadLogin( const ioData &rData, LoginManager &rLoginMgr )
{
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog(0, "%s LogOut:%s:%s:%s", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetEncodePW().c_str() );
		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		kPacket << false;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}
}

void ioLocalTaiwan::ThreadGetCash( const ioData &rData )
{
	const ioHashString& szNexonNo = rData.GetNexonUserNo();

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] getCash Data is Empty." );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] getCash LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
			  
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioLocalTaiwan::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}
		
		return;
	}

	int iReturnCash    = 0;
	int iPurchasedCash = 0; // 현금을 주고 구매한 캐쉬
	int iResult = 0;

	ioHashString szErrString;

#ifdef _TEST
	iReturnCash = 500;

	SP2Packet testkPacket( BSTPK_GET_CASH_RESULT );
	testkPacket << rData.GetUserIndex();
	testkPacket << rData.GetBillingGUID();
	testkPacket << rData.GetSetUserMouse();
	testkPacket << CASH_RESULT_SUCCESS;
	testkPacket << iReturnCash;
	testkPacket << iPurchasedCash;

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), testkPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeNaver::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		return;
	}
	return;
#endif	

	if(ValofeGetCash(rData, iReturnCash, iPurchasedCash, iResult, szErrString) == FALSE)
	{
		SendExecptMessage(BSTPK_GET_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);

		LOG.PrintTimeAndLog( 0, "[error][nexon] getCash fail: threadid:%d userIndex: %d "
			"BillGUID:%s "
			"PrivateID %s "
			"PublicID  %s "
			"UserNo : %s "
			"NexonUserNo : %s"
			"(%d/%d)"
			"errString :%s",
			GetCurrentThreadId(),rData.GetUserIndex(),
			rData.GetBillingGUID().c_str(),
			rData.GetPrivateID().c_str(),
			rData.GetPublicID().c_str(),
			rData.GetUserNo().c_str(),
			rData.GetNexonUserNo().c_str(),
			iReturnCash, iPurchasedCash,
			szErrString.c_str());
		return;
	}

	SP2Packet kPacket( BSTPK_GET_CASH_RESULT );

	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << rData.GetSetUserMouse();
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << iReturnCash;
	kPacket << iReturnCash;

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s:%s", "ioLocalTaiwan::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetNexonUserNo().c_str() );
	}

	LOG.PrintTimeAndLog( 0, "[info][Taiwan] getCash Success: threadId : %d userIndex : %d "
		"BillGUID : %s "
		"PrivateID : %s "
		"PublicID : %s "
		"UserNo : %s "
		"TaiwanNo : %s "
		"(%d/%d)",
		GetCurrentThreadId(), rData.GetUserIndex(),
		rData.GetBillingGUID().c_str(),
		rData.GetPrivateID().c_str(),
		rData.GetPublicID().c_str(),
		rData.GetUserNo().c_str(),
		rData.GetNexonUserNo().c_str(),
		iReturnCash, iPurchasedCash  );
}

void ioLocalTaiwan::ThreadOutputCash( const ioData &rData )
{
	const ioHashString& szNexonID = rData.GetNexonUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash Data is Empty.");
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetNexonUserNo().c_str() );

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		
		return;
	}

#ifdef _TEST
	SP2Packet testkPacket( BSTPK_OUTPUT_CASH_RESULT );
	testkPacket << rData.GetUserIndex();
	testkPacket << rData.GetBillingGUID();
	testkPacket << CASH_RESULT_SUCCESS;
	testkPacket << rData.GetExp();
	testkPacket << rData.GetItemType();
	testkPacket << rData.GetItemPayAmt();
	testkPacket << 0; // TransactionID ( FOR US ) //kyg 여기에 구매 유니크값 szReturnBuyNO
	testkPacket << "";

	int iiItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i <MAX_ITEM_VALUE; i++)
		iiItemValueList[i] = rData.GetItemValue( i );
	SetItemValueList( testkPacket, rData.GetItemType(), iiItemValueList );
	testkPacket << rData.GetChannelingType(); // 공통
	testkPacket << 0;
	testkPacket << 0;

	static TwoOfINTVec vvInfo;
	vvInfo.clear();

	rData.GetBonusCashInfo(vvInfo);
	int iiBonusCashSize	= vvInfo.size();

	testkPacket << iiBonusCashSize	;

	for( int i = 0; i < iiBonusCashSize; i++ )
	{
		testkPacket << vvInfo[i].value1 << vvInfo[i].value2;
	}

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), testkPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioChannelingNodeNaver::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
	return;
#endif
	if(ValofePurchaseItem(szNexonID, szBuyNo, (ioData&)rData, szErrString, iResult) == FALSE)
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash RestAPI Error: %d:%d:%s:%s:%s:%s:%s:%d:%s",GetCurrentThreadId(), rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetNexonUserNo().c_str(), GetLastError(), szErrString.c_str());

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );

	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << rData.GetExp();
	kPacket << rData.GetItemType();
	kPacket << rData.GetItemPayAmt();
	kPacket << 0; // TransactionID ( FOR US )
	kPacket << szBuyNo; // same szChargeNo

	int iItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i <MAX_ITEM_VALUE; i++)
		iItemValueList[i] = rData.GetItemValue( i );
	ioChannelingNodeParent::SetItemValueList( kPacket, rData.GetItemType(), iItemValueList );

	int iReturnCash = 0;//kyg 채울것
	int iPurchaseCash = 0;

	if( ValofeGetCash(rData, iReturnCash, iPurchaseCash, iResult, szErrString) == FALSE )
		iReturnCash = 0;

	kPacket << rData.GetChannelingType(); // 공통
	kPacket << iReturnCash;
	kPacket << iReturnCash;

	static TwoOfINTVec vInfo;
	vInfo.clear();

	rData.GetBonusCashInfo(vInfo);
	int iBonusCashSize	= vInfo.size();

	kPacket << iBonusCashSize;

	for( int i = 0; i < iBonusCashSize; i++ )
	{
		kPacket << vInfo[i].value1 << vInfo[i].value2;
	}

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioLocalTaiwan::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
}


void ioLocalTaiwan::ThreadPresent( const ioData &rData )
{
	const ioHashString& szNexonID = rData.GetNexonUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash Data is Empty.");
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetNexonUserNo().c_str() );

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		
		return;
	}

	if(ValofePurchaseGift(szNexonID, szBuyNo, (ioData&)rData, szErrString, iResult) == FALSE)
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] outputCash RestAPI Error: %d:%d:%s:%s:%s:%s:%s:%d:%s",GetCurrentThreadId(), rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetNexonUserNo().c_str(), GetLastError(), szErrString.c_str());

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );

	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << rData.GetExp();
	kPacket << rData.GetItemType();
	kPacket << rData.GetItemPayAmt();
	kPacket << 0; // TransactionID ( FOR US )
	kPacket << szBuyNo; // same szChargeNo

	int iItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i <MAX_ITEM_VALUE; i++)
		iItemValueList[i] = rData.GetItemValue( i );
	ioChannelingNodeParent::SetItemValueList( kPacket, rData.GetItemType(), iItemValueList );

	int iReturnCash = 0;//kyg 채울것
	int iPurchaseCash = 0;

	if( ValofeGetCash(rData, iReturnCash, iPurchaseCash, iResult, szErrString) == FALSE )
		iReturnCash = 0;

	kPacket << rData.GetChannelingType(); // 공통
	kPacket << iReturnCash;
	kPacket << iReturnCash;

	static TwoOfINTVec vInfo;
	vInfo.clear();

	rData.GetBonusCashInfo(vInfo);
	int iBonusCashSize	= vInfo.size();

	kPacket << iBonusCashSize;

	for( int i = 0; i < iBonusCashSize; i++ )
	{
		kPacket << vInfo[i].value1 << vInfo[i].value2;
	}

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioLocalTaiwan::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
}

void ioLocalTaiwan::ThreadSubscriptionRetract( const ioData& rData )
{
	/*
	ioHashString sError = "UnKnownError";

	const ioHashString& szNexonID = rData.GetNexonUserNo();

	int iResult = 0;

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "%s LogOut: %d:%s:%s:%s:%s",__FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), szNexonID.c_str() );

		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, BILLING_SUBSCRIPTION_RETRACT_RESULT_FAIL, rData, sError);
	}

	if(NexonSubscription(szNexonID, (ioData&)rData, sError, iResult) == TRUE)
	{
		int iReturnCash = 0;
		int iPurchaseCash = 0;
		int iCancelCash = rData.GetItemPayAmt();

		if( ValofeGetCash(rData, iReturnCash, iPurchaseCash, iResult, sError) == FALSE )
			iReturnCash = 0;

		SP2Packet kPacket( BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT );

		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetIndex();
		kPacket << rData.GetChargeNo();
		kPacket << BILLING_SUBSCRIPTION_RETRACT_RESULT_SUCCESS;
		kPacket << iCancelCash;
		kPacket << iReturnCash; 
		kPacket << iReturnCash; 

		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioLocalTaiwan::ThreadSubscriptionRetract", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
			return;
		}
	}
	else //fail처리 추가 
	{
	 
		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, BILLING_SUBSCRIPTION_RETRACT_RESULT_FAIL, rData, sError);

		LOG.PrintTimeAndLog(0, "[error][nexon] subscriptionRetract Fail thriadID:%d userIndex %d "
			"BillGUID :%s "
			"PrivateID: %s "
			"PublicID : %s "
			"UserNo : %s "
			"NexonUserNo : %s"
			"Charge No : %s ",
			GetCurrentThreadId(),rData.GetUserIndex(),
			rData.GetBillingGUID().c_str(),
			rData.GetPrivateID().c_str(),
			rData.GetPublicID().c_str(),
			rData.GetUserNo().c_str(),
			rData.GetNexonUserNo().c_str(),
			rData.GetChargeNo().c_str() );
		return;
	}
	// 캐쉬와 실재 구매한 캐쉬가 동일하다.

	LOG.PrintTimeAndLog(0, "[info][nexon] subscriptionRetract Success threadID:%d userIndex %d "
									"BillGUID :%s "
									"PrivateID: %s "
									"PublicID : %s "
									"UserNo : %s "
									"NexonUserNo : %s"
									"Charge No : %s ",
									GetCurrentThreadId(),rData.GetUserIndex(),
									rData.GetBillingGUID().c_str(),
									rData.GetPrivateID().c_str(),
									rData.GetPublicID().c_str(),
									rData.GetUserNo().c_str(),
									rData.GetNexonUserNo().c_str(),
									rData.GetChargeNo().c_str() );
*/

}
  
BOOL ioLocalTaiwan::ValofeGetCash(const ioData& rData, int& iRealCash, int& iPurchase, int& iResult, ioHashString& szErrString )
{
	int cpType = 0;
	ioHashString strUserID = rData.GetPrivateID().c_str();
	if(rData.GetChannelingType() == CNT_HAPPYTUK)
	{
		cpType = 2;
		strUserID = rData.GetNexonUserNo();
	}

	char szReturnData[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnData, WEB_BUFF_SIZE );
	std:string strPostData;
	strPostData = "service_code=";
	strPostData += m_sServiceCode.c_str();
	strPostData += "&user_id=";
	strPostData += strUserID.c_str();
	strPostData += "&channeling_type=";
	strPostData += std::to_string( static_cast<long long>(cpType));

	ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
	if( !Winhttp.GetResultData( m_sGetURL.c_str(), strPostData.c_str(), szReturnData, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog(0,"[error][valofe] getcash ioRestAPI Error :%s %s %s %s %d", szReturnData, szErrString.c_str(), rData.GetPrivateID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
		return TRUE;
	}

	if(ValofeGetCashParse(szReturnData, iRealCash, iPurchase, iResult, szErrString) == TRUE)
	{
		iRealCash = iRealCash + iPurchase;
		return TRUE;
	}
	else
		LOG.PrintTimeAndLog(0,"[error][valofe] getcash Json Return Error :%s %s %s %s", szReturnData, szErrString.c_str(), rData.GetPrivateID().c_str(), rData.GetUserNo().c_str());

	return FALSE;
}

BOOL ioLocalTaiwan::ValofeGetCashParse( const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString )
{
	try 
	{
		cJSON *pJson = NULL;
		pJson=cJSON_Parse(szReturnData);

		if( pJson == NULL )
			return FALSE;

		BOOL bState =FALSE;

		for (BYTE i = 0; i < cJSON_GetArraySize(pJson); i++)
		{
			cJSON *pObject = cJSON_GetArrayItem( pJson, i);

			if( pObject && pObject->string )
			{
				if(strcmp("Result", pObject->string) == 0)
				{
					if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
						bState = TRUE;
				}
				else if(strcmp("real_balance", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						iRealCash = pObject->valueint;
					}
				}
				else if(strcmp("bonus_balance", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						iBonus = pObject->valueint;
					}
				}
			}
		}
		cJSON_Delete(pJson);
		return bState;
	}

	catch ( ... )
	{
		LOG.PrintTimeAndLog( 0, "[error][restapi] ValofeGetCash Json parse Crash %s ", szReturnData);
		return FALSE;
	}

	return FALSE;
}


BOOL ioLocalTaiwan::ValofePurchaseItem(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult)
{
	ioRestAPI ioRest;
	char szFullURL[MAX_PATH*2] = {0,};
	char szHeader[MAX_PATH] = {0,};
	std::string szTempString;
	ioHashString szJsonData;

	StringCbPrintf(szFullURL, sizeof(szFullURL), "%s", m_sPurchaseItemURL.c_str());

	if(ValofePurchaseParam(szUserID, rData, szErrString, szJsonData) == FALSE)
	{
		szErrString = "Json Error";
		return FALSE;
	}

	///////////////
	char szReturnData[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnData, WEB_BUFF_SIZE );

	ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
	if( !Winhttp.GetResultData( m_sPurchaseItemURL.c_str(), szJsonData.c_str(), szReturnData, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog(0,"[error][valofe] ValofePurchaseItem Error :%s %s %s %d", szReturnData, szErrString.c_str(), szUserID.c_str() ,GetLastError() );
		return TRUE;
	}


	if(ValofePurchaseParse(szReturnData, szBuyNo, rData, szErrString) == TRUE)
	{
		LOG.PrintTimeAndLog(0,"[Success][valofe] ValofePurchaseItem Return Test :%s JSon-%s UserID-%s GoodsNo-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo());
		return TRUE;
	}
	else
		LOG.PrintTimeAndLog(0,"[error][valofe] ValofePurchaseItem Return Error :%s JSon-%s UserID-%s GoodsNo-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo());


	return FALSE;
}

BOOL ioLocalTaiwan::ValofePurchaseParam( const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData )
{
	int iProduct_No = 0;
	ProductInfoMap::const_iterator iter = m_ProductInfoMap.find( rData.GetGoodsNo() );
	if( iter != m_ProductInfoMap.end() )
	{
		iProduct_No = iter->second;
	}

	int cpType = 0;
	ioHashString strUserID = rData.GetPrivateID().c_str();
	if(rData.GetChannelingType() == CNT_HAPPYTUK)
	{
		cpType = 2;
		strUserID = rData.GetNexonUserNo();
	}

	std:string strPostData;
	strPostData = "service_code=";
	strPostData += m_sServiceCode.c_str();
	strPostData += "&ip=";
	strPostData += std::to_string( static_cast<long long>(Help::GetStringIPToDWORDIP( rData.GetUserIP().c_str())));
	strPostData += "&short_ip=";
	strPostData += rData.GetUserIP().c_str();
	strPostData += "&reason=";
	strPostData += "1";
	strPostData += "&user_age=";
	strPostData += "1";
	strPostData += "&order_id=";
	strPostData += rData.GetBillingGUID().c_str();
	strPostData += "&total_amount=";
	strPostData += std::to_string( static_cast<long long>(rData.GetItemPayAmt()));
	strPostData += "&user_id=";
	strPostData += strUserID.c_str();
	strPostData += "&user_oid=";
	strPostData += std::to_string( static_cast<long long>(rData.GetUserIndex()));
	strPostData += "&payment_type=";
	strPostData += "13001";
	strPostData += "&payment_rule_id=";
	strPostData += "1";
	strPostData += "&product_array_length=";
	strPostData += "1";
	strPostData += "&product_info=";
	strPostData += "[";

	cJSON *ProductInfo = NULL;
	ProductInfo = cJSON_CreateObject();
	cJSON_AddNumberToObject(ProductInfo, "product_no", iProduct_No);
	cJSON_AddNumberToObject(ProductInfo, "order_quantity", 1);
			
	char* szReutrnTemp = cJSON_PrintUnformatted(ProductInfo);
	strPostData += szReutrnTemp;
	strPostData += "]";
	strPostData += "&channeling_type=";
	strPostData += std::to_string( static_cast<long long>(cpType));

	szJsonData = strPostData.c_str();
	return TRUE;
}

BOOL ioLocalTaiwan::ValofePurchaseParse( const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString )
{
	try 
	{
		cJSON *pJson = NULL;
		pJson=cJSON_Parse(szReturnData);

		if( pJson == NULL )
			return FALSE;

		BOOL bState =FALSE;

		for (BYTE i = 0; i < cJSON_GetArraySize(pJson); i++)
		{
			cJSON *pObject = cJSON_GetArrayItem( pJson, i);

			if( pObject && pObject->string )
			{
				if(strcmp("Result", pObject->string) == 0)
				{
					if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
					{
						bState = TRUE;
					}
					else
					{
						int iReturnAnsiSize = 0;
						char szTemp[512] = {0,};

						UTF8ToAnsi( pObject->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );

						szErrString = szTemp;
					}
				}
				else if(strcmp("OrderNo", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						if(pObject->valueint == 0)
						{
							bState = FALSE;
						}
					}
				}
				else if(strcmp("OrderID", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						if(strcmp("null", pObject->valuestring) == 0)
						{
							bState = FALSE;
						}
					}
				}
			}
		}
		cJSON_Delete(pJson);
		return bState;
	}

	catch ( ... )
	{
		LOG.PrintTimeAndLog( 0, "[error][restapi] ValofeGetCash Json parse Crash %s ", szReturnData);
		return FALSE;
	}

	return FALSE;
}


BOOL ioLocalTaiwan::ValofePurchaseGift(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult)
{
	ioRestAPI ioRest;
	char szFullURL[MAX_PATH*2] = {0,};
	char szHeader[MAX_PATH] = {0,};
	std::string szTempString;
	ioHashString szJsonData;

	StringCbPrintf(szFullURL, sizeof(szFullURL), "%s", m_sPurchaseItemURL.c_str());

	if(ValofePurchaseGiftParam(szUserID, rData, szErrString, szJsonData) == FALSE)
	{
		szErrString = "Json Error";
		return FALSE;
	}

	///////////////
	char szReturnData[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnData, WEB_BUFF_SIZE );

	ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
	if( !Winhttp.GetResultData( m_sPurchaseGiftURL.c_str(), szJsonData.c_str(), szReturnData, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog(0,"[error][valofe] ValofePurchaseGift Error :%s %s %s %d", szReturnData, szErrString.c_str(), szUserID.c_str() ,GetLastError() );
		return TRUE;
	}


	if(ValofePurchaseGiftParse(szReturnData, szBuyNo, rData, szErrString) == TRUE)
	{
		LOG.PrintTimeAndLog(0,"[Success][valofe] ValofePurchaseGift Return Test :%s JSon-%s UserID-%s GoodsNo-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo());

		return TRUE;
	}
	else
		LOG.PrintTimeAndLog(0,"[error][valofe] ValofePurchaseGift Return Error :%s JSon-%s UserID-%s GoodsNo-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo());


	return FALSE;
}

BOOL ioLocalTaiwan::ValofePurchaseGiftParam( const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData )
{
	int iProduct_No = 0;
	ProductInfoMap::const_iterator iter = m_ProductInfoMap.find( rData.GetGoodsNo() );
	if( iter != m_ProductInfoMap.end() )
	{
		iProduct_No = iter->second;
	}

	int cpType = 0;
	ioHashString strUserID = rData.GetPrivateID().c_str();
	if(rData.GetChannelingType() == CNT_HAPPYTUK)
	{
		cpType = 2;
		strUserID = rData.GetNexonUserNo();
	}

	std:string strPostData;
	strPostData = "service_code=";
	strPostData += m_sServiceCode.c_str();
	strPostData += "&ip=";
	strPostData += std::to_string( static_cast<long long>(Help::GetStringIPToDWORDIP( rData.GetUserIP().c_str())));
	strPostData += "&short_ip=";
	strPostData += rData.GetUserIP().c_str();
	strPostData += "&reason=";
	strPostData += "1";
	strPostData += "&sender_user_id=";
	strPostData += strUserID.c_str();
	strPostData += "&sender_user_oid=";
	strPostData += std::to_string( static_cast<long long>(rData.GetUserIndex()));
	strPostData += "&sender_user_age=";
	strPostData += "1";
	strPostData += "&receiver_user_id=";
	strPostData += rData.GetReceivePrivateID().c_str();
	strPostData += "&receiver_user_oid=";
	strPostData += std::to_string( static_cast<long long>(rData.GetRecvUserIndex()));
	strPostData += "&receiver_server_no=";
	strPostData += "1";
	strPostData += "&message=";
	strPostData += rData.GetPublicID().c_str();
	strPostData += " Gift ";
	strPostData += rData.GetReceivePublicID().c_str();
	strPostData += "&order_id=";
	strPostData += rData.GetBillingGUID().c_str();
	strPostData += "&payment_type=";
	strPostData += "13001";
	strPostData += "&payment_rule_id=";
	strPostData += "1";
	strPostData += "&total_amount=";
	strPostData += std::to_string( static_cast<long long>(rData.GetItemPayAmt()));
	strPostData += "&product_array_length=";
	strPostData += "1";
	strPostData += "&product_info=";
	strPostData += "[";

	cJSON *ProductInfo = NULL;
	ProductInfo = cJSON_CreateObject();
	cJSON_AddNumberToObject(ProductInfo, "product_no", iProduct_No);
	cJSON_AddNumberToObject(ProductInfo, "order_quantity", 1);
			
	char* szReutrnTemp = cJSON_PrintUnformatted(ProductInfo);
	strPostData += szReutrnTemp;
	strPostData += "]";
	strPostData += "&channeling_type=";
	strPostData += std::to_string( static_cast<long long>(cpType));

	szJsonData = strPostData.c_str();
	return TRUE;
}

BOOL ioLocalTaiwan::ValofePurchaseGiftParse( const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString )
{
	try 
	{
		cJSON *pJson = NULL;
		pJson=cJSON_Parse(szReturnData);

		if( pJson == NULL )
			return FALSE;

		BOOL bState =FALSE;

		for (BYTE i = 0; i < cJSON_GetArraySize(pJson); i++)
		{
			cJSON *pObject = cJSON_GetArrayItem( pJson, i);

			if( pObject && pObject->string )
			{
				if(strcmp("Result", pObject->string) == 0)
				{
					if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
					{
						bState = TRUE;
					}
					else
					{
						int iReturnAnsiSize = 0;
						char szTemp[512] = {0,};

						UTF8ToAnsi( pObject->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );

						szErrString = szTemp;
					}
				}
				else if(strcmp("OrderNo", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						if(pObject->valueint == 0)
						{
							bState = FALSE;
						}
					}
				}
				else if(strcmp("OrderID", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						if(strcmp("null", pObject->valuestring) == 0)
						{
							bState = FALSE;
						}
					}
				}
			}
		}
		cJSON_Delete(pJson);
		return bState;
	}

	catch ( ... )
	{
		LOG.PrintTimeAndLog( 0, "[error][restapi] ValofeGetCash Json parse Crash %s ", szReturnData);
		return FALSE;
	}

	return FALSE;
}


void ioLocalTaiwan::SendExecptMessage( DWORD dwPacketID, int iErrCode, const ioData& rData, ioHashString& szErrString )
{
	SP2Packet kPacket( dwPacketID );

	switch(dwPacketID)
	{
	case BSTPK_GET_CASH_RESULT:
		{
			kPacket << rData.GetUserIndex();
			kPacket << rData.GetBillingGUID();
			kPacket << rData.GetSetUserMouse();
			kPacket << iErrCode;
			kPacket << (!szErrString.IsEmpty());
			kPacket << szErrString;
		}
		break;
	case BSTPK_OUTPUT_CASH_RESULT:
		{
			bool bError	= !szErrString.IsEmpty();

			kPacket << rData.GetUserIndex();
			kPacket << rData.GetBillingGUID();
			kPacket << iErrCode;
			kPacket << rData.GetExp();
			kPacket << rData.GetItemType();
			kPacket << bError;

			if( bError )
				kPacket << szErrString;

			static TwoOfINTVec vInfo;
			vInfo.clear();

			rData.GetBonusCashInfo(vInfo);
			int iBonusCashSize	= vInfo.size();

			kPacket << iBonusCashSize;

			for( int i = 0; i < iBonusCashSize; i++ )
			{
				kPacket << vInfo[i].value1 ;
			}

			kPacket << true;
		}
		break;
	case BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT: 
		{
			kPacket << rData.GetUserIndex();
			kPacket << rData.GetBillingGUID();
			kPacket << (DWORD) 0;
			kPacket << rData.GetChargeNo();
			kPacket << iErrCode;
		}
		break;
	}

	if(!g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ))
	{
		LOG.PrintTimeAndLog( 0, "[error][nexon] ProcessType:%d Send Fail: %d:%s:%s", "ioChannelingNodeWemadeCashLink::SendExecptMessage Fail", 
			rData.GetProcessType(),
			rData.GetUserIndex(),
			rData.GetBillingGUID().c_str(),
			rData.GetPrivateID().c_str() );
	}
	return;
}

void ioLocalTaiwan::UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize )
{
	if( szUTF8 == NULL )
		return;

	WCHAR szUni[MAX_PATH*2];
	ZeroMemory( szUni, sizeof(szUni) );

	int iUnisize = MultiByteToWideChar(CP_UTF8, 0, szUTF8, -1 , szUni, MAX_PATH*2 );
	riReturnAnsiSize = WideCharToMultiByte(CP_ACP, 0 , szUni, iUnisize, szAnsi, iAnsiBufferSize ,NULL ,NULL );
}

void ioLocalTaiwan::MakeTransactionID( char* szTransactionID, ioData &rData )
{
	if(szTransactionID)
	{
		char szAgencyNo[ioLocalParent::MAX_AGENCY_NO_PLUS_ONE]="";

		ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
		if( pLocal )
		{
			pLocal->GetAgencyNo( szAgencyNo, sizeof( szAgencyNo ), true );
		}
		StringCbPrintf( szTransactionID, 64, "NEXONBILL%s%s10000", szAgencyNo, rData.GetUserNo().c_str() );
	}
}

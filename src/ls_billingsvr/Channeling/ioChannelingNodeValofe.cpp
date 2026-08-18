#include "../stdafx.h"
#include "./ioChannelingNodeValofe.h"
#include "../NodeInfo/ServerNode.h"
#include "../Util/ioHashString.h"
#include "../ThreadPool/ioThreadPool.h"
#include "../NodeInfo/ServerNodeManager.h"
#include "../MainProcess.h"
#include "../database/logdbclient.h"
#include "../Local/ioLocalParent.h" 
#include "LS_RestAPI/ioRestAPI.h"
#include "../ThreadPool/NexonThreadPool.h"
#include "../EtcHelpFunc.h"
#include "../Util/cJSON.h"

#define VALUFE_REUTRN_SUCCESS 1

extern CLog LOG;
extern CLog BillingItemLOG;

#ifndef VALOFE_NEW_BILLING_SYS_SYH
#import "../Adbill/bxIPGClient.dll" no_namespace
#endif // VALOFE_NEW_BILLING_SYS_SYH

#define WTOM( wstr, mstr ) WideCharToMultiByte( CP_ACP, 0, (wstr), -1, (mstr), MAX_BUF_SIZE, NULL, NULL )

ioChannelingNodeValofe::ioChannelingNodeValofe(void)
{
	char szTemp[MAX_PATH]="";
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle("Valofe_billing");
	
#ifdef VALOFE_NEW_BILLING_SYS_SYH
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

	kLoader.LoadString( "ValofePurchaseItemRetractURL", "", szTemp, sizeof( szTemp ) );
	m_sSubscriptionRetractURL = szTemp;

	kLoader.LoadString( "ValofePurchaseGiftURL", "", szTemp, sizeof( szTemp ) );
	m_sPurchaseGiftURL = szTemp;

	m_ProductInfoMap.clear();
#else
	kLoader.LoadString( "ValofeCompanyCD", "", szTemp, sizeof( szTemp ) );
	m_szCompanyCD = szTemp;
#endif
}

ioChannelingNodeValofe::~ioChannelingNodeValofe(void)
{
	for(ProductInfoMap::iterator iter = m_ProductInfoMap.begin(); iter != m_ProductInfoMap.end(); ++iter)
		delete iter->second;

	m_ProductInfoMap.clear();
}

ChannelingType ioChannelingNodeValofe::GetType()
{
	return CNT_VALOFE;
}

BOOL ioChannelingNodeValofe::ProductInit()
{
	return TRUE;
}

void ioChannelingNodeValofe::_OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType   = 0;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex       = 0;
	ioHashString szPrivateIndex;
	ioHashString szPrivateID;
	ioHashString szPublicID;
	bool         bSetUserMouse     = false;
	ioHashString szValofeID;
	ioHashString szValofeNo;
	ioData kData;

	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> szPrivateID >> szPublicID >> bSetUserMouse; // 공통사항
	rkPacket >> szValofeID; // For Valofe
	rkPacket >> szValofeNo; // For Valofe

	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPrivateID( szPrivateID );
	kData.SetPublicID( szPublicID );
	kData.SetSetUserMouse( bSetUserMouse );
	kData.SetUserNo( szValofeID );
	kData.SetNexonUserNo( szValofeNo );

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_GET_CASH );

	g_ThreadPool.SetData( kData );
}

void ioChannelingNodeValofe::_OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName )
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
	int			 iBonusCashSize	= 0;

	ioData kData;
	rkPacket >> iChannelingType >> szBillingGUID >> iReturnItemPrice >> dwUserIndex >> szPublicID >> szPrivateID >> szUserIP >> iPayAmt >> iBonusCashSize;

#ifdef VALOFE_NEW_BILLING_LOG
	LOG.PrintTimeAndLog( 0, "[Check][valofe] %s - iChannelingType : %d, szBillingGUID : %s, iReturnItemPrice : %d, dwUserIndex : %d", 
		__FUNCTION__, iChannelingType, szBillingGUID.c_str(), iReturnItemPrice, dwUserIndex );
	LOG.PrintTimeAndLog( 0, "[Check][valofe] %s - szPublicID : %s, szPrivateID : %s, szUserIP : %s, iPayAmt : %d", 
		__FUNCTION__, szPublicID.c_str(), szPrivateID.c_str(), szUserIP.c_str(), iPayAmt );
#endif

	for( int i = 0; i < iBonusCashSize; i++ )
	{
		int iIndex		= 0;
		int iBonusCash	= 0;

		rkPacket >> iIndex >> iBonusCash;

#ifdef VALOFE_NEW_BILLING_LOG
		LOG.PrintTimeAndLog( 0, "[Check][valofe] %s - Size : %d, iIndex : %d, iBonusCash : %d", __FUNCTION__, iBonusCashSize, iIndex, iBonusCash );
#endif

		kData.AddBonusCashInfoForConsume(iIndex, iBonusCash);
	}

	rkPacket >> iType; // 공통사항	
#ifdef VALOFE_NEW_BILLING_LOG
	LOG.PrintTimeAndLog( 0, "[Check][valofe] %s - Type : %d", __FUNCTION__, iType );
#endif

	int iItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i < MAX_ITEM_VALUE ; i++)
		iItemValueList[i] = 0;
	if( iType != OUTPUT_CASH_PRESENT)
		GetItemValueList( rkPacket, iType, iItemValueList );

	if( iType == OUTPUT_CASH_PRESENT)
	{
		short		iPresentType = 0;
		int			iBuyValue1 = 0;
		int			iBuyValue2 = 0;
		DWORD        dwReUserIndex = 0;

		rkPacket >>  iPresentType;
		rkPacket >>  iBuyValue1;
		rkPacket >>  iBuyValue2;
		rkPacket >>  dwReUserIndex;

		iItemValueList[ 0 ] = iPresentType;
		iItemValueList[ 1 ] = iBuyValue1;
		iItemValueList[ 2 ] = iBuyValue2;
		iItemValueList[ 3 ] = dwReUserIndex;
		iItemValueList[ 4 ] = 0;

		kData.SetRecvUserIndex(dwReUserIndex);

#ifdef VALOFE_NEW_BILLING_LOG
		LOG.PrintTimeAndLog( 0, "[Check][valofe] %s - PresentType : %d, BuyValue1 : %d, BuyValue2 : %d, ReUserIndex : %d", __FUNCTION__, iPresentType, iBuyValue1, iBuyValue2, dwReUserIndex );
#endif
	}

	ioHashString szValofeID;
	ioHashString szValofeNo;
	int          iGameServerPort = 0;
	ioHashString szReceivePrivateID; // 선물인 경우만 셋팅됨
	ioHashString szReceivePublicID;  // 선물인 경우만 셋팅됨
	DWORD dwRecvUserIndex = 0;

	rkPacket >> szValofeID; // For Valofe
	rkPacket >> szValofeNo; // For Valofe
	rkPacket >> iGameServerPort >> szReceivePrivateID >> szReceivePublicID >> dwRecvUserIndex; // wemadebuy

	kData.SetUserNo( szValofeID );
	kData.SetNexonUserNo( szValofeNo );
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
	kData.SetGoodsNo( dwGoodsNo );
	kData.SetGoodsName( rszGoodsName );
	kData.SetGameServerPort(iGameServerPort);
	kData.SetReceivePrivateID(szReceivePrivateID);
	kData.SetReceivePublicID(szReceivePublicID);

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}

	if( iType == OUTPUT_CASH_PRESENT)
	{
		kData.SetProcessType( ioData::PT_PRESENT );
	}
	else
	{
		kData.SetProcessType( ioData::PT_OUTPUT_CASH );
	}
		kData.SetEmpty( false );

	g_ThreadPool.SetData( kData );
}

void ioChannelingNodeValofe::_OnSubscriptionRetract( ServerNode* pServerNode, SP2Packet& rkPacket ) //kyg 이루틴 타는걸로도 테스트 해봐야함 
{
	int          iChannelingType = 0;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex = 0;
	ioHashString szPublicID;
	ioHashString szPrivateID;
	DWORD		 dwIndex = 0;
	ioHashString szChargeNo;
	ioHashString szValofeID;
	ioHashString szValofeNo;
	int cancelGold = 0;

	rkPacket >> iChannelingType;
	rkPacket >> szBillingGUID;
	rkPacket >> dwUserIndex;
	rkPacket >> szPublicID;
	rkPacket >> szPrivateID;
	rkPacket >> dwIndex; 
	//위에까지 공통인자 
	rkPacket >> szChargeNo;
	rkPacket >> szValofeID;
	rkPacket >> szValofeNo;
	rkPacket >> cancelGold;

	ioData kData;

	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPublicID( szPublicID );
	kData.SetPrivateID( szPrivateID );
	kData.SetUserNo( szValofeID );
	kData.SetChargeNo( szChargeNo );
	kData.SetIndex( dwIndex );
	kData.SetItemPayAmt(cancelGold);
	kData.SetNexonUserNo( szValofeNo );

	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_SUBSCRIPTION_RETRACT );

	g_ThreadPool.SetData( kData );
}


void ioChannelingNodeValofe::ThreadGetCash( const ioData &rData )
{
#ifdef VALOFE_NEW_BILLING_SYS_SYH
	const ioHashString& szNexonNo = rData.GetNexonUserNo();

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] getCash Data is Empty." );
		return;
	} 

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] getCash LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );
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

		LOG.PrintTimeAndLog( 0, "[error][valofe] getCash fail: threadid:%d userIndex: %d "
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

	LOG.PrintTimeAndLog( 0, "[info][valofeChannelling] getCash Success: threadId : %d userIndex : %d "
		"BillGUID : %s "
		"PrivateID : %s "
		"PublicID : %s "
		"UserNo : %s "
		"(%d/%d)",
		GetCurrentThreadId(), rData.GetUserIndex(),
		rData.GetBillingGUID().c_str(),
		rData.GetPrivateID().c_str(),
		rData.GetPublicID().c_str(),
		rData.GetUserNo().c_str(),
		iReturnCash, iPurchasedCash  );
#else
	const ioHashString& szNexonNo = rData.GetUserNo();

	if( rData.IsEmpty() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe] getCash Data is Empty." );
		return;
	}

	ioHashString szErrString;

	if( g_App.IsReserveLogOut() )
	{
		SendExecptMessage(BSTPK_GET_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	int iReturnCash    = 0;
	int iPurchasedCash = 0; // 현금을 주고 구매한 캐쉬
	int iResult = 0;

	if(ValofeGetCash(rData.GetNexonUserNo().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), iReturnCash, iPurchasedCash, iResult, szErrString) == FALSE)
	{
		SendExecptMessage(BSTPK_GET_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);

		BillingItemLOG.PrintTimeAndLog( 0, "[error][valofe] getCash fail: threadid:%d userIndex: %d "
			"BillGUID:%s "
			"UserNo %s "
			"PublicID  %s "
			"UserNo : %s "
			"NexonUserNo : %s"
			"(%d/%d)"
			"errString :%s",
			GetCurrentThreadId(),rData.GetUserIndex(),
			rData.GetBillingGUID().c_str(),
			rData.GetUserNo().c_str(),
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
	kPacket << iResult;
	kPacket << iReturnCash;
	kPacket << iReturnCash;

	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioChannelingNodeValofe::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}

	BillingItemLOG.PrintTimeAndLog( 0, "%s Success: %d:%s:PrivateID %s:%s:[%d/%d],%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), iReturnCash, iPurchasedCash, iResult  );
#endif //VALOFE_NEW_BILLING_SYS_SYH
}

void ioChannelingNodeValofe::ThreadOutputCash( const ioData &rData )
{
#ifdef VALOFE_NEW_BILLING_SYS_SYH
	const ioHashString& szValofeID = rData.GetNexonUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash Data is Empty.");
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetNexonUserNo().c_str() );

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
	if(ValofePurchaseItem(szValofeID, szBuyNo, (ioData&)rData, szErrString, iResult) == FALSE)
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash RestAPI Error: %d:%d:%s:%s:%s:%s:%s:%d:%s",GetCurrentThreadId(), rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetNexonUserNo().c_str(), GetLastError(), szErrString.c_str());

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

#else
	const ioHashString& szNexonID = rData.GetUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe]%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe] outputCash %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		
		return;
	}
	
	char szOrderNo[MAX_BUF_SIZE]="";
	ZeroMemory( szOrderNo, sizeof( szOrderNo ) );
	long iReturnValue = 0;
	try 
	{
		IIPGClientPtr ptrClient;
		ptrClient.CreateInstance(__uuidof(IPGClient));

		_variant_t vsUserNo, vsUserID, vsCompanyCD, vsNickName, vsGoodsNo, vsPayAmt, vsAgencyNo, vsUserIP, vsEtc1Name, vsEtc1Value, vsEtc1Receive;// input
		_variant_t vsBxaid ;    // output

		int iAdbillbillingCode = 6000;
		enum { MAX_ETC_PLUS_ONE = 101, };
		char szEtc1[MAX_ETC_PLUS_ONE]="";
		StringCbPrintf( szEtc1, sizeof( szEtc1 ), "%d_%s", rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
		
		char szAgencyNo[ioLocalParent::MAX_AGENCY_NO_PLUS_ONE]="";
		ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
		if( pLocal )
		{
			pLocal->GetAgencyNo( szAgencyNo, sizeof( szAgencyNo ), true );
		}

		vsUserNo		= (LPSTR) rData.GetNexonUserNo().c_str(); 
		
		vsUserID		= (LPSTR) rData.GetUserNo().c_str();
		vsNickName		= (LPSTR) rData.GetPublicID().c_str();
		vsCompanyCD		= (LPSTR) m_szCompanyCD.c_str(); 
		vsUserIP		= (LPSTR) rData.GetUserIP().c_str();

		vsGoodsNo		= (LONG)  iAdbillbillingCode;
//		vsPayAmt		= (LONG)  10;
		vsPayAmt		= (LONG)  rData.GetItemPayAmt();
		vsAgencyNo		= (LPSTR) szAgencyNo;

		vsEtc1Name		= (LPSTR) szEtc1;
		vsEtc1Value     = (LPSTR) "1";
		vsEtc1Receive	= (LPSTR) rData.GetNexonUserNo().c_str();

		// 값을 받을때까지 대기 
		_variant_t vsResult = ptrClient->Output(&vsUserNo, &vsUserID, &vsCompanyCD, &vsNickName, &vsUserIP, &vsGoodsNo, &vsPayAmt ,&vsAgencyNo, &vsEtc1Name, &vsEtc1Value, &vsEtc1Receive, &vsBxaid ); 
		char szResult[MAX_PATH] = "";
		WTOM( vsResult.bstrVal, szResult );

		if (strcmp(szResult,"0000") == 0)
		{
			iReturnValue = CASH_RESULT_SUCCESS;

			char szBuy[MAX_PATH] = "";
			WTOM( vsBxaid.bstrVal, szBuy );
			szBuyNo = szBuy;
			BillingItemLOG.PrintTimeAndLog(0, "%s Success:%d:%s:PrivateID %s:%s:%s:%u:%s:%d:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), szOrderNo, rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt(), szBuyNo );
		}
		else
		{
			iReturnValue = CASH_RESULT_EXCEPT;
			BillingItemLOG.PrintTimeAndLog(0, "%s ServerNode::ThreadOutputCash ReturnValue is Error:%d:%s:PrivateID %s:%s:%s:%u:%s", __FUNCTION__, rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), szResult, rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
		}
	} 
	catch (const _com_error &e )
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%u:%s:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), (char*) e.ErrorMessage(), GetLastError() );
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}
	catch ( ... )	
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%u:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), GetLastError() );
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	// return value
	if( iReturnValue != CASH_RESULT_SUCCESS )
	{
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
	}

	int iReturnCash = 0;//kyg 채울것
	int iPurchaseCash = 0;
	if( ValofeGetCash(rData.GetNexonUserNo().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), iReturnCash, iPurchaseCash, iResult, szErrString) == FALSE )
		iReturnCash = 0;

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

	// Cancel Step 1
	kPacket << rData.GetChannelingType();  // 공통
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
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioLocalTaiwan::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
#endif // VALOFE_NEW_BILLING_SYS_SYH
}


void ioChannelingNodeValofe::ThreadPresent( const ioData &rData )
{
#ifdef VALOFE_NEW_BILLING_SYS_SYH
	const ioHashString& szNexonID = rData.GetNexonUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash Data is Empty.");
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetNexonUserNo().c_str() );

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);

		return;
	}

	if(ValofePurchaseItem(szNexonID, szBuyNo, (ioData&)rData, szErrString, iResult) == FALSE)
	{
		LOG.PrintTimeAndLog( 0, "[error][valofe] outputCash RestAPI Error: %d:%d:%s:%s:%s:%s:%s:%d:%s",GetCurrentThreadId(), rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetNexonUserNo().c_str(), GetLastError(), szErrString.c_str());

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
#else
	const ioHashString& szValofeID = rData.GetUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe]%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe] ThreadPresent %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );

		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}
	
	char szOrderNo[MAX_BUF_SIZE]="";
	ZeroMemory( szOrderNo, sizeof( szOrderNo ) );
	long iReturnValue = 0;
	try 
	{
		IIPGClientPtr ptrClient;
		ptrClient.CreateInstance(__uuidof(IPGClient));

		_variant_t vsUserNo, vsUserID, vsCompanyCD, vsNickName, vsGoodsNo, vsPayAmt, vsAgencyNo, vsUserIP, vsEtc1Name, vsEtc1Value, vsEtc1Receive;// input
		_variant_t vsBxaid ;    // output

		int iAdbillbillingCode = 6000;
		enum { MAX_ETC_PLUS_ONE = 101, };
		char szEtc1[MAX_ETC_PLUS_ONE]="";
		StringCbPrintf( szEtc1, sizeof( szEtc1 ), "%d_%s", rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
		
		char szAgencyNo[ioLocalParent::MAX_AGENCY_NO_PLUS_ONE]="";
		ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
		if( pLocal )
		{
			pLocal->GetAgencyNo( szAgencyNo, sizeof( szAgencyNo ), true );
		}

		char szRecvUserIndex[MAX_PATH] = "";
		StringCbPrintf( szRecvUserIndex, sizeof(szRecvUserIndex), "%d", rData.GetRecvUserIndex());
		vsUserNo		= (LPSTR) rData.GetNexonUserNo().c_str(); 
		
		vsUserID		= (LPSTR) rData.GetUserNo().c_str();
		vsNickName		= (LPSTR) rData.GetPublicID().c_str();
		vsCompanyCD		= (LPSTR) m_szCompanyCD.c_str(); 
		vsUserIP		= (LPSTR) rData.GetUserIP().c_str();

		vsGoodsNo		= (LONG)  iAdbillbillingCode;
//		vsPayAmt		= (LONG)  10;
		vsPayAmt		= (LONG)  rData.GetItemPayAmt();
		vsAgencyNo		= (LPSTR) szAgencyNo;

		vsEtc1Name		= (LPSTR) szEtc1;
		vsEtc1Value     = (LPSTR) "1";
		vsEtc1Receive	= (LPSTR) szRecvUserIndex;

		// 값을 받을때까지 대기 
		_variant_t vsResult = ptrClient->Output(&vsUserNo, &vsUserID, &vsCompanyCD, &vsNickName, &vsUserIP, &vsGoodsNo, &vsPayAmt ,&vsAgencyNo, &vsEtc1Name, &vsEtc1Value, &vsEtc1Receive, &vsBxaid ); 
		char szResult[MAX_PATH] = "";
		WTOM( vsResult.bstrVal, szResult );

		if (strcmp(szResult,"0000") == 0)
		{
			iReturnValue = CASH_RESULT_SUCCESS;

			char szBuy[MAX_PATH] = "";
			WTOM( vsBxaid.bstrVal, szBuy );
			szBuyNo = szBuy;
			BillingItemLOG.PrintTimeAndLog(0, "%s Recv:%s Success:%d:%s:PrivateID %s:%s:%s:%u:%s:%d:%s", __FUNCTION__, szRecvUserIndex, rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), szOrderNo, rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt(), szBuyNo );
		}
		else
		{
			iReturnValue = CASH_RESULT_EXCEPT;
			BillingItemLOG.PrintTimeAndLog(0, "%s ServerNode::ThreadPresent ReturnValue is Error:%d:%s:PrivateID %s:%s:%s:%u:%s", __FUNCTION__, szRecvUserIndex, rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), szResult, rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
		}
	} 
	catch (const _com_error &e )
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%u:%s:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), (char*) e.ErrorMessage(), GetLastError() );
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}
	catch ( ... )	
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%u:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), GetLastError() );
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	// return value
	if( iReturnValue != CASH_RESULT_SUCCESS )
	{
		SendExecptMessage(BSTPK_OUTPUT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
	}

	int iReturnCash = 0;//kyg 채울것
	int iPurchaseCash = 0;
	if( ValofeGetCash(rData.GetNexonUserNo().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), iReturnCash, iPurchaseCash, iResult, szErrString) == FALSE )
		iReturnCash = 0;

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

	// Cancel Step 1
	kPacket << rData.GetChannelingType();  // 공통
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
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioLocalTaiwan::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
#endif // VALOFE_NEW_BILLING_SYS_SYH
}

void ioChannelingNodeValofe::ThreadSubscriptionRetract( const ioData& rData )
{
#ifdef VALOFE_NEW_BILLING_SYS_SYH
	// 청약 철회 기능 연결하지 않음 (인게임에서 쓰이지 않을것이기 때문)
#else
	ioHashString sError = "UnKnownError";

	const ioHashString& szValofeID = rData.GetUserNo();
	ioHashString szErrString;
	ioHashString szBuyNo;
	int iResult = 0;

	if( rData.IsEmpty() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe]%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe] ThreadSubscriptionRetract %s LogOut: %d:%s:%s:%s:%s", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );

		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	long iReturnValue = 0;
	try 
	{
		IIPGClientPtr ptrClient;
		ptrClient.CreateInstance(__uuidof(IPGClient));

		_variant_t vsUserNo, vsUserID, vsCompanyCD, vsNickName, vsUserIP, vOrderNo, vsGoodsNo, vsAgencyNo, vsEtc1Name, vsEtc1Value, vsEtc1Receive;// input
		_variant_t vsBxaid ;    // output

		int iAdbillbillingCode = 6000;
		enum { MAX_ETC_PLUS_ONE = 101, };
		char szEtc1[MAX_ETC_PLUS_ONE]="";
		StringCbPrintf( szEtc1, sizeof( szEtc1 ), "%d_%s", rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
		
		char szAgencyNo[ioLocalParent::MAX_AGENCY_NO_PLUS_ONE]="";
		ioLocalParent *pLocal = g_LocalMgr.GetLocal( ioLocalManager::GetLocalType() );
		if( pLocal )
		{
			pLocal->GetAgencyNo( szAgencyNo, sizeof( szAgencyNo ), true );
		}

		char szCancel[MAX_PATH] = "";
		StringCbPrintf( szCancel, sizeof(szCancel), "구매취소_%s", rData.GetChargeNo().c_str());


		vsUserNo		= (LPSTR) rData.GetNexonUserNo().c_str(); 
		
		vsUserID		= (LPSTR) rData.GetUserNo().c_str();
		vsNickName		= (LPSTR) rData.GetPublicID().c_str();
		vsCompanyCD		= (LPSTR) m_szCompanyCD.c_str(); 
		vsUserIP		= (LPSTR) rData.GetUserIP().c_str();
		vOrderNo		= (LPSTR) rData.GetChargeNo().c_str();
		vsGoodsNo		= (LONG)  iAdbillbillingCode;
		vsAgencyNo		= (LPSTR) szAgencyNo;

		vsEtc1Name		= (LPSTR) szCancel;
		vsEtc1Value     = (LPSTR) "1";
		vsEtc1Receive	= (LPSTR) rData.GetNexonUserNo().c_str();

		// 값을 받을때까지 대기 
		_variant_t vsResult = ptrClient->OutputCancel(&vsUserNo, &vsUserID, &vsCompanyCD, &vsNickName, &vsUserIP, &vOrderNo, &vsGoodsNo, &vsAgencyNo, &vsEtc1Name, &vsEtc1Value, &vsEtc1Receive, &vsBxaid ); 
		ioHashString strResult;
		char szResult[MAX_PATH] = "";
		WTOM( vsResult.bstrVal, szResult );
		strResult = szResult;

		if (strcmp(szResult,"0000") == 0)
		{
			char szBuy[MAX_PATH] = "";

			WTOM( vsBxaid.bstrVal, szBuy );
			szBuyNo = szBuy;
			iReturnValue = BILLING_SUBSCRIPTION_RETRACT_RESULT_SUCCESS;
			BillingItemLOG.PrintTimeAndLog(0, "%s Recv:%s Success:%d:%s:PrivateID %s:%s:%s:%d:%s", __FUNCTION__, rData.GetNexonUserNo().c_str(), rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), rData.GetChargeNo().c_str(), rData.GetItemPayAmt(), szBuyNo );
		}
		else
		{
			iReturnValue = CASH_RESULT_EXCEPT;
			BillingItemLOG.PrintTimeAndLog(0, "%s Recv:%s ReturnValue is Error:%d:%s:PrivateID %s:%s:%s:%d:%s", __FUNCTION__, rData.GetNexonUserNo().c_str(), rData.GetUserIndex(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetBillingGUID().c_str(), rData.GetChargeNo().c_str(), rData.GetItemPayAmt(), szResult);
		}
	} 
	catch (const _com_error &e )
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetChargeNo().c_str(), (char*) e.ErrorMessage(), GetLastError() );

		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}
	catch ( ... )	
	{
		BillingItemLOG.PrintTimeAndLog(0, "%s Exception:%s:%s:%s:%d", __FUNCTION__, rData.GetPublicID().c_str(), rData.GetBillingGUID().c_str(), rData.GetChargeNo().c_str(), GetLastError() );

		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
		return;
	}

	// return value
	if( iReturnValue != BILLING_SUBSCRIPTION_RETRACT_RESULT_SUCCESS )
	{
		SendExecptMessage(BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT, CASH_RESULT_EXCEPT, rData, szErrString);
	}

	int iReturnCash = 0;//kyg 채울것
	int iPurchaseCash = 0;

	if( ValofeGetCash(rData.GetNexonUserNo().c_str(), rData.GetUserNo().c_str(), rData.GetPublicID().c_str(), iReturnCash, iPurchaseCash, iResult, szErrString) == FALSE )
		iReturnCash = 0;

	// 캐쉬와 실재 구매한 캐쉬가 동일하다.
	SP2Packet kPacket( BSTPK_SUBSCRIPTION_RETRACT_CASH_RESULT );
	
	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << rData.GetIndex();
	kPacket << rData.GetChargeNo();
	kPacket << iReturnValue;
	kPacket << rData.GetItemPayAmt();
	kPacket << iReturnCash; 
	kPacket << iReturnCash; 
	 
	//kyg 일단.. 넣어주질 않음 szReturnBuyNO를 
	if(!g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ))
	{
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s Ret %d", "ioChannelingNodeDaum_v2::ThreadSubscriptionRetract", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetUserNo().c_str(), rData.GetChargeNo());
		return;
	}
#endif //VALOFE_NEW_BILLING_SYS_SYH
}
  
#ifdef VALOFE_NEW_BILLING_SYS_SYH
BOOL ioChannelingNodeValofe::ValofeGetCash(const ioData& rData, int& iRealCash, int& iPurchase, int& iResult, ioHashString& szErrString )
{
	int cpType = 1000;
	int steam = 0;
	ioHashString strUserID = rData.GetUserNo().c_str();		// 벨로프 채널링 ID

	char szReturnData[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnData, WEB_BUFF_SIZE );
	std:string strPostData;
	strPostData = "service_code=";
	strPostData += m_sServiceCode.c_str();
	strPostData += "&channeling_type=";
	strPostData += std::to_string( static_cast<long long>(cpType));
	strPostData += "&user_id=";
#ifdef VALOFE_NEW_BILLING_TEST
	strPostData += "valofe_qa1";
#else
	strPostData += strUserID.c_str();
#endif

	ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
	if( !Winhttp.GetResultData( m_sGetURL.c_str(), strPostData.c_str(), szReturnData, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog(0,"[error][valofe] getcash ioRestAPI Error :%s %s %s %d", szReturnData, rData.GetPrivateID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
		return TRUE;
	}

	if(ValofeGetCashParse(szReturnData, iRealCash, iPurchase, iResult, szErrString) == TRUE)
	{
		iRealCash = iRealCash + iPurchase;
		return TRUE;
	}
	else
		LOG.PrintTimeAndLog(0,"[error][valofe] getcash Json Return Error :%s %s %s %s - ErrorStr : %s", szReturnData, szErrString.c_str(), rData.GetPrivateID().c_str(), rData.GetUserNo().c_str(), szErrString.c_str());

	return FALSE;
}
#else
BOOL ioChannelingNodeValofe::ValofeGetCash(const ioHashString& szUserIndex, const ioHashString& szUserID, const ioHashString& szPublicID, int& iRealCash, int& iPurchase, int& iResult, ioHashString& szErrString )
{
	try 
	{
		IIPGClientPtr ptrClient;
		ptrClient.CreateInstance(__uuidof(IPGClient));

		_variant_t vsUserNo, vsUserID, vsNickName, vsCompanyCD; // input
		_variant_t vsRemainCashShop, vsRemainCashContents, vsRemainCashBonus,
			vsRemainCashEtc, vsRemainMileageShop, vaRemainMileageContents;// output

		vsUserNo    = (LPSTR) szUserIndex.c_str(); 
		
//		vsUserID    = (LPSTR) "valofe_qa1";
		vsUserID    = (LPSTR) szUserID.c_str();
		vsNickName  = (LPSTR) szPublicID.c_str();
		vsCompanyCD = (LPSTR) m_szCompanyCD.c_str(); 

		// 값을 받을때까지 대기
		_variant_t vsResult = ptrClient->GetCash( &vsUserNo, &vsUserID, &vsCompanyCD, &vsNickName
							,&vsRemainCashShop,&vsRemainCashContents ,&vsRemainCashBonus
				            ,&vsRemainCashEtc,&vsRemainMileageShop, &vaRemainMileageContents); 
		char szResult[MAX_PATH] = "";
		WTOM( vsResult.bstrVal, szResult );

		iPurchase = 0;
		if (strcmp(szResult,"0000") == 0)
		{
			iResult = CASH_RESULT_SUCCESS;
			iPurchase += vsRemainCashShop.lVal;
			iPurchase += vsRemainCashContents.lVal;
			iPurchase += vsRemainCashBonus.lVal;
			iPurchase += vsRemainCashEtc.lVal;
			iPurchase += vsRemainMileageShop.lVal;
			iPurchase += vaRemainMileageContents.lVal;
		}
		else
		{
			iResult = CASH_RESULT_EXCEPT;
			BillingItemLOG.PrintTimeAndLog(0, "%s is Error:%s:%s:PrivateID %s:%s", __FUNCTION__, szUserIndex.c_str(), szPublicID.c_str(), szUserID.c_str(), szResult);
		}
				
		iRealCash = iPurchase;
	} 
	catch (const _com_error &e )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "%s Come Exception: %s:%s:%s:%s:%d", __FUNCTION__, szUserIndex.c_str(), szUserID.c_str(), szPublicID.c_str(), (char*) e.ErrorMessage(), GetLastError()  );
		return FALSE;
	}
	catch ( ... )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "%s Come Exception: %s:%s:%s:%d",__FUNCTION__, szUserIndex.c_str(), szUserID.c_str(), szPublicID.c_str(), GetLastError()  );
		return FALSE;
	}

	return TRUE;
}
#endif // VALOFE_NEW_BILLING_SYS_SYH

#ifdef VALOFE_NEW_BILLING_SYS_SYH
BOOL ioChannelingNodeValofe::ValofeGetCashParse( const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString )
{
	try 
	{
		cJSON *pJson = NULL;
		pJson=cJSON_Parse(szReturnData);

		if( pJson == NULL )
			return FALSE;

		BOOL bState =FALSE;
		int iTotalCash = 0;

		for (BYTE i = 0; i < cJSON_GetArraySize(pJson); i++)
		{
			cJSON *pObject = cJSON_GetArrayItem( pJson, i);

			if( pObject && pObject->string )
			{
				if(strcmp("code", pObject->string) == 0)
				{
					if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
						bState = TRUE;
				}
				else if(strcmp("total_balance", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						iTotalCash = pObject->valueint;
					}
				}
				else if(strcmp("charg1_balance", pObject->string) == 0)
				{
					if(bState == TRUE)
					{
						iRealCash = pObject->valueint;
					}
				}
				else if(strcmp("charg2_balance", pObject->string) == 0)
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

BOOL ioChannelingNodeValofe::ValofePurchaseItem(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult)
{
	ioRestAPI ioRest;
	char szFullURL[MAX_PATH*2] = {0,};
	char szHeader[MAX_PATH] = {0,};
	std::string szTempString;
	ioHashString szJsonData;

	StringCbPrintf(szFullURL, sizeof(szFullURL), "%s", m_sPurchaseItemURL.c_str());

	BOOL bResult1 = ValofePurchaseParam(szUserID, rData, szErrString, szJsonData);
	if(bResult1 == FALSE)
	{
		szErrString = "Invalid Product";
		return FALSE;
	}

	///////////////
	char szReturnData[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnData, WEB_BUFF_SIZE );

	ioHTTP Winhttp; //kyg 확인 필요  POST방식 되는지 확인 해야함 
	BOOL bResult2 = Winhttp.GetResultData( m_sPurchaseItemURL.c_str(), szJsonData.c_str(), szReturnData, WEB_BUFF_SIZE );
	if( bResult2 == FALSE )
	{
		LOG.PrintTimeAndLog(0,"[error][valofe] GetResultData Error :%s %s %d", szReturnData, szUserID.c_str() );
		return TRUE;
	}

	if(ValofePurchaseParse(szReturnData, szBuyNo, rData, szErrString) == TRUE)
	{
		LOG.PrintTimeAndLog(0,"[Success][valofe] ValofePurchaseItem Return Test :%s JSon-%s UserID-%s GoodsNo-%d PayAmout-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetItemPayAmt());
		return TRUE;
	}
	else
		LOG.PrintTimeAndLog(0,"[error][valofe] ValofePurchaseItem Return Error :%s JSon-%s UserID-%s GoodsNo-%d", szReturnData, szJsonData.c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo());

	return FALSE;
}

BOOL ioChannelingNodeValofe::ValofePurchaseParse( const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString )
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
				if(strcmp("code", pObject->string) == 0)
				{
					if(pObject->valueint == VALUFE_REUTRN_SUCCESS)
					{
						bState = TRUE;
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

BOOL ioChannelingNodeValofe::ValofePurchaseParam( const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData )
{
	int cpType = 1000;
	ioHashString strUserID = rData.GetUserNo().c_str();		// 벨로프 채널링 ID

	enum { MAX_ETC_PLUS_ONE = 101, };
	char szEtc1[MAX_ETC_PLUS_ONE]="";

	char szEncodeGoodsName[USER_ID_NUM] = "";
	
	StringCbPrintf( szEtc1, sizeof( szEtc1 ), "%d_%s", rData.GetGoodsNo(), rData.GetGoodsName().c_str() );
	ioHashString szGoods = szEtc1;
	Get_Encode(szGoods, szEncodeGoodsName);

	std:string strPostData;
	strPostData = "service_code=";
	strPostData += m_sServiceCode.c_str();
	strPostData += "&channeling_type=";
	strPostData += std::to_string( static_cast<long long>(cpType));
	strPostData += "&user_id=";
#ifdef VALOFE_NEW_BILLING_TEST
	strPostData += "valofe_qa1";
#else
	strPostData += strUserID.c_str();
#endif
	strPostData += "&title=";
	strPostData += rData.GetGoodsName().c_str();
	strPostData += "&client_ip=";
	strPostData += rData.GetUserIP().c_str();
	strPostData += "&payamt=";
	strPostData += std::to_string( static_cast<long long>(rData.GetItemPayAmt()));
	strPostData += "&itemid=";
	strPostData += szEncodeGoodsName;
	strPostData += "&item_amount=";
	strPostData += "1";
	strPostData += "&memo=";
	strPostData += "";

#ifdef VALOFE_NEW_BILLING_LOG
	LOG.PrintTimeAndLog(0,"[Purchase][valofe] %s -- PostData : %s", __FUNCTION__, strPostData.c_str() );
#endif
	szJsonData = strPostData.c_str();

	return TRUE;
}
#endif // VALOFE_NEW_BILLING_SYS_SYH

void ioChannelingNodeValofe::SendExecptMessage( DWORD dwPacketID, int iErrCode, const ioData& rData, ioHashString& szErrString )
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
		BillingItemLOG.PrintTimeAndLog( 0, "[error][Valofe] ProcessType:%d Send Fail: %d:%s:%s", "ioChannelingNodeWemadeCashLink::SendExecptMessage Fail", 
			rData.GetProcessType(),
			rData.GetUserIndex(),
			rData.GetBillingGUID().c_str(),
			rData.GetUserNo().c_str() );
	}
	return;
}

#ifdef VALOFE_NEW_BILLING_SYS_SYH
void ioChannelingNodeValofe::UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize )
{
	if( szUTF8 == NULL )
		return;

	WCHAR szUni[MAX_PATH*2];
	ZeroMemory( szUni, sizeof(szUni) );

	int iUnisize = MultiByteToWideChar(CP_UTF8, 0, szUTF8, -1 , szUni, MAX_PATH*2 );
	riReturnAnsiSize = WideCharToMultiByte(CP_ACP, 0 , szUni, iUnisize, szAnsi, iAnsiBufferSize ,NULL ,NULL );
}
void ioChannelingNodeValofe::Get_Encode(const ioHashString& strMulti, char* strUtf8 ) 
{
	wchar_t strUnicode[256] = {0,}; 
	char        strMultibyte[256] = {0,}; 
	strcpy_s(strMultibyte,256,strMulti.c_str()); 
	int nLen = MultiByteToWideChar(CP_ACP, 0, strMultibyte, strlen(strMultibyte), NULL, NULL); 
	MultiByteToWideChar(CP_ACP, 0, strMultibyte, strlen(strMultibyte), strUnicode, nLen);


	nLen = WideCharToMultiByte(CP_UTF8, 0, strUnicode, lstrlenW(strUnicode), NULL, 0, NULL, NULL);
	WideCharToMultiByte (CP_UTF8, 0, strUnicode, lstrlenW(strUnicode), strUtf8, nLen, NULL, NULL);
}
#endif // VALOFE_NEW_BILLING_SYS_SYH
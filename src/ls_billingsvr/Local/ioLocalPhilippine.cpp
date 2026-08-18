#include "../stdafx.h"
#include "./ioLocalPhilippine.h"
#include "../MainProcess.h"
#include "../NodeInfo/ServerNode.h"
#include "../DataBase/DBClient.h"
#include "../DataBase/LogDBClient.h"
#include "../Channeling/ioChannelingNodeParent.h"
#include "../ThreadPool/ioThreadPool.h"
#include "../NodeInfo/ServerNodeManager.h"
#include "../EtcHelpFunc.h"
#include "../PhilippineBillingServer/PhilippineBillingServer.h"


#ifdef __OHTG_LOCAL_PHILIPPINE__
extern CLog LOG;


ioLocalPhilippine::ioLocalPhilippine(void)
{
}

ioLocalPhilippine::~ioLocalPhilippine(void)
{
}

ioLocalManager::LocalType ioLocalPhilippine::GetType()
{
	return ioLocalManager::LCT_PHILIPPINE;
}


void ioLocalPhilippine::Init()
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "Philippine" );

	char szKreonLoginURL[MAX_PATH]="";
	kLoader.LoadString( "PhilippineLoginURL", "", szKreonLoginURL, sizeof( szKreonLoginURL ) );
	m_sLoginURL = szKreonLoginURL;

	char szKreonBillingGetURL[MAX_PATH]="";
	kLoader.LoadString( "PhilippineBillingGetURL", "", szKreonBillingGetURL, sizeof( szKreonBillingGetURL ) );
	m_sBillingGetURL = szKreonBillingGetURL;

	char szKreonBillingOutPutURL[MAX_PATH]="";
	kLoader.LoadString( "PhilippineBillingOutPutURL", "", szKreonBillingOutPutURL, sizeof( szKreonBillingOutPutURL ) );
	m_sBillingOutPutURL = szKreonBillingOutPutURL;

	char szKreonPCRoomURL[MAX_PATH]="";
	kLoader.LoadString( "PhilippinePCRoomURL", "", szKreonPCRoomURL, sizeof( szKreonPCRoomURL ) );
	m_sPCRoomURL = szKreonPCRoomURL;

	if( m_sLoginURL.IsEmpty() || m_sBillingGetURL.IsEmpty() || m_sBillingOutPutURL.IsEmpty() || m_sPCRoomURL.IsEmpty() )
		LOG.PrintTimeAndLog(0,"Error Kreon URL");
	
	m_dwBillingReqKey = 0;
}



void ioLocalPhilippine::OnAutoupgradeLogin( ServerNode *pServerNode, SP2Packet &rkPacket )
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

	enum { MAX_RANDOM_KEY = 20, };
	char szRandomKey[MAX_PATH]="";
	if( sEncodePW.Length() > MAX_RANDOM_KEY )
		StringCbCopyN( szRandomKey, sizeof( szRandomKey ), sEncodePW.c_str(), MAX_RANDOM_KEY );
	char szEncode[MAX_PATH]="";
	int iEncodeCnt = 0;
	int iEncodPwLength = sEncodePW.Length();
	for (int i = 0; i < iEncodPwLength; i++)
	{
		if( i >= MAX_RANDOM_KEY )
		{
			szEncode[iEncodeCnt] = sEncodePW.At(i);
			iEncodeCnt++;
			if( iEncodeCnt >= MAX_PATH )
				break;
		}
	}
	char szPW[MAX_PATH]="";
	char szUserKey[MAX_PATH]="";
	StringCbPrintf( szUserKey, sizeof( szUserKey ), "%s%s", szPrivateID.c_str(), szRandomKey );
	Help::Decode( szEncode, strlen( szEncode ), szPW, sizeof( szPW ), szUserKey, strlen( szUserKey ) );

	ioData kData;
	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetPrivateID( szPrivateID );
	kData.SetEncodePW( szPW );
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

	g_DBClient.OnPhilippineAutoLogin(kData);
	LOG.PrintTimeAndLog(0, "Test %s pServerNode == NULL:%s:%s:%s:%s:%s:%s:%d", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str(), sEncodePW.c_str(), szPW, szPublicIP.c_str(), szMacAddress.c_str(), iChannelingType );

//	g_ThreadPool.SetData( kData );
}


void ioLocalPhilippine::OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket )
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

	enum { MAX_RANDOM_KEY = 20, };
	char szRandomKey[MAX_PATH]="";
	if( sEncodePW.Length() > MAX_RANDOM_KEY )
		StringCbCopyN( szRandomKey, sizeof( szRandomKey ), sEncodePW.c_str(), MAX_RANDOM_KEY );
	char szEncode[MAX_PATH]="";
	int iEncodeCnt = 0;
	int iEncodPwLength = sEncodePW.Length();
	for (int i = 0; i < iEncodPwLength; i++)
	{
		if( i >= MAX_RANDOM_KEY )
		{
			szEncode[iEncodeCnt] = sEncodePW.At(i);
			iEncodeCnt++;
			if( iEncodeCnt >= MAX_PATH )
				break;
		}
	}
	char szPW[MAX_PATH]="";
	char szUserKey[MAX_PATH]="";
	StringCbPrintf( szUserKey, sizeof( szUserKey ), "%s%s", szPrivateID.c_str(), szRandomKey );
	Help::Decode( szEncode, strlen( szEncode ), szPW, sizeof( szPW ), szUserKey, strlen( szUserKey ) );

	ioData kData;
	kData.SetChannelingType( 0 );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetPrivateID( szPrivateID );
	kData.SetEncodePW( szPW );
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
/*
	if(g_App.IsTestMode())
	{
		SP2Packet kPacket( dwReturnMsgType );
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szPrivateID;
		kPacket << szPrivateID;

		kPacket << szPrivateID;
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szPrivateID;
		g_ServerNodeManager.SendMessageIP( (ioHashString) kData.GetServerIP(),  kData.GetServerPort(), kPacket );

		return;	
	}
	*/
	g_DBClient.OnPhilippineLogin(kData);

	LOG.PrintTimeAndLog(0, "Test %s Complete.MsgType:%s:%s:%s:%s:%s:%s:%d", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str(), sEncodePW.c_str(), szPW, szPublicIP.c_str(), szMacAddress.c_str(), 0 );
}




void ioLocalPhilippine::_OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType   = 0;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex       = 0;
	ioHashString szPrivateID;
	ioHashString szPublicID;
	bool         bSetUserMouse     = false;
	ioHashString szBillingUserKey;
	DWORD		 dwKey				= 0;
	DWORD		 dwSiteCode			= 0;
	ioHashString szPHLMemberID;

	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> szPrivateID >> szPublicID >> bSetUserMouse; // 공통사항
	rkPacket >> szPHLMemberID >> szBillingUserKey;

	if( !pServerNode )
	{
		LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%d:%s:%s", __FUNCTION__, szBillingGUID.c_str(), dwUserIndex, szPrivateID.c_str(), szPublicID.c_str() );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "%s LogOut: %d:%s:%s:%s",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str() );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << false;
		pServerNode->SendMessage( kPacket );
		return;
	}

	//HRYOON 20150112
	if(g_App.IsTestMode())
	{
		int iCash = 0;
		TestCashManager::TestCashInfo *pInfo = m_TestCashManager.GetInfo( szPrivateID );
		if( pInfo )
		{
			m_TestCashManager.CheckNChargeCash( pInfo );
			iCash = pInfo->m_iCash;
		}
		else
		{
			m_TestCashManager.AddInfo( szPrivateID );
			TestCashManager::TestCashInfo *pInfo = m_TestCashManager.GetInfo( szPrivateID );
			if( pInfo )
			{
				iCash = pInfo->m_iCash;
			}
		}

		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_SUCCESS;
		kPacket << iCash;
		kPacket << iCash;
		pServerNode->SendMessage( kPacket );
		
		LOG.PrintTimeAndLog( 0, "[TEST GETCASH] %s Success: %d:%s:%s:gold:%d", 
			__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), iCash);

		return;	
	}
	/*
	0       성공
	450	    Stored Procedure 실행 오류입니다.
	452	    데이터베이스에 연결할 수 없습니다.
	1001	파라메터 오류입니다.
	1003	정의되지 않은 전문 명령어 요청입니다.
	1004	Socket Timeout 이 발생하였습니다.
	1006	GTX서버가 일시 중지 되었습니다.
	1007	허용되지 않은 IP 요청입니다.
	1010	GTX서버가 중지 중입니다. 클라이언트의 요청을 수락할 수 없습니다.
	1011	요청된 클라이언트 수가 GTX서버의 처리 용량을 초과하였습니다.
	1012	GTX 서버 내부 Exception 오류입니다.
	1013	요청된 전문이 정의 규약에 벗어납니다.
    */

	if(szPHLMemberID.IsEmpty())
	{
		LOG.PrintTimeAndLog( 0, "%s PHLMemberID == NULL  %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}

	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->PopBillInfo();

	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}
	dwKey = InterlockedIncrement( (LONG *)&m_dwBillingReqKey );

	pInfo->m_dwKey		  = dwKey;
	pInfo->m_szKey        = szPrivateID;
	pInfo->m_dwCreateTime = TIMEGETTIME();
	pInfo->m_eType	= BillInfoManager::AT_GET;

	pInfo->m_kPacket << dwUserIndex << szBillingGUID << bSetUserMouse << pServerNode->GetIP() << pServerNode->GetClientPort();
	pInfo->m_kPacket.SetPosBegin();
	if(!g_BillInfoManager->Add(pInfo))
	{
		LOG.PrintTimeAndLog( 0, "%s BillingInfo is full.: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}

	//스팀
	if( dwSiteCode == 1)
		dwSiteCode = 4;
	else if(dwSiteCode == 0)
		dwSiteCode = 2;

	LOG.PrintTimeAndLog( 0, "%s [Send BALANCE_REQUEST] %d:%s:%s:%s:%s:%d:%d", 	__FUNCTION__, dwUserIndex, szPHLMemberID.c_str(), szPrivateID.c_str(), szBillingUserKey.c_str(), szPublicID.c_str(), dwKey, dwSiteCode);

	GTX_PHL_PK_GETBALANCE kInfo;
	kInfo.SetInfo( dwKey, szPHLMemberID.c_str(), szPrivateID.c_str(), szPublicID.c_str() );
//	kInfo.Htonl();

	SP2Packet kPacket(BTBRBTPK_BALANCE_REQUEST);
	
	kPacket << kInfo;
	if( !g_PhilippineBillingServer.SendMessage( kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Fail Send: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		pServerNode->SendMessage( kPacket );

		g_BillInfoManager->Delete(szPrivateID);

		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
		return;
	}
	LOG.PrintTimeAndLog(0, "%s ThreadPool:%s:%s", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str());
}


void ioLocalPhilippine::OnRecieveGetCash( const GTX_PHL_PK_GETBALANCE &rkResult )
{
	ioHashString szPrivateID;
	char szTemp[MAX_PATH]="";
//	StringCbPrintf( szTemp, sizeof( szTemp ), "%u", rkResult.UserNo );
	szPrivateID = rkResult.UserID;

	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->Get(szPrivateID);

	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL. %d:%s:%d:%d:%d:%s", __FUNCTION__, rkResult.UserNo, rkResult.UserID, rkResult.RealCash, rkResult.BonusCash, rkResult.RetCode, rkResult.RetMsg );
		return;
	}

	DWORD        dwUserIndex   = 0;
	ioHashString szBillingGUID;
	bool         bSetUserMouse = false;
	ioHashString szServerIP;
	int          iClientPort   = 0;
	pInfo->m_kPacket >> dwUserIndex >> szBillingGUID >> bSetUserMouse >> szServerIP >> iClientPort;

	if( rkResult.RetCode != RESULT_SUCCESS )
	{
		ioHashString sResultMessage = rkResult.RetMsg;

		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << bSetUserMouse;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << true;
		kPacket << sResultMessage;
		if( !g_ServerNodeManager.SendMessageIP( szServerIP, iClientPort, kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail(1): %d:%s:%s:Ret %d:%d:%d", __FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(),  rkResult.RetCode, rkResult.RealCash, rkResult.BonusCash );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
			g_BillInfoManager->Delete(szPrivateID);
			return;
		}
		LOG.PrintTimeAndLog( 0, "%s Fail: %d:%s:%s:Ret %d:%s:%d:%d", __FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(),  rkResult.RetCode, rkResult.RetMsg, rkResult.RealCash, rkResult.BonusCash );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, rkResult.RetCode, "Error return value." );
		g_BillInfoManager->Delete(szPrivateID);
		return;
	}

	

	int iReturnCash    = ( rkResult.RealCash + rkResult.BonusCash );
	int iPurchasedCash = iReturnCash;


	SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
	kPacket << dwUserIndex;
	kPacket << szBillingGUID;
	kPacket << bSetUserMouse;
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << iReturnCash;
	kPacket << iPurchasedCash;
	if( !g_ServerNodeManager.SendMessageIP( szServerIP, iClientPort, kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail(2): %d:%s:%s:Ret %d:%d:%d", __FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(),  rkResult.RetCode, rkResult.RealCash, rkResult.BonusCash );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
		g_BillInfoManager->Delete(szPrivateID);
		return;
	}
	LOG.PrintTimeAndLog( 0, "%s Success: %d:%s:%s:Ret %d:%d:%d", __FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), rkResult.RetCode, rkResult.RealCash, rkResult.BonusCash );
	g_BillInfoManager->Delete(szPrivateID);
}


void ioLocalPhilippine::_OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName )
{
	int				iChannelingType = 0;
	ioHashString	szBillingGUID;
	int				iReturnItemPrice = 0;
	DWORD			dwUserIndex = 0;
	ioHashString	szPublicID;
	ioHashString	szPrivateID;
	ioHashString	szUserIP;
	int				iPayAmt  = 0;
	int				iType    = 0;
	DWORD			dwKey		= 0;
	int				iBonusCashSize	= 0;

	rkPacket >> iChannelingType >> szBillingGUID >> iReturnItemPrice >> dwUserIndex >> szPublicID >> szPrivateID >> szUserIP >> iPayAmt >> iBonusCashSize;

	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->PopBillInfo();
	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT  );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iReturnItemPrice;
		kPacket << iType;
		kPacket << false;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}


	for( int i = 0; i < iBonusCashSize; i++ )
	{
		int iIndex		= 0;
		int iBonusCash	= 0;

		rkPacket >> iIndex >> iBonusCash;
		pInfo->AddBonusCashInfoForConsume(iIndex, iBonusCash);
	}

	rkPacket >> iType; // 공통사항

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "%s LogOut: %d:%s:%s:%s",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str() );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT  );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iPayAmt;
		kPacket << iType;
		kPacket << false;
		pServerNode->SendMessage( kPacket );
		return;
	}

	ioChannelingNodeParent::ItemInfo kBuyInfo;
	ioChannelingNodeParent::GetItemInfo( rkPacket, iType, kBuyInfo );

	LOG.PrintTimeAndLog( 0, "%s OutputSend Send: %d:%d:%s:%s:%s:Price %d:%d:%s",__FUNCTION__, dwKey, dwUserIndex, szPrivateID.c_str(), szPublicID.c_str()
		                                                                                 , szUserIP.c_str(), iPayAmt, dwGoodsNo, rszGoodsName.c_str() );


	ioHashString szPHLMemberID;

	rkPacket >> szPHLMemberID; // Philippine


	//HRYOON 20150112
	if( g_App.IsTestMode() )
	{
		int iCash = 0;
		TestCashManager::TestCashInfo *testpInfo = m_TestCashManager.GetInfo( szPrivateID );
		if( testpInfo  )
		{
			m_TestCashManager.CheckNChargeCash( testpInfo  );
			testpInfo->m_iCash -= iPayAmt;
			iCash = testpInfo ->m_iCash;
		}
		else
		{
			m_TestCashManager.AddInfo( szPrivateID );
			TestCashManager::TestCashInfo *testpInfo  = m_TestCashManager.GetInfo( szPrivateID );
			if( testpInfo  )
			{
				testpInfo->m_iCash -= iPayAmt;
				iCash = testpInfo->m_iCash;
			}
		}

		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_SUCCESS;	//iReturnValue
		kPacket << iReturnItemPrice; 
		kPacket << iType;
		kPacket << iPayAmt;
		kPacket << 0; // TransactionID ( FOR Philippine )
		kPacket << "";
		ioChannelingNodeParent::SetItemInfo( kPacket, iType, kBuyInfo );
		
		// Cancel Step 1
		kPacket << iChannelingType;  // 공통
		kPacket << iCash;
		kPacket << iCash;

		static TwoOfINTVec vInfo;
		vInfo.clear();

		pInfo->GetBonusCashInfo(vInfo);
		int iiBonusCashSize	= vInfo.size();

		kPacket << iiBonusCashSize	;

		for( int i = 0; i < iiBonusCashSize	; i++ )
		{
			kPacket << vInfo[i].value1 << vInfo[i].value2;
		}


		kPacket << szPrivateID;
		kPacket << szPrivateID;		//Philippine userId
		kPacket << "";
		kPacket	<< 1; 
		//스팀용
		kPacket << 0;	//보너스 캐쉬

		pServerNode->SendMessage( kPacket );
		LOG.PrintTimeAndLog( 0, "[TEST OUTPUT CASH] %s Success: %d:%s:%s:gold:%d,payAmt:%d", 
			__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), iCash,iPayAmt );
		return;
	}

	if(szPHLMemberID.IsEmpty())
	{
		LOG.PrintTimeAndLog( 0, "%s PHLMemberID == NULL  %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iReturnItemPrice; 
		kPacket << iType;
		kPacket << false;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}

	dwKey = InterlockedIncrement( (LONG *)&m_dwBillingReqKey );
	pInfo->m_dwReqKey	  = dwKey;
	pInfo->m_szKey        = szPrivateID;
	pInfo->m_dwCreateTime = TIMEGETTIME();
	pInfo->m_eType				= BillInfoManager::AT_OUTPUT;
	pInfo->m_kPacket << dwUserIndex << szBillingGUID << iReturnItemPrice << iType << iPayAmt << iChannelingType << szUserIP;
	pInfo->m_kPacket << pServerNode->GetIP() << pServerNode->GetClientPort();
	ioChannelingNodeParent::SetItemInfo( pInfo->m_kPacket, iType, kBuyInfo );
	pInfo->m_kPacket.SetPosBegin();

	if( !g_BillInfoManager->Add(pInfo))
	{
		LOG.PrintTimeAndLog( 0, "%s BillingInfo is full.: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iReturnItemPrice; 
		kPacket << iType;
		kPacket << false;
		pServerNode->SendMessage( kPacket );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_ADD_BUYINFO,"fail add buy info." );
		return;
	}

	LOG.PrintTimeAndLog( 0, "%s OutputSend Buy: %d:%d:%s:%s:%s:%s:Price %d:%d:%s",__FUNCTION__, dwKey, dwUserIndex, szPrivateID.c_str(), szPHLMemberID.c_str(), szPublicID.c_str()
		                                                                                 , szUserIP.c_str(), iPayAmt, dwGoodsNo, rszGoodsName.c_str() );

	GTX_PHL_PK_PURCHASEITEM kInfo;
	kInfo.SetInfo( dwKey, szPHLMemberID.c_str(), szPrivateID.c_str(), 0, Help::GetStringIPToDWORDIP( szUserIP.c_str() ), dwGoodsNo, iPayAmt, rszGoodsName.c_str() );
//	kInfo.Htonl();

	SP2Packet kPacket( BTPHBTPK_BUY_REQUEST );
	kPacket << kInfo;
	if( !g_PhilippineBillingServer.SendMessage( kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Fail Send:MemberType : %d, PhilippineMemberID:%s, PhilippineMemberID :%s, %d:%s:%s:%s:%d",
						__FUNCTION__, 1, szPrivateID.c_str(), szPHLMemberID.c_str(), dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iReturnItemPrice;
		kPacket << iType;
		kPacket << false;
		pServerNode->SendMessage( kPacket );

		g_BillInfoManager->Delete(szPrivateID);
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
		return;
	}

	LOG.PrintTimeAndLog( 0, "%s OutputSend Success: %d:%s:%s:%s:%s:Price %d:%d:%s",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str()
		                                                                                 , szUserIP.c_str(), iPayAmt, dwGoodsNo, rszGoodsName.c_str() );
}

void ioLocalPhilippine::OnRecieveOutputCash( const GTX_PHL_PK_PURCHASEITEM& rkResult )
{
	ioHashString szPrivateID;
	char szTemp[MAX_PATH]="";
	szPrivateID = rkResult.UserID;

	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->Get(szPrivateID);
	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL. %s-%d:%s:%d:%d:%d:%s[%s]", __FUNCTION__, szPrivateID, rkResult.UserNo, rkResult.UserID, rkResult.RealCash, rkResult.BonusCash, rkResult.RetCode, rkResult.RetMsg, rkResult.ChargeNo );
		return;
	}

	DWORD        dwUserIndex		= 0;
	ioHashString szBillingGUID;
	int          iReturnItemPrice	= 0;
	int          iType				= 0;
	int          iPayAmt			= 0;
	int          iChannelingType	= 0;
	ioHashString szUserIP;
	ioHashString szServerIP;
	int          iClientPort		= 0;
	int			 iPhilippineMemberType		= 0;
	pInfo->m_kPacket >> dwUserIndex >> szBillingGUID >> iReturnItemPrice >> iType >> iPayAmt >> iChannelingType >> szUserIP >> szServerIP >> iClientPort;

	if( rkResult.RetCode != RESULT_SUCCESS )
	{
		ioHashString sResultMessage = rkResult.RetMsg;

		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << iReturnItemPrice;
		kPacket << iType;
		kPacket << true;
		kPacket << sResultMessage;

		if( !g_ServerNodeManager.SendMessageIP( szServerIP, iClientPort, kPacket ) )
		{
			g_BillInfoManager->Delete(szPrivateID);
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s:Ret %d:%d:%d[%s]", __FUNCTION__, rkResult.UserNo, szBillingGUID.c_str(), rkResult.UserID, rkResult.RetCode
				                                                                           , rkResult.RealCash, rkResult.BonusCash, rkResult.ChargeNo );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_DISCONNECT_GAMESERVER,"disconnect game server." );
			return;
		}

		g_BillInfoManager->Delete(szPrivateID);
		LOG.PrintTimeAndLog( 0, "%s Fail: %d:%s:%s:Ret %d:%s:%d:%d[%s]", __FUNCTION__, rkResult.UserNo, szBillingGUID.c_str(), rkResult.UserID,  rkResult.RetCode
			                                                                         , rkResult.RetMsg, rkResult.RealCash, rkResult.BonusCash, rkResult.ChargeNo );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, rkResult.RetCode,"Error return value." );
		return;
	}

	int iReturnCash    = ( rkResult.RealCash + rkResult.BonusCash );
	int iPurchasedCash = iReturnCash;

	SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
	kPacket << dwUserIndex;
	kPacket << szBillingGUID;
	kPacket << CASH_RESULT_SUCCESS;	//iReturnValue
	kPacket << iReturnItemPrice;
	kPacket << iType;
	kPacket << rkResult.ChargedCashAmt;
	kPacket << 0; // TransactionID ( FOR Philippine )
	kPacket << "";
	ioChannelingNodeParent::ItemInfo kItemInfo;
	ioChannelingNodeParent::GetItemInfo( pInfo->m_kPacket, iType, kItemInfo );
	ioChannelingNodeParent::SetItemInfo( kPacket, iType, kItemInfo );

	ioHashString sBillingUserKey = rkResult.UserID;
	ioHashString sChargeNo       = rkResult.ChargeNo;

	// Cancel Step 1
	kPacket << iChannelingType;  // 공통
	kPacket << iReturnCash;
	kPacket << iPurchasedCash;
	
	static TwoOfINTVec vInfo;
	vInfo.clear();

	pInfo->GetBonusCashInfo(vInfo);
	int iiBonusCashSize	= vInfo.size();

	kPacket << iiBonusCashSize	;

	for( int i = 0; i < iiBonusCashSize	; i++ )
	{
		kPacket << vInfo[i].value1 << vInfo[i].value2;
	}

	kPacket << szPrivateID;
	kPacket << sBillingUserKey;		//Philippine userId
	kPacket << sChargeNo;
	kPacket	<< iPhilippineMemberType; 
	//스팀용
	kPacket << rkResult.BonusCash;	//보너스 캐쉬

	if( !g_ServerNodeManager.SendMessageIP( szServerIP, iClientPort, kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s:Ret %d:%d:%d[%s]", __FUNCTION__, rkResult.UserNo, szBillingGUID.c_str(), rkResult.UserID,  rkResult.RetCode, rkResult.RealCash, rkResult.BonusCash, rkResult.ChargeNo );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_DISCONNECT_GAMESERVER,"disconnect game server." );
		g_BillInfoManager->Delete(szPrivateID);
		return;
	}

	g_BillInfoManager->Delete(szPrivateID);
	LOG.PrintTimeAndLog( 0, "%s Success: %d:%s:%s:Ret %d:%d:%d:%d[%s]", __FUNCTION__, rkResult.UserNo, szBillingGUID.c_str(), rkResult.UserID,  rkResult.RetCode
		                                                                            , rkResult.RealCash, rkResult.BonusCash, rkResult.ChargedCashAmt, rkResult.ChargeNo );
}

void ioLocalPhilippine::OnCancelCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType	= 0;
	DWORD		 dwReqKey			= 0;
	DWORD        dwUserIndex		= 0;
	WORD		 siteCode			= 0;
	
	DWORD		 RealCash			= 0;
	DWORD		 BonusCash			= 0;
	DWORD		 CanceledCashAmt	= 0;


	ioHashString szPrivateID;
	ioHashString szPHLMemberID;
	ioHashString szChargeNo;
	ioHashString szBillingGUID;

	// 공통
	rkPacket >> iChannelingType; 
	rkPacket >> szBillingGUID; 
	rkPacket >> dwUserIndex; 

	// Cancel Step 4
	rkPacket >> szPrivateID;
	rkPacket >> szPHLMemberID;
	rkPacket >> szChargeNo;
	rkPacket >> siteCode;
	//Philippine 인경우
	rkPacket >> RealCash;
	rkPacket >> BonusCash;
	rkPacket >> CanceledCashAmt;

	if(siteCode == 1)
		siteCode = 4;	//스팀사용자
	else if( siteCode == 0)	
		siteCode = 1;	//일반사용자

	LOG.PrintTimeAndLog( 0, "%s Send: Index:%d, GUID:%s, PrivateID:%s, ChargeNo:%s, PhilippineID:%s, PrevCash:%d, PrevBonusCash:%d,CanceledCash:%d",
							__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szChargeNo.c_str(), szPHLMemberID.c_str(),  RealCash, BonusCash, CanceledCashAmt );

	GTX_PHL_PK_CNLPURCHASE kInfo;
	kInfo.SetInfo( dwReqKey, szPrivateID.c_str(), szPHLMemberID.c_str(), szChargeNo.c_str(),RealCash, BonusCash ,CanceledCashAmt );
	kInfo.Htonl();

	SP2Packet kPacket( BTPHBTPK_CANCEL_REQUEST );
	kPacket << kInfo;
	if( !g_PhilippineBillingServer.SendMessage( kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Fail Send: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szChargeNo.c_str(), GetLastError()  );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
		return;
	}
}

void ioLocalPhilippine::OnRecieveCancelCash( const GTX_PHL_PK_CNLPURCHASE &rkResult )
{
}

void ioLocalPhilippine::OnLogoutLog( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szprivateID;
	szprivateID.Clear();

	rkPacket >> szprivateID;

	LOG.PrintTimeAndLog( 0, "[TEST LOGOUT] ioLocalPhilippine::OnLogoutLog ID:%s", szprivateID.c_str() );
	
	if(g_App.IsTestMode())
	{
		int iCash = 0;
		TestCashManager::TestCashInfo *pInfo = m_TestCashManager.GetInfo( szprivateID );
		if( pInfo )
		{
			m_TestCashManager.DelInfo( szprivateID );
		}
	}
}
#endif //__OHTG_LOCAL_PHILIPPINE__
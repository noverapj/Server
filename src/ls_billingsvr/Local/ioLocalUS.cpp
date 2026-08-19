#include "../stdafx.h"
#include "./iolocalus.h"
#include "../NodeInfo/ServerNode.h"
#include "../ThreadPool/ioThreadPool.h"
#include "../DataBase/LogDBClient.h"
#include "../Channeling/ioChannelingNodeParent.h"
#include "../NodeInfo/ServerNodeManager.h"
#include "../MainProcess.h"
#include "../EtcHelpFunc.h"
#include "../USBillingServer/USBillingServer.h"
#include "../USAuthServer/USAuthServer.h"
#include "../NodeInfo/MemInfoManager.h"
#include "../xml/ioxmldocument.h"
#include "../xml/ioxmlelement.h"
#include <bcrypt.h>
#include <atltime.h>

#define SHA256_DIGEST_LENGTH 32

extern CLog LOG;

ioLocalUS::ioLocalUS(void)
{
	m_sRedeemURL.Clear();
	m_sHashKey.Clear();
	m_sFillCashURL.Clear();
	m_HashStep = 3;
}

ioLocalUS::~ioLocalUS(void)
{
}

ioLocalManager::LocalType ioLocalUS::GetType()
{
	return ioLocalManager::LCT_US;
}

void ioLocalUS::Init()
{
	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "US" );

	char szCashURL[MAX_PATH]="";
	kLoader.LoadString( "USFillCashURL", "", szCashURL, sizeof( szCashURL ) );
	m_sFillCashURL = szCashURL;

	if( m_sFillCashURL.IsEmpty() )
		LOG.PrintTimeAndLog(0,"Error FillCash URL Error");


	char szRedeemURL[MAX_PATH]="";
	kLoader.LoadString( "USRedeemURL", "", szRedeemURL, sizeof( szRedeemURL ) );
	m_sRedeemURL = szRedeemURL;

	if( m_sRedeemURL.IsEmpty() )
		LOG.PrintTimeAndLog(0,"Error Redeem URL Error");

	char szHashKey[MAX_PATH]="";
	kLoader.LoadString( "USHashKey", "", szHashKey, sizeof( szHashKey ) );
	m_sHashKey = szHashKey;

	m_dwAuthReqKey = 0;
	m_dwAuthReqKey		= 0;	
	m_dwBillingReqKey	= 0;
	

}
void ioLocalUS::GetHashString( const ioHashString& szMemberUID, int iBillingType, const int dwUserIndex, const ioHashString& szBillingGUID, const ioHashString& serverIP, int serverPort)
{
	/*
	충전을 클릭하면 게임서버 -> 빌링 -> 페이레터 전달
	EY | member_id(USN)| TIME | KEY 형태로 구성해서 SHA256을 통해서 Auth HASH Key 생성
	생성을 위한 KEY 값은 LM2d3k54pU 를 이용한다.
	TIME 값은 현재 시간의 3분 단위값을 이용한다. 3/6/9/12/15 예 23:53:15 -> 23:54, 23:59:00 -> 24:00
	23:54|1234567|23:54|LM2d3k54pU -> d5cae0acca05b4d352b953dd96e62094deee759bc6c016471a42b9b5639628df

	*/
	enum { DAY_SEC = 86400, HOUR_SEC = 3600, MINUTE_SEC = 60, };
	
	CTime cCurTime = CTime::GetCurrentTime();

	int nAdd = (cCurTime.GetMinute() % 3);

	switch(nAdd)
	{
	case 0:
		break;
	case 1:
		nAdd = 2;
		break;
	case 2:
		nAdd = 1;
		break;
	}

	CTimeSpan cAddMin( 0, 0, nAdd, 0 );
	cCurTime += cAddMin;

	time_t kCurServerTime = TIMEGETTIME();

	kCurServerTime = kCurServerTime / 1000;
	

	int minTime = (kCurServerTime % HOUR_SEC) / MINUTE_SEC;
	int additionMin = minTime % m_HashStep;
	if(additionMin > 0)
	{
		kCurServerTime += (m_HashStep - additionMin) * MINUTE_SEC;
	}

	char szTime[MAX_PATH] = {0,};
	sprintf_s(szTime, "%02d:%02d", cCurTime.GetHour(), cCurTime.GetMinute());

	// 해쉬키
	char sourceKey[MAX_PATH] = {0,};
	sprintf_s(sourceKey, "%s|%s|%s|%s", m_sHashKey.c_str(),  szMemberUID.c_str(), szTime, m_sHashKey.c_str());
	
	
	// 해쉬값 생성
	size_t sizeHash = 0;
	char hashKey[MAX_PATH]="";
	MakeHashCode(sourceKey, hashKey);

	char szFullURL[WEB_BUFF_SIZE] = {0,};
	
	LOG.PrintTimeAndLog(0, "ioLocalUS::SourceKey : %s, GetKey:%s", sourceKey, hashKey);

	//Redeem : 0, 충전 : 1 
	if( iBillingType == 0 )
		StringCbPrintf( szFullURL, sizeof( szFullURL ), "%s?usn=%s&auth=%s" , m_sRedeemURL.c_str(), szMemberUID.c_str(), hashKey );
	else
		StringCbPrintf( szFullURL, sizeof( szFullURL ), "%s?usn=%s&auth=%s" , m_sFillCashURL.c_str(), szMemberUID.c_str(), hashKey );

	//게임서버 전달
	SP2Packet kPacket( BSTPK_FILL_CASH_URL_RESULT );
	
	
	kPacket << dwUserIndex;
	kPacket << szBillingGUID;
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << szFullURL;
	g_ServerNodeManager.SendMessageIP( (ioHashString) serverIP,  serverPort, kPacket );
	
	
	
	LOG.PrintTimeAndLog(0, "ioLocalUS::GetHashString SendURL : %s", szFullURL);
 

}
void ioLocalUS::MakeHashCode(  const char *rszCode, OUT char *szHashCode )
{
	unsigned char digest[SHA256_DIGEST_LENGTH];

	BCRYPT_ALG_HANDLE hAlg = NULL;
	BCRYPT_HASH_HANDLE hHash = NULL;
	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0)
	{
		if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0)
		{
			BCryptHashData(hHash, (PUCHAR)rszCode, (ULONG)strlen(rszCode), 0);
			BCryptFinishHash(hHash, digest, sizeof(digest), 0);
			BCryptDestroyHash(hHash);
		}
		BCryptCloseAlgorithmProvider(hAlg, 0);
	}

	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
		sprintf(&szHashCode[i*2], "%02x", (unsigned int)digest[i]);
}

void ioLocalUS::OnFillCashUrl( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szPrivateID;
	ioHashString szPublicID;
	ioHashString szBillingGUID;
	DWORD        dwUserIndex = 0;
	int		 iChannelingType = 0;
	int		 iBillingType = 0;

	szPrivateID.Clear();
	szPublicID.Clear();

	/*
	SP2Packet getPacket( BSTPK_FILL_CASH_URL );
	getPacket << "1234567";
	getPacket << "getGUID1234";	//guid
	getPacket << (int)1234;	//index
	getPacket << (int)1;	//index
	getPacket << "private";	//guid
	getPacket << "public";	//guid
	getPacket.SetPosBegin();
	*/
	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> iBillingType; // 공통사항
	rkPacket >> szPrivateID >> szPublicID; 

	//const ioHashString& szMemberUID, int iBillingType, const int dwUserIndex, const ioHashString& szBillingGUID, const ioHashString& serverIP, int serverPort
	GetHashString( szPrivateID.c_str() , iBillingType, dwUserIndex, szBillingGUID.c_str(), pServerNode->GetIP(), pServerNode->GetClientPort() );
}

void ioLocalUS::OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szBillingGUID;
	ioHashString szPrivateID;
	ioHashString sUserToken;
	ioHashString szPublicIP;
	DWORD        dwReturnMsgType = 0;
	ioHashString szUserType;

	sUserToken.Clear();

	rkPacket >> szBillingGUID >> szPrivateID >> sUserToken >> szPublicIP >> dwReturnMsgType; // 공통
	rkPacket >> szUserType; // us 오토업그레이드에서 넣어줌( USER_TYPE_NORMAL / USER_TYPE_FB )

	if( !pServerNode )
	{
		LOG.PrintTimeAndLog(0, "%s pServerNode == NULL:%s:%s:%s", __FUNCTION__, szBillingGUID.c_str(), szPrivateID.c_str(), sUserToken.c_str() );
		return;
	}

	ioData kData;
	kData.SetBillingGUID( szBillingGUID );
	kData.SetPrivateID( szPrivateID );
	kData.SetTokenKey( sUserToken );
	kData.SetReturnMsgType( dwReturnMsgType );
	kData.SetUserKey( szUserType );
	kData.SetUserIP(szPublicIP);
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_LOGIN );

	g_ThreadPool.SetData( kData );
} 

void ioLocalUS::ThreadLogin( const ioData &rData, LoginManager &rLoginMgr )
{
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog(0, "%s LogOut:%s:%s:%s", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetTokenKey().c_str() );
		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		kPacket << false;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}

	//오토업그레이드 패스
	if( rData.GetReturnMsgType() == BSTPK_AUTOUPGRADE_LOGIN_RESULT )
	{

		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		return;
	}
	if( g_App.IsTestMode() )
	{
		DWORD		 dwKey = 0;
		dwKey = InterlockedIncrement((LONG *)&m_dwAuthReqKey);
		BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->PopBillInfo();
		if( !pInfo )
		{
			LOG.PrintTimeAndLog( 0, "%s LogOut: %s:%s", "ioLocalUS::ThreadLogin", rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			return;
		}
		pInfo->m_dwKey				= dwKey;			
		pInfo->m_dwCreateTime		= TIMEGETTIME();
		pInfo->m_eType				= BillInfoManager::AT_AUTH;

		pInfo->m_kPacket << dwKey << rData.GetReturnMsgType() << rData.GetBillingGUID() << rData.GetPrivateID() << rData.GetTokenKey() << rData.GetServerIP() << rData.GetServerPort();
		pInfo->m_kPacket.SetPosBegin();

		if( !g_BillInfoManager->Add( pInfo ) )
		{
			LOG.PrintTimeAndLog( 0, "%s BillingInfo is full.: %d:%s:%s:%d", "ioLocalUS::ThreadLogin", dwKey, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), GetLastError()  );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			return;
		}
		if( rData.GetTokenKey().IsEmpty() )
		{
			LOG.PrintTimeAndLog( 0, "%s OnSendAuth Token Empty 1: %s:%s", "ioLocalUS::ThreadLogin", rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			return;
		}
		SP2Packet kPacket( BSTPK_LOGIN_RESULT );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetPrivateID();
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
		g_BillInfoManager->DeleteEUInfo( dwKey );
		
		LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::TEST LOGIN SUCCESS userID:%s, GUID:%s", rData.GetPrivateID().c_str(), rData.GetBillingGUID().c_str() );

	}
	else 
	{
		DWORD		 dwKey = 0;
		dwKey = InterlockedIncrement((LONG *)&m_dwAuthReqKey);
		BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->PopBillInfo();
		if( !pInfo )
		{
			LOG.PrintTimeAndLog( 0, "%s LogOut: %s:%s", "ioLocalUS::ThreadLogin", rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			return;
		}
		pInfo->m_dwKey				= dwKey;			
		pInfo->m_dwCreateTime		= TIMEGETTIME();
		pInfo->m_eType				= BillInfoManager::AT_AUTH;

		pInfo->m_kPacket << dwKey << rData.GetReturnMsgType() << rData.GetBillingGUID() << rData.GetPrivateID() << rData.GetTokenKey() << rData.GetServerIP() << rData.GetServerPort();
		pInfo->m_kPacket.SetPosBegin();

		if( !g_BillInfoManager->Add( pInfo ) )
		{
			LOG.PrintTimeAndLog( 0, "%s BillingInfo is full.: %d:%s:%s:%d", "ioLocalUS::ThreadLogin", dwKey, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), GetLastError()  );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			return;
		}
		if( rData.GetTokenKey().IsEmpty() )
		{
			LOG.PrintTimeAndLog( 0, "%s OnSendAuth: %s:%s Token Empty 1", "ioLocalUS::ThreadLogin", rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_BillInfoManager->DeleteEUInfo( dwKey );
				
			return;
		}
		bool bOK = false;
		//인증확인
		bOK = g_USAuthServer.OnSendAuth((char*)rData.GetTokenKey().c_str(), dwKey);
		if( bOK == false) 
		{
			LOG.PrintTimeAndLog( 0, "%s OnSendAuth: %s:%s Token Empty 2", "ioLocalUS::ThreadLogin", rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
			SP2Packet kPacket( BSTPK_LOGIN_RESULT );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_BillInfoManager->DeleteEUInfo( dwKey );
			return;
		}
		LOG.PrintTimeAndLog( 0, "%s OnSendAuth SEND: key : %d, %s:%s", "ioLocalUS::ThreadLogin", dwKey, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
	}
	/*
	if( rLoginMgr.CheckLogin( rData ) )
		return;


	enum { MAX_RANDOM_KEY = 20, };
	char szRandomKey[MAX_PATH]="";
	if( rData.GetTokenKey().Length() > MAX_RANDOM_KEY )
		StringCbCopyN( szRandomKey, sizeof( szRandomKey ), rData.GetTokenKey().c_str(), MAX_RANDOM_KEY );
	char szEncode[MAX_PATH*2]="";
	int iEncodeCnt = 0;
	int iEncodPwLength = rData.GetTokenKey().Length();
	for (int i = 0; i < iEncodPwLength; i++)
	{
		if( i >= MAX_RANDOM_KEY )
		{
			szEncode[iEncodeCnt] = rData.GetTokenKey().At(i);
			iEncodeCnt++;
			if( iEncodeCnt >= MAX_PATH*2 )
				break;
		}
	}

	char szPW[MAX_PATH*2]="";
	char szUserKey[MAX_PATH]="";
	StringCbPrintf( szUserKey, sizeof( szUserKey ), "%s%s", rData.GetPrivateID().c_str(), szRandomKey );
	Help::Decode( szEncode, strlen( szEncode ), szPW, sizeof( szPW ), szUserKey, strlen( szUserKey ) );

	char szRealLoginID[MAX_PATH]="";
	StringCbCopy( szRealLoginID, sizeof( szRealLoginID ), rData.GetPrivateID().c_str() );
	if( rData.GetPrivateID().Length() > 0 && rData.GetPrivateID().Length() < MAX_PATH )
		szRealLoginID[rData.GetPrivateID().Length()-1] = NULL; // 페이스북 숫자 아이디가 실제 privateID와 중복 될 수 있으므로 |구분자을 붙였던 것을 제거

	char szUserIP[MAX_PATH]="";
	StringCbCopy( szUserIP, sizeof( szUserIP ), rData.GetUserIP().c_str() );


	char szReturnValue[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnValue, WEB_BUFF_SIZE );

	char szFullURL[MAX_PATH*4]="";
	if( rData.GetUserKey() == USER_TYPE_NORMAL )
	{
//		StringCbPrintf( szFullURL, sizeof( szFullURL ), "%s?email=%s&password=%s&usertype=%s", m_sLoginURL.c_str(), szRealLoginID, szPW, rData.GetUserKey().c_str() );
//		StringCbPrintf(szFullURL, sizeof( szFullURL ), "%s?appid=dfg6hdfAds&userid=%s&pass=%s", m_sLoginURL.c_str(), szRealLoginID, szPW);

		StringCbPrintf(szFullURL, sizeof( szFullURL ), "%sloginLostsaga.aspx", m_sLoginURL.c_str());//, szRealLoginID, szPW);

//		https://app.z8games.com/WS/loginLostsaga.aspx?appid=dfg6hdfAds&userid=jalnan&pass=jal1092
	}
	else if( rData.GetUserKey() == USER_TYPE_FB )
	{
		StringCbPrintf( szFullURL, sizeof( szFullURL ), "%s?facebook_id=%s&access_token=%s&usertype=%s", m_sLoginURL.c_str(), szRealLoginID, szPW, rData.GetUserKey().c_str() );
	}
	else
	{
		LOG.PrintTimeAndLog( 0 , "%s Error Type %s %s", __FUNCTION__, rData.GetPrivateID().c_str(), rData.GetUserKey().c_str() );
	}
	
	if( g_App.IsTestMode() )
	{
		int iUserID = 0;
		if( strcmp( szRealLoginID, "derks2007@ioenter.com" ) == 0 )
			iUserID = 1;
		if( strcmp( szRealLoginID, "derks2008@ioenter.com" ) == 0 )
			iUserID = 2;
		if( strcmp( szRealLoginID, "derks2009@ioenter.com" ) == 0 )
			iUserID = 3;
		if( strcmp( szRealLoginID, "derks2010@ioenter.com" ) == 0 )
			iUserID = 4;
		if( strcmp( szRealLoginID, "derks2011@ioenter.com" ) == 0 )
			iUserID = 5;
		if( strcmp( szRealLoginID, "derks2012@ioenter.com" ) == 0 )
			iUserID = 6;
		if( strcmp( szRealLoginID, "derks2013@ioenter.com" ) == 0 )
			iUserID = 7;
		if( strcmp( szRealLoginID, "derks2014@ioenter.com" ) == 0 )
			iUserID = 8;
		if( strcmp( szRealLoginID, "derks2015@ioenter.com" ) == 0 )
			iUserID = 9;
		if( strcmp( szRealLoginID, "jalnan" ) == 0 )
			iUserID = 10;

		StringCbPrintf( szReturnValue, WEB_BUFF_SIZE,  "{\"result\" : 0, \"msg\" : \"Login successful\" , \"userid\":%d, \"username\":\"teset@test.com\", \"loginToken\":\"ABCDE\" }", iUserID );
	}
	else
	{
		ioHTTP Winhttp; //kyg 확인 필요 
		char szPost[MAX_PATH*2]="";
		StringCbPrintf(szPost, sizeof( szPost ), "appid=dfg6hdfAds&userid=%s&pass=%s&ipaddress=%s", 
					szRealLoginID, szPW, szUserIP);

		if( !Winhttp.GetResultData( szFullURL, szPost, szReturnValue, WEB_BUFF_SIZE ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Http Error: %s:%s:%d", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), GetLastError() );
			SP2Packet kPacket( rData.GetReturnMsgType()  );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Login exception HTTP." );
			return;
		}
	
	}

	if( g_App.IsTestMode())
	{
		char szInsertValue[MAX_PATH*2]="";

		enum { MAX_INSERT_VALUE = 3, };

		ioHashStringVec vValueVec;
		vValueVec.reserve(MAX_INSERT_VALUE);
		vValueVec.push_back( szRealLoginID );
		vValueVec.push_back( szRealLoginID );
		vValueVec.push_back( szRealLoginID );

		for (int i = 0; i < MAX_INSERT_VALUE ; i++)
		{
			StringCbCat( szInsertValue, sizeof( szInsertValue ), vValueVec[i].c_str() ); // userid, loginToken, username
			StringCbCat( szInsertValue, sizeof( szInsertValue ), "|" );
		}

		if( !rLoginMgr.InsertLogin( rData, szInsertValue, 0, false, LoginInfo::PT_ENCODE_LOGIN_PW ) ) 
			return;

		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szInsertValue;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );

		LOG.PrintTimeAndLog( 0, "%s %s [%s %s %s %s ]", __FUNCTION__, rData.GetPrivateID().c_str(), vValueVec[0].c_str(), vValueVec[1].c_str(), vValueVec[2].c_str(), szInsertValue );
	}
	else
	{
		ioXMLDocument xmlDoc;
		if( !xmlDoc.LoadFromMemory( szReturnValue ) )
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			LOG.PrintTimeAndLog(0, "%s Fail ioXMLDocument::LoadFromMemory :%d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
			LOG.PrintTimeAndLog(0, "%s Fail ReturnValue :%s", __FUNCTION__, szReturnValue );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Billing exception taiwan." );
			return;
		}

		ioXMLElement xRootElement = xmlDoc.GetRootElement();
		if( xRootElement.IsEmpty() )	
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			LOG.PrintTimeAndLog(0, "%s ioXMLElement is Empty. :%d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
			LOG.PrintTimeAndLog(0, "%s Fail ReturnValue :%s", __FUNCTION__, szReturnValue );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Billing exception taiwan." );
			return;
		}

		
			//	<Z8RetMsg xmlns="http://schemas.datacontract.org/2004/07/WS" 
			//	xmlns:i="http://www.w3.org/2001/XMLSchema-instance">
			//	<data>412f4ef1-3042-4169-b5fd-c70b22a99796</data>
			//	<errorcode>0</errorcode>
			//	<status>true</status>
			//	</Z8RetMsg>

			//	-110    Internal IP Only
			//			We are currently under maintenance. Please try again at a later time.

			//	-120	Invalid Auth Infomation		
			//			Please check your login credentials and try again.

			//	-150	Auth Token is expired   
			//			Please check your login credentials and try again.

			//	-200	CBT User Only Access
			//			You have not signed up for Closed Beta Testing

			//			¿¡AO ½a RestAPI for Lostsaga v1.1.pptx Au°iCI¼A ¤¶¤²
			//	-300
			//	-400
			//	-500
			//	-1001   Your ID/Password is wrong   
			//			Please check your login credentials and try again.

			//

		// xml parsing
		int iReturnCode = -10000;
		ioXMLElement xChildElement = xRootElement.FirstChild( "errorcode" );
		if( !xChildElement.IsEmpty() && xChildElement.GetText() != NULL )
			iReturnCode = atoi( xChildElement.GetText() );

		ioHashString sReturnText;
		xChildElement = xRootElement.FirstChild( "data" );
		if( !xChildElement.IsEmpty() && xChildElement.GetText() != NULL )
			sReturnText = xChildElement.GetText();

		if( iReturnCode != 0 )
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << true;

			char szError[MAX_PATH]="";
			StringCbPrintf( szError, sizeof( szError ), "%s(%d)", sReturnText.c_str(), iReturnCode );
			kPacket << szError;

			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Login Return Error." );
			LOG.PrintTimeAndLog(0, "%s Return Error.:%s:%s:%s[%s:%s:%s]", 
									__FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), szError, szPW, szUserKey, rData.GetTokenKey().c_str() );
			return;
		}


		char szFullURLGet[MAX_PATH*2]="";
		StringCbPrintf( szFullURLGet, sizeof(szFullURLGet), "http://api.z8games.com/ws/lostsaga.svc/getuserinfo/dfg6hdfAds/%s" , sReturnText.c_str() );

		ioHTTP Winhttp;
		strcpy_s(szReturnValue, "");

		if( !Winhttp.GetResultData( szFullURLGet, "GET", szReturnValue, WEB_BUFF_SIZE ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Http Error: %s:%s:%d", __FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), GetLastError() );
			SP2Packet kPacket( rData.GetReturnMsgType()  );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << false;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Login exception HTTP." );
			return;
		}

		ioXMLDocument xmlDocGetUserInfo;
		if( !xmlDocGetUserInfo.LoadFromMemory( szReturnValue ) )
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			LOG.PrintTimeAndLog(0, "%s Fail GetUserInfo :%d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
			LOG.PrintTimeAndLog(0, "%s Fail ReturnValue :%s", __FUNCTION__, szReturnValue );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Billing exception taiwan." );
			return;
		}

		ioXMLElement xRootElementUser = xmlDocGetUserInfo.GetRootElement();
		if( xRootElementUser.IsEmpty() )	
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			LOG.PrintTimeAndLog(0, "%s GetUserInfo is Empty. :%d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
			LOG.PrintTimeAndLog(0, "%s Fail ReturnValue :%s", __FUNCTION__, szReturnValue );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Billing exception taiwan." );
			return;
		}


		// xml parsing
		iReturnCode = -10000;
		xChildElement = xRootElementUser.FirstChild( "ErrCode" );
		if( !xChildElement.IsEmpty() && xChildElement.GetText() != NULL )
			iReturnCode = atoi( xChildElement.GetText() );

		ioHashString sRtnUserNo;
		xChildElement = xRootElementUser.FirstChild( "UserNo" );
		if( !xChildElement.IsEmpty() && xChildElement.GetText() != NULL )
			sRtnUserNo = xChildElement.GetText();

		ioHashString sRtnUserID;
		xChildElement = xRootElementUser.FirstChild( "UserID" );
		if( !xChildElement.IsEmpty() && xChildElement.GetText() != NULL )
			sRtnUserID = xChildElement.GetText();

		if( iReturnCode != 0 )
		{
			SP2Packet kPacket( rData.GetReturnMsgType() );
			kPacket << rData.GetPrivateID();
			kPacket << rData.GetBillingGUID();
			kPacket << BILLING_LOGIN_RESULT_FAIL;
			kPacket << true;

			char szError[MAX_PATH]="";
			StringCbPrintf( szError, sizeof( szError ), "GetUserInfo %s(%d)", sReturnText.c_str(), iReturnCode );
			kPacket << szError;

			g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );
			g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_EXCEPTION,"Login Return Error." );
			LOG.PrintTimeAndLog(0, "%s Return Error.:%s:%s:%s[%s:%s:%s]", 
								__FUNCTION__, rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), szError, szPW, szUserKey, rData.GetTokenKey().c_str() );
			return;
		}

		char szInsertValue[MAX_PATH*2]="";

		enum { MAX_INSERT_VALUE = 3, };

		ioHashStringVec vValueVec;
		vValueVec.reserve(MAX_INSERT_VALUE);
		vValueVec.push_back( sRtnUserNo );
		vValueVec.push_back( sReturnText );
		vValueVec.push_back( sRtnUserID );

		for (int i = 0; i < MAX_INSERT_VALUE ; i++)
		{
			StringCbCat( szInsertValue, sizeof( szInsertValue ), vValueVec[i].c_str() ); // userid, loginToken, username
			StringCbCat( szInsertValue, sizeof( szInsertValue ), "|" );
		}

		if( !rLoginMgr.InsertLogin( rData, szInsertValue, 0, false, LoginInfo::PT_ENCODE_LOGIN_PW ) ) 
			return;

		SP2Packet kPacket( rData.GetReturnMsgType() );
		kPacket << rData.GetPrivateID();
		kPacket << rData.GetBillingGUID();
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szInsertValue;
		g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket );

		LOG.PrintTimeAndLog( 0, "RESULT_SUCCESS %s %s [%s %s %s %s ]", __FUNCTION__, rData.GetPrivateID().c_str(), vValueVec[0].c_str(), vValueVec[1].c_str(), vValueVec[2].c_str(), szInsertValue );
	}
	*/
	
}

void ioLocalUS::_OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	bool         bSetUserMouse     = false;

	DWORD        dwUserIndex        = 0;
	DWORD		 dwKey				= 0;
	DWORD		 dwSiteCode			= 0;
	int          iChannelingType    = 0;

	ioHashString szUSMemberID;
	ioHashString szBillingGUID;
	ioHashString szPrivateID;
	ioHashString szPublicID;

	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> szPrivateID >> szPublicID >> bSetUserMouse; // 공통사항
	rkPacket >> szUSMemberID >> dwSiteCode; // Wemade USA

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
		dwSiteCode = 1;

	GTX_PK_GETBALANCE kInfo;
	kInfo.SetInfo( dwKey, (DWORD)dwSiteCode, szPrivateID.c_str(), szUSMemberID.c_str(), szPublicID.c_str() );
	kInfo.Htonl();

	SP2Packet kPacket(BTUBTPK_BALANCE_REQUEST);
	
	kPacket << kInfo;
	if( !g_USBillingServer.SendMessage( kPacket ) )
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
}

void ioLocalUS::OnRecieveGetCash( const GTX_PK_GETBALANCE &rkResult )
{
	ioHashString szPrivateID;
	char szTemp[MAX_PATH]="";
	StringCbPrintf( szTemp, sizeof( szTemp ), "%u", rkResult.UserNo );
	szPrivateID = szTemp;

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
	int iPurchasedCash = rkResult.RealCash;


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



void ioLocalUS::_OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName )
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

	int          iGameServerPort = 0;
	DWORD		 dwMemberType = 0;
	ioHashString szUSMemberID;

	rkPacket >> iGameServerPort;  // us
	rkPacket >> dwMemberType;	  //스팀 구별용
	rkPacket >> szUSMemberID; // us


	//HRYOON 20150112
	if( g_App.IsTestMode() )
	{
		if(dwMemberType == 1)
			dwMemberType = 4;	//스팀사용자
		else if( dwMemberType == 0)	
			dwMemberType = 1;	//일반사용자

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
		kPacket << 0; // TransactionID ( FOR US )
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
		kPacket << szPrivateID;		//US userId
		kPacket << "";
		kPacket	<< dwMemberType; 
		//스팀용
		kPacket << 0;	//보너스 캐쉬

		pServerNode->SendMessage( kPacket );
		LOG.PrintTimeAndLog( 0, "[TEST OUTPUT CASH] %s Success: %d:%s:%s:gold:%d,payAmt:%d", 
			__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), iCash,iPayAmt );
		return;

		/*
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
		kPacket << dwUserIndex;
		kPacket << szBillingGUID;
		kPacket << iReturn;
		kPacket << iType;

		// return value
		if( iReturn == CASH_RESULT_SUCCESS )
		{
		kPacket << iPayAmt;
		kPacket << 0; // TransactionID ( FOR US )
		kPacket << szChargeNo;

		ItemInfo kItemInfo;
		GetItemInfo( pInfo->m_kPacket, iType, kItemInfo );
		SetItemInfo( kPacket, iType, kItemInfo );

		kPacket << iChannelingType;  // 공통
		kPacket << rkResult.m_iMCash;
		kPacket << rkResult.m_iPCash;
		}
		*/
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
	if(dwMemberType == 1)
		dwMemberType = 4;	//스팀사용자
	else if( dwMemberType == 0)	
		dwMemberType = 1;	//일반사용자

	GTX_PK_PURCHASEITEM kInfo;
	kInfo.SetInfo( dwKey, (WORD)dwMemberType, szPrivateID.c_str(), szUSMemberID.c_str(), szPublicID.c_str(), Help::GetStringIPToDWORDIP( szUserIP.c_str() ), (WORD)iGameServerPort, dwGoodsNo, iPayAmt, rszGoodsName.c_str() );
	kInfo.Htonl();

	SP2Packet kPacket( BTUBTPK_BUY_REQUEST );
	kPacket << kInfo;
	if( !g_USBillingServer.SendMessage( kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Fail Send:MemberType : %d, usMemberID:%s, usMemberID :%s, %d:%s:%s:%s:%d",
						__FUNCTION__, dwMemberType, szPrivateID.c_str(), szUSMemberID.c_str(), dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str(), GetLastError()  );
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

	LOG.PrintTimeAndLog( 0, "%s Send Success: %d:%s:%s:%s:%s:Price %d:%d:%s",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szPublicID.c_str()
		                                                                                 , szUserIP.c_str(), iPayAmt, dwGoodsNo, rszGoodsName.c_str() );
}

void ioLocalUS::OnRecieveOutputCash( const GTX_PK_PURCHASEITEM& rkResult )
{
	ioHashString szPrivateID;
	char szTemp[MAX_PATH]="";
	StringCbPrintf( szTemp, sizeof( szTemp ), "%u", rkResult.UserNo );
	szPrivateID = szTemp;

	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->Get(szPrivateID);
	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL. %d:%s:%d:%d:%d:%s[%s]", __FUNCTION__, rkResult.UserNo, rkResult.UserID, rkResult.RealCash, rkResult.BonusCash, rkResult.RetCode, rkResult.RetMsg, rkResult.ChargeNo );
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
	int			 iUSMemberType		= 0;
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
	int iPurchasedCash = rkResult.RealCash;
	iUSMemberType = rkResult.SiteCode;

	if(iUSMemberType == 4)
		iUSMemberType = 1;	//스팀사용자
	else if( iUSMemberType == 1)	
		iUSMemberType = 0;	//일반사용자

	SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
	kPacket << dwUserIndex;
	kPacket << szBillingGUID;
	kPacket << CASH_RESULT_SUCCESS;	//iReturnValue
	kPacket << iReturnItemPrice;
	kPacket << iType;
	kPacket << rkResult.ChargedCashAmt;
	kPacket << 0; // TransactionID ( FOR US )
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
	kPacket << sBillingUserKey;		//US userId
	kPacket << sChargeNo;
	kPacket	<< iUSMemberType; 
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

void ioLocalUS::OnCancelCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType	= 0;
	DWORD		 dwReqKey			= 0;
	DWORD        dwUserIndex		= 0;
	WORD		 siteCode			= 0;
	
	DWORD		 RealCash			= 0;
	DWORD		 BonusCash			= 0;
	DWORD		 CanceledCashAmt	= 0;


	ioHashString szPrivateID;
	ioHashString szUSMemberID;
	ioHashString szChargeNo;
	ioHashString szBillingGUID;

	// 공통
	rkPacket >> iChannelingType; 
	rkPacket >> szBillingGUID; 
	rkPacket >> dwUserIndex; 

	// Cancel Step 4
	rkPacket >> szPrivateID;
	rkPacket >> szUSMemberID;
	rkPacket >> szChargeNo;
	rkPacket >> siteCode;
	//US 인경우
	rkPacket >> RealCash;
	rkPacket >> BonusCash;
	rkPacket >> CanceledCashAmt;

	if(siteCode == 1)
		siteCode = 4;	//스팀사용자
	else if( siteCode == 0)	
		siteCode = 1;	//일반사용자

	GTX_PK_CNLPURCHASE kInfo;
	kInfo.SetInfo( dwReqKey, siteCode, szPrivateID.c_str(), szUSMemberID.c_str(), szPrivateID.c_str(), szChargeNo.c_str(),RealCash,BonusCash ,CanceledCashAmt );
	kInfo.Htonl();

	SP2Packet kPacket( BTUBTPK_CANCEL_REQUEST );
	kPacket << kInfo;
	if( !g_USBillingServer.SendMessage( kPacket ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Fail Send: %d:%s:%s:%s:%d",__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szChargeNo.c_str(), GetLastError()  );
		g_LogDBClient.OnInsertBillingServerError( CNT_WEMADEBUY, BILLING_ERROR_LOG_FAIL_SEND_WEMADE_BILLINGSERVER,"fail send billingserver." );
		return;
	}

	LOG.PrintTimeAndLog( 0, "%s Send: Index:%d, GUID:%s, PrivateID:%s, ChargeNo:%s, USID:%s, PrevCash:%d, PrevBonusCash:%d,CanceledCash:%d",
							__FUNCTION__, dwUserIndex, szBillingGUID.c_str(), szPrivateID.c_str(), szChargeNo.c_str(), szUSMemberID.c_str(),  RealCash, BonusCash, CanceledCashAmt );
}

void ioLocalUS::OnReceiveUSAuth( DWORD memberId, CHAR* userId, int result, int identity )
{
	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->GetEUInfo( identity );
	if( !pInfo )
	{
		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL. key:%d memberID:%d", __FUNCTION__, identity, memberId );
		return;
	}
	bool         bSetUserMouse = false;
	int          iPort   = 0;
	DWORD		 dwMsgKey = 0;
	DWORD        dwKey   = 0;
	DWORD        dwUserIndex   = 0;
	ioHashString szPrivateID;
	ioHashString szBillingGUID;
	ioHashString szServerIP;
	ioHashString szTokenKey;
	char szuserID[32];
	ZeroMemory( szuserID, sizeof(szuserID) );

	pInfo->m_kPacket >> dwKey >> dwMsgKey >> szBillingGUID >> szPrivateID >> szTokenKey >> szServerIP >> iPort;
	
	//if( g_App.IsTestMode() )
	//{
	//	if(userId == NULL)
	//		userId = "testUser";
	//	SP2Packet kPacket( dwMsgKey );
	//	kPacket << szPrivateID;	//
	//	kPacket << szBillingGUID;
	//	kPacket << BILLING_LOGIN_RESULT_SUCCESS;
	//	kPacket << memberId;	//us 추가
	//	kPacket << userId;		//us 추가
	//	g_ServerNodeManager.SendMessageIP( szServerIP, iPort, kPacket );
	//	g_BillInfoManager->DeleteEUInfo( identity );
	//	LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::recvTokenResult LOGIN SUCCESS %s %s,AuthuserID:%s, AuthmemberIndex:%d"
	//								,szPrivateID.c_str(), szBillingGUID.c_str(), userId, memberId );
	//	return;
	//}
	
	if( result > 0 )
	{
		sprintf_s( szuserID, sizeof(szuserID), "%d", memberId);

		SP2Packet kPacket( dwMsgKey );
		/*kPacket << szuserID;*/
		kPacket << szPrivateID;
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
		kPacket << szuserID;	//us 추가 실제userID
		kPacket << userId;		//us 추가
		g_ServerNodeManager.SendMessageIP( szServerIP, iPort, kPacket );
		g_BillInfoManager->DeleteEUInfo( identity );
		LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::recvTokenResult() LOGIN SUCCESS key : %d, %s %s,userID:%s, memberIndex:%d",
			identity,szPrivateID.c_str(), szBillingGUID.c_str(), userId, memberId );

	}
	else
	{
		SP2Packet kPacket( dwMsgKey );
		kPacket << szPrivateID;
		kPacket << szBillingGUID;
		kPacket << BILLING_LOGIN_RESULT_FAIL;
		g_ServerNodeManager.SendMessageIP( szServerIP, iPort, kPacket );
		g_BillInfoManager->DeleteEUInfo( identity );
		LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::recvTokenResult() LOGIN FAIL ErrorCode : key : %d, %d, %s %s", identity, result, szPrivateID.c_str(), szBillingGUID.c_str() );
	}
		
}


void ioLocalUS::OnLogoutLog( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	ioHashString szprivateID;
	szprivateID.Clear();

	rkPacket >> szprivateID;

	LOG.PrintTimeAndLog( 0, "[TEST LOGOUT] ioLocalUS::OnLogoutLog ID:%s", szprivateID.c_str() );
	
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
//
//VOID ioLocalUS::recvTokenResult(UINT _memberId, CHAR* _userId, INT _result, INT _identity)
//{
//	BillInfoManager::BillingInfo *pInfo = g_BillInfoManager->GetEUInfo( _identity );
//	if( !pInfo )
//	{
//		LOG.PrintTimeAndLog( 0, "%s pInfo == NULL. %d:%d:%d:%d", __FUNCTION__, _identity, _memberId );
//		return;
//	}
//	DWORD		 dwMsgKey = 0;
//	DWORD        dwKey   = 0;
//	DWORD        dwUserIndex   = 0;
//	ioHashString szPrivateID;
//	ioHashString szBillingGUID;
//	bool         bSetUserMouse = false;
//	ioHashString szServerIP;
//	ioHashString szTokenKey;
//	int          iPort   = 0;
//
//	pInfo->m_kPacket >> dwKey >> dwMsgKey >> szBillingGUID >> szPrivateID >> szTokenKey >> szServerIP >> iPort;
//	if( _result > 0 )
//	{
//		SP2Packet kPacket( dwMsgKey );
//		kPacket << szPrivateID;
//		kPacket << szBillingGUID;
//		kPacket << _memberId;	//us 추가
//		kPacket << _userId;		//us 추가
//		kPacket << BILLING_LOGIN_RESULT_SUCCESS;
//		g_ServerNodeManager.SendMessageIP( szServerIP, iPort, kPacket );
//		g_BillInfoManager->DeleteEUInfo( _identity );
//		LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::recvTokenResult() LOGIN SUCCESS %s %s", szPrivateID.c_str(), szBillingGUID.c_str() );
//	}
//	else
//	{
//		SP2Packet kPacket( dwMsgKey );
//		kPacket << szPrivateID;
//		kPacket << szBillingGUID;
//		kPacket << BILLING_LOGIN_RESULT_FAIL;
//		g_ServerNodeManager.SendMessageIP( szServerIP, iPort, kPacket );
//		g_BillInfoManager->DeleteEUInfo( _identity );
//		LOG.PrintTimeAndLog( 0, "VOID ioLocalUS::recvTokenResult() LOGIN FAIL %s %s", szPrivateID.c_str(), szBillingGUID.c_str() );
//	}
//	
//		
//}
//VOID ioLocalUS::recvKickUser(UINT _memberId, INT _reason)
//{
//
//}
//VOID ioLocalUS::recvLogoutUserResult(UINT _memberId, BOOL _retType)
//{
//
//}
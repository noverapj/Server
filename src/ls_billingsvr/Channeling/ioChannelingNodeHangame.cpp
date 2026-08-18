#include "../stdafx.h"
#include "../NodeInfo/ServerNode.h"
#include "../xml/ioxmldocument.h"
#include "../xml/ioxmlelement.h"
#include "../database/logdbclient.h"
#include "../MainProcess.h"
#include <strsafe.h>
#include "../Util/ioHashString.h"
#include "../ThreadPool/ioThreadPool.h"
#include "../NodeInfo/ServerNodeManager.h"
#include "LS_HTTP/LS_HTTP/ioHTTP.h"
#include "ioChannelingNodeHangame.h"
#include "../Util/cJSON.h"
#include "LS_RestAPI/ioRestAPI.h"

extern CLog LOG;
extern CLog BillingItemLOG;

ioChannelingNodeHangame::ioChannelingNodeHangame()
{
	Init();
}

ioChannelingNodeHangame::~ioChannelingNodeHangame()
{
	Destroy();
}

void ioChannelingNodeHangame::Init()
{
	char szURL[MAX_PATH]="";

	ioINILoader kLoader( "ls_config_billingsvr.ini" );
	kLoader.SetTitle( "Channeling" );

	kLoader.LoadString( "HangameGetURL", "", szURL, sizeof( szURL ) );
	m_szGetURL = szURL;

	kLoader.LoadString( "HangameOutPutURL", "", szURL, sizeof( szURL ) );
	m_szBuyURL = szURL;
}

void ioChannelingNodeHangame::Destroy()
{
}

void ioChannelingNodeHangame::_OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	int          iChannelingType   = 0;
	DWORD        dwUserIndex       = 0;
	bool         bSetUserMouse     = false;

	ioHashString szPrivateID;
	ioHashString szPublicID;
	ioHashString szBillingGUID;
	ioHashString szUserKey;

	//LOG.PrintTimeAndLog( 0, "[test][hangame]getcash Start" ); 
	rkPacket >> iChannelingType >> szBillingGUID >> dwUserIndex >> szPrivateID >> szPublicID >> bSetUserMouse; // 공통사항
	rkPacket >> szUserKey; // 한게임.

	ioData kData;
	kData.SetChannelingType( iChannelingType );
	kData.SetBillingGUID( szBillingGUID );
	kData.SetUserIndex( dwUserIndex );
	kData.SetPrivateID( szPrivateID );
	kData.SetPublicID( szPublicID );
	kData.SetSetUserMouse( bSetUserMouse );
	kData.SetUserNo( szUserKey );
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_GET_CASH );

	g_ThreadPool.SetData( kData );

	//LOG.PrintTimeAndLog( 0, "[test][hangame]getcash Push END" );
}

void ioChannelingNodeHangame::TestGetURL()
{
	ioData kData;
	kData.SetChannelingType( CNT_HANGAME );

	kData.SetUserIndex( 1 );
	kData.SetPrivateID( "kk" );
	kData.SetPublicID( "kk" );
	kData.SetSetUserMouse( 1 );
	kData.SetUserNo( "bta_1512_012" );
	/*if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}*/
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_GET_CASH );

	g_ThreadPool.SetData( kData );
}

void ioChannelingNodeHangame::ThreadGetCash( const ioData &rData )
{
	//LOG.PrintTimeAndLog( 0, "[test][hangame]Call get cash thread" );
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][hangame]%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "%s LogOut: %d:%s:%s:%s:%s",__FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str() );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
		
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}
		return;
	}

	int iReturnCash    = 0;
	int iPurchasedCash = 0; // 현금을 주고 구매한 캐쉬
	char szReturnValue[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnValue, WEB_BUFF_SIZE );

	int iResult;

	int iReturnUTF8Size = 0;
	char szTemp[512] = {0,};
	AnsiToUTF8( rData.GetUserNo().c_str(), szTemp, iReturnUTF8Size, sizeof( szTemp) );

	std:string strPostData;
	cJSON *ProductInfo = NULL;
	ProductInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(ProductInfo, "chnl", HANGAME_CHNL);
	cJSON_AddStringToObject(ProductInfo, "userkey", szTemp);
	cJSON_AddStringToObject(ProductInfo, "cointype", HANGAME_COIN_TYPE);
			
	char* szPostTemp = cJSON_PrintUnformatted(ProductInfo);
	strPostData += szPostTemp;

	char szHeader[MAX_PATH]="";
	StringCbPrintf( szHeader, sizeof( szHeader ), "Content-Type: application/json; charset=utf-8");

	ioHTTP Winhttp;
	if( !Winhttp.GetResultData( m_szGetURL.c_str(), szPostTemp, szHeader, szReturnValue, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Http Error: %d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
		
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}

		g_LogDBClient.OnInsertBillingServerError( CNT_HANGAME, BILLING_ERROR_LOG_EXCEPTION,"Billing exception hangame." );
		return;
	}

	ioHashString szErrString;
	if(ParseGetCash(szReturnValue, iReturnCash, iPurchasedCash, iResult, szErrString) == FALSE)
	{
		BillingItemLOG.PrintTimeAndLog(0,"[error][hangame] getcash %s %s Json Return Error :%s %s %s", m_szGetURL.c_str(), szPostTemp, szReturnValue, rData.GetUserNo().c_str(), szErrString.c_str());
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
			  
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}

		return;
	}

	iReturnCash = iReturnCash + iPurchasedCash;

	iPurchasedCash = iReturnCash;

	SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << rData.GetSetUserMouse();
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << iReturnCash;
	kPacket << iPurchasedCash;
	      
	if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
	{
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		return;
	}
	BillingItemLOG.PrintTimeAndLog( 0, "%s Success: %d:%s:PrivateID %s:%s:%s[%d:%d]", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), iReturnCash, iPurchasedCash  );
	//LOG.PrintTimeAndLog(0, "%s Success ReturnValue :%s", __FUNCTION__, szReturnValue );
}


BOOL ioChannelingNodeHangame::ParseGetCash(const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString)
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
				if(strcmp("header", pObject->string) == 0)
				{
					cJSON* name = NULL;  cJSON* index = NULL;

					int iHeaderSize = cJSON_GetArraySize(pObject);
					for (BYTE J = 0; J < iHeaderSize; J++)
					{
						cJSON *pHeader = cJSON_GetArrayItem( pObject, J);
						if( pHeader && pHeader->string )
						{
							if(strcmp("isSuccessful", pHeader->string) == 0)
							{
								int iReturnAnsiSize = 0;
								char szTemp[512] = {0,};

								UTF8ToAnsi( pHeader->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );
							}
							else if(strcmp("resultCode", pHeader->string) == 0)
							{
								if(pHeader->valueint == HANGAME_SUCCESS)
									bState = TRUE;
							}
							else if(strcmp("resultMessage", pHeader->string) == 0)
							{
								int iReturnAnsiSize = 0;
								char szTemp[512] = {0,};

								UTF8ToAnsi( pHeader->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );
								szErrString = szTemp;
							}
						}
					}
				}
				else if(strcmp("result", pObject->string) == 0)
				{
					cJSON* name = NULL;  cJSON* index = NULL;

					int iResultSize = cJSON_GetArraySize(pObject);
					for (BYTE J = 0; J < iResultSize; J++)
					{
						cJSON *pResult = cJSON_GetArrayItem( pObject, J);
						if( pResult && pResult->string )
						{
							if(strcmp("totamt", pResult->string) == 0)
							{
								iRealCash = pResult->valueint;
							}
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
		BillingItemLOG.PrintTimeAndLog( 0, "[error][restapi] GetCashParse Json parse Crash %s - %s ", szReturnData, szErrString);
		return FALSE;
	}

	return FALSE;
}


void ioChannelingNodeHangame::_OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName )
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
	GetItemValueList( rkPacket, iType, iItemValueList );

	ioHashString szUserKey;
	rkPacket >> szUserKey; // 한게임 용 

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
	kData.SetUserNo( szUserKey );
	kData.SetGoodsNo( dwGoodsNo );
	kData.SetGoodsName( rszGoodsName );
	if( pServerNode )
	{
		kData.SetServerIP( pServerNode->GetIP() );
		kData.SetServerPort( pServerNode->GetClientPort() );
	}
	kData.SetEmpty( false );
	kData.SetProcessType( ioData::PT_OUTPUT_CASH );

	g_ThreadPool.SetData( kData );
}

void ioChannelingNodeHangame::ThreadOutputCash( const ioData &rData )
{
	if( rData.IsEmpty() )
	{
		LOG.PrintTimeAndLog( 0, "[error][hangame]%s Data is Empty.", __FUNCTION__ );
		return;
	}

	if( g_App.IsReserveLogOut() )
	{
		LOG.PrintTimeAndLog( 0, "%s LogOut: %d:%s:%s:%s",__FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str() );
		SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT  );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << CASH_RESULT_EXCEPT;
		kPacket << rData.GetExp();
		kPacket << rData.GetItemType();
		
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			LOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioChannelingNodeHangame::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		}
		return;
	}

	char szReturnValue[WEB_BUFF_SIZE]="";
	ZeroMemory( szReturnValue, WEB_BUFF_SIZE );

	//
	int iReturnCash    = 0;
	int iPurchasedCash = 0; // 현금을 주고 구매한 캐쉬
	int iResult;
	char szItemName[MAX_PATH]="";
	StringCbPrintf( szItemName, sizeof( szItemName ), "%d_%s", rData.GetGoodsNo(), rData.GetGoodsName().c_str() );

	int iReturnUTF8Size = 0;
	char szTemp[512] = {0,};
	AnsiToUTF8( rData.GetUserNo().c_str(), szTemp, iReturnUTF8Size, sizeof( szTemp) );

	std:string strPostData;
	cJSON *ProductInfo = NULL;
	ProductInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(ProductInfo, "chnl", HANGAME_CHNL);
	cJSON_AddStringToObject(ProductInfo, "userkey", szTemp);
	cJSON_AddStringToObject(ProductInfo, "cointype", HANGAME_COIN_TYPE);
	cJSON_AddStringToObject(ProductInfo, "cpid", HANGAME_CPID);
	cJSON_AddNumberToObject(ProductInfo, "payamt", rData.GetItemPayAmt());
	cJSON_AddNumberToObject(ProductInfo, "itemid", rData.GetGoodsNo());
			
	char* szPostTemp = cJSON_PrintUnformatted(ProductInfo);
	strPostData += szPostTemp;

	char szHeader[MAX_PATH]="";
	StringCbPrintf( szHeader, sizeof( szHeader ), "Content-Type: application/json; charset=utf-8");

	ioHTTP Winhttp;
	if( !Winhttp.GetResultData( m_szBuyURL.c_str(), szPostTemp, szHeader, szReturnValue, WEB_BUFF_SIZE ) )
	{
		LOG.PrintTimeAndLog( 0, "%s Http Error: %d:%s:%s:%s:%s:%d", __FUNCTION__, rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), GetLastError() );
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
		
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}

		g_LogDBClient.OnInsertBillingServerError( CNT_HANGAME, BILLING_ERROR_LOG_EXCEPTION,"Billing exception hangame." );
		return;
	}

	ioHashString szBuyNo;
	ioHashString szErrString;

	if(ParsePurchase(szReturnValue, szBuyNo, (ioData&)rData, iReturnCash, iPurchasedCash, iResult, szErrString) == FALSE)
	{
		BillingItemLOG.PrintTimeAndLog(0,"[error][hangame] getcash %s %s Json Return Error :%s %s %s", m_szBuyURL.c_str(), szPostTemp, szReturnValue, rData.GetUserNo().c_str(), szErrString.c_str());
		SP2Packet kPacket( BSTPK_GET_CASH_RESULT );
		kPacket << rData.GetUserIndex();
		kPacket << rData.GetBillingGUID();
		kPacket << rData.GetSetUserMouse();
		kPacket << CASH_RESULT_EXCEPT;
			  
		if ( !g_ServerNodeManager.SendMessageIP( (ioHashString) rData.GetServerIP(),  rData.GetServerPort(), kPacket ) )
		{
			BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s", "ioChannelingNodeHangame::ThreadGetCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str() );
		}

		return;
	}

	iReturnCash = iReturnCash + iPurchasedCash;


	SP2Packet kPacket( BSTPK_OUTPUT_CASH_RESULT );
	kPacket << rData.GetUserIndex();
	kPacket << rData.GetBillingGUID();
	kPacket << CASH_RESULT_SUCCESS;
	kPacket << rData.GetExp();
	kPacket << rData.GetItemType();
	kPacket << rData.GetItemPayAmt();
	kPacket << 0; // TransactionID ( FOR US ) //kyg 여기에 구매 유니크값 szReturnBuyNO
	kPacket << szBuyNo;

	int iItemValueList[MAX_ITEM_VALUE];
	for (int i = 0; i <MAX_ITEM_VALUE; i++)
		iItemValueList[i] = rData.GetItemValue( i );

	SetItemValueList( kPacket, rData.GetItemType(), iItemValueList );
	kPacket << rData.GetChannelingType(); // 공통
	kPacket << iReturnCash;
	kPacket << iPurchasedCash;
	
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
		BillingItemLOG.PrintTimeAndLog( 0, "%s Send Fail: %d:%s:%s  Ret %d:%s:%d", "ioChannelingNodeHangame::ThreadOutputCash", rData.GetUserIndex(), rData.GetBillingGUID().c_str(), rData.GetPrivateID().c_str(), rData.GetGoodsNo(), rData.GetGoodsName().c_str(), rData.GetItemPayAmt() );
		return;
	}
	
	BillingItemLOG.PrintTimeAndLog(0, "[info][hangame]goods buy success. :%d:%s:PrivateID %s:%s:%s[%d:%s:%d:%d:%d:%s]", rData.GetUserIndex(), rData.GetBillingGUID().c_str(),rData.GetPrivateID().c_str(), rData.GetPublicID().c_str(), rData.GetUserNo().c_str(), rData.GetGoodsNo(), szItemName, rData.GetItemPayAmt(), iReturnCash, iPurchasedCash, szBuyNo.c_str() );
}


BOOL ioChannelingNodeHangame::ParsePurchase(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString)
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
				if(strcmp("header", pObject->string) == 0)
				{
					cJSON* name = NULL;  cJSON* index = NULL;

					int iHeaderSize = cJSON_GetArraySize(pObject);
					for (BYTE J = 0; J < iHeaderSize; J++)
					{
						cJSON *pHeader = cJSON_GetArrayItem( pObject, J);
						if( pHeader && pHeader->string )
						{
							if(strcmp("isSuccessful", pHeader->string) == 0)
							{
								int iReturnAnsiSize = 0;
								char szTemp[512] = {0,};

								UTF8ToAnsi( pHeader->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );
							}
							else if(strcmp("resultCode", pHeader->string) == 0)
							{
								if(pHeader->valueint == HANGAME_SUCCESS)
									bState = TRUE;
							}
							else if(strcmp("resultMessage", pHeader->string) == 0)
							{
								int iReturnAnsiSize = 0;
								char szTemp[512] = {0,};

								UTF8ToAnsi( pHeader->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );
								szErrString = szTemp;
							}
						}
					}
				}
				else if(strcmp("result", pObject->string) == 0)
				{
					cJSON* name = NULL;  cJSON* index = NULL;

					int iResultSize = cJSON_GetArraySize(pObject);
					for (BYTE J = 0; J < iResultSize; J++)
					{
						cJSON *pResult = cJSON_GetArrayItem( pObject, J);
						if( pResult && pResult->string )
						{
							if(strcmp("rtn", pResult->string) == 0)
							{
								int iResult = pResult->valueint;
								if(pResult->valueint == HANGAME_SUCCESS)
									bState = TRUE;
							}
							else if(strcmp("status", pResult->string) == 0)
							{
								if(bState == TRUE)
								{
									int iReturnAnsiSize = 0;
									char szTemp[512] = {0,};

									UTF8ToAnsi( pResult->valuestring, szTemp, iReturnAnsiSize, sizeof(szTemp) );
									szBuyNo = szTemp;
								}
							}
							else if(strcmp("bal1", pResult->string) == 0)
							{
								iRealCash += pResult->valueint;
							}
							else if(strcmp("bal2", pResult->string) == 0)
							{
								iRealCash += pResult->valueint;
							}
							else if(strcmp("bal3", pResult->string) == 0)
							{
								iRealCash += pResult->valueint;
							}
							else if(strcmp("bal4", pResult->string) == 0)
							{
								iRealCash += pResult->valueint;
							}
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
		LOG.PrintTimeAndLog( 0, "[error][restapi] ParsePurchase Json parse Crash %s ", szReturnData);
		return FALSE;
	}

	return FALSE;
}


void ioChannelingNodeHangame::AnsiToUTF8( IN const char *szAnsi, OUT char *szUTF8, OUT int &riReturnUTF8Size, IN int iUTF8BufferSize )
{
	if( szAnsi == NULL )
		return;

	WCHAR szUni[MAX_PATH*2];
	ZeroMemory( szUni, sizeof(szUni) );

	int iUnisize = MultiByteToWideChar(CP_ACP, 0, szAnsi, -1 , szUni, MAX_PATH*2 );
	riReturnUTF8Size = WideCharToMultiByte(CP_UTF8, 0 , szUni, iUnisize, szUTF8, iUTF8BufferSize ,NULL ,NULL );
}
void ioChannelingNodeHangame::UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize )
{
	if( szUTF8 == NULL )
		return;

	WCHAR szUni[MAX_PATH*2];
	ZeroMemory( szUni, sizeof(szUni) );

	int iUnisize = MultiByteToWideChar(CP_UTF8, 0, szUTF8, -1 , szUni, MAX_PATH*2 );
	riReturnAnsiSize = WideCharToMultiByte(CP_ACP, 0 , szUni, iUnisize, szAnsi, iAnsiBufferSize ,NULL ,NULL );
}
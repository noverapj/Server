#ifndef __ioChannelingNodeHappyTuk_h__
#define __ioChannelingNodeHappyTuk_h__

#include "ioChannelingNodeParent.h"
#include "../LoginManager/loginmanager.h"
#include "../NodeInfo/TestCashManager.h"

#define TAIWAN_CODE "00015"
#define TAIWAN_TOKEN '|'
#define TAIWAN_EXTEND_TOKEN '='

class ioChannelingNodeHappyTuk : public ioChannelingNodeParent
{
protected:
	enum
	{
		MAX_LOGIN_ARRAY    = 2,
		LOGIN_ARRAY_KEY    = 0,
		LOGIN_ARRAY_USERNO = 1,
	};

private:
	typedef std::map< DWORD, int> ProductInfoMap;
	ProductInfoMap m_ProductInfoMap;

protected:
	ioHashString m_sServiceCode;
	ioHashString m_sLoginURL;
	ioHashString m_sGetURL;
	ioHashString m_sProductListURL;
	ioHashString m_sPurchaseItemURL;
	ioHashString m_sPurchaseGiftURL;
	ioHashString m_sOutputURL;
	ioHashString m_sSubscriptionRetractURL;
	ioHashString m_sClientSecret;
	ioHashString m_sKey;

public:

	virtual void OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );
	virtual void _OnSubscriptionRetract( ServerNode* pServerNode, SP2Packet& rkPakcet);
	
public:

	virtual void ThreadAutoUpgradeLogin( const ioData &rData );
	virtual void ThreadOTP( const ioData &rData );
	virtual void ThreadLogin( const ioData &rData, LoginManager &rLoginMgr );
	virtual void ThreadGetCash( const ioData &rData );
	virtual void ThreadOutputCash( const ioData &rData );
	virtual void ThreadPresent( const ioData &rData );
	virtual void ThreadSubscriptionRetract( const ioData& rData);

protected:
	BOOL HappyTalkGetCash(const ioHashString& szUserID, int& iReturnCash, int& iPurchase, int& iResult, ioHashString& szErrString);
	BOOL HappyTalkGetCashParse(const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString);

	BOOL HappyTalkPurchaseItem(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult);
	BOOL HappyTalkPurchaseParam(const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData);
	BOOL HappyTalkPurchaseParse(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString);

	BOOL HappyTalkPurchaseGift(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult);
	BOOL HappyTalkPurchaseGiftParam(const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData);
	BOOL HappyTalkPurchaseGiftParse(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString);

	void MakeTransactionID( char* szTransactionID, ioData &rData );

protected:
	void SendExecptMessage(DWORD dwPacketID, int iErrCode, const ioData& rData, ioHashString& szErrString);
	void UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize );

public:
	virtual ChannelingType GetType();
	ioChannelingNodeHappyTuk(void);
	virtual ~ioChannelingNodeHappyTuk(void);
};

#endif //__ioChannelingNodeHappyTuk_h__
 
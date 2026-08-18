#ifndef __ioLocalTaiwan_h__
#define __ioLocalTaiwan_h__

#include "ioLocalParent.h"
#include "../LoginManager/loginmanager.h"
#include "../NodeInfo/TestCashManager.h"

#define TAIWAN_CODE "00015"
#define TAIWAN_TOKEN '|'
#define TAIWAN_EXTEND_TOKEN '='

class ioLocalTaiwan  : public ioLocalParent
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
	virtual BOOL ProductInit();

	virtual void OnAutoupgradeLogin( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void OnOTP( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );
	virtual void _OnSubscriptionRetract( ServerNode* pServerNode, SP2Packet& rkPakcet);
	
public:
	virtual void ThreadAutoUpgradeLogin( const ioData &rData, LoginManager &rLoginMgr );
	virtual void ThreadOTP( const ioData &rData );
	virtual void ThreadLogin( const ioData &rData, LoginManager &rLoginMgr );
	virtual void ThreadGetCash( const ioData &rData );
	virtual void ThreadOutputCash( const ioData &rData );
	virtual void ThreadPresent( const ioData &rData );
	virtual void ThreadSubscriptionRetract( const ioData& rData);

protected:
	BOOL ValofeGetCash(const ioData& rData, int& iReturnCash, int& iPurchase, int& iResult, ioHashString& szErrString);
	BOOL ValofeGetCashParse(const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString);

	BOOL ValofePurchaseItem(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult);
	BOOL ValofePurchaseParam(const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData);
	BOOL ValofePurchaseParse(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString);

	BOOL ValofePurchaseGift(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult);
	BOOL ValofePurchaseGiftParam(const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData);
	BOOL ValofePurchaseGiftParse(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString);

	void MakeTransactionID( char* szTransactionID, ioData &rData );

protected:
	void SendExecptMessage(DWORD dwPacketID, int iErrCode, const ioData& rData, ioHashString& szErrString);

protected:
	void UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize );

public:
	virtual ioLocalManager::LocalType GetType();

public:
	ioLocalTaiwan(void);
	virtual ~ioLocalTaiwan(void);
};

#endif // __ioLocalTaiwan_h__
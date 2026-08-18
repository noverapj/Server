#pragma once

#include "ioChannelingNodeParent.h"

class ioRestAPI;

#define NEXON_GAME_CODE 112
#define NEXON_PATH      512

class ioChannelingNodeValofe : public ioChannelingNodeParent
{
private:
	struct ProductInfo
	{
		int m_iProductNo;
		int m_iProductPrice;

		ProductInfo()
		{
			m_iProductNo	= 0;
			m_iProductPrice = 0;
		}
	};

	typedef std::map< DWORD, ProductInfo* > ProductInfoMap;
	ProductInfoMap m_ProductInfoMap;

private:

#ifdef VALOFE_NEW_BILLING_SYS_SYH
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
#else
	ioHashString    m_szCompanyCD;
#endif
	

public:
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );
	virtual void _OnSubscriptionRetract( ServerNode* pServerNode, SP2Packet& rkPakcet);
	
public:
	virtual void ThreadGetCash( const ioData &rData );
	virtual void ThreadOutputCash( const ioData &rData );
	virtual void ThreadPresent( const ioData &rData );
	virtual void ThreadSubscriptionRetract( const ioData& rData);

protected:
#ifdef VALOFE_NEW_BILLING_SYS_SYH
	BOOL ValofeGetCash(const ioData& rData, int& iRealCash, int& iPurchase, int& iResult, ioHashString& szErrString );
	BOOL ValofeGetCashParse(const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString);

	BOOL ValofePurchaseItem(const ioHashString& szUserID, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString, int& iResult);
	BOOL ValofePurchaseParam(const ioHashString& szUserID, ioData& rData, ioHashString& szErrString, ioHashString& szJsonData);
	BOOL ValofePurchaseParse(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, ioHashString& szErrString);
#else
	BOOL ValofeGetCash(const ioHashString& szUserIndex, const ioHashString& szUserID, const ioHashString& szPublicID, int& iReturnCash, int& iPurchase, int& iResult, ioHashString& szErrString);
#endif // VALOFE_NEW_BILLING_SYS_SYH

protected:
	void SendExecptMessage(DWORD dwPacketID, int iErrCode, const ioData& rData, ioHashString& szErrString);

#ifdef VALOFE_NEW_BILLING_SYS_SYH
protected:
	void UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize );
	void Get_Encode(const ioHashString& strMulti, char* strUtf8 );
#endif // VALOFE_NEW_BILLING_SYS_SYH
public:
	virtual ChannelingType GetType();

	virtual BOOL ProductInit();

public:
	ioChannelingNodeValofe(void);
	virtual ~ioChannelingNodeValofe(void);
};

 
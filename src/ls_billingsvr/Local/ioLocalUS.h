#ifndef __ioLocalUS_h__
#define __ioLocalUS_h__

#include "ioLocalParent.h"
#include "../NodeInfo/TestCashManager.h"
#include "../NodeInfo/MemInfoManager.h" 

#define USER_TYPE_NORMAL "WMU"
#define USER_TYPE_FB     "FB"
#define SHA256_DIGEST_SIZE 32
 

class ioLocalUS : public ioLocalParent//, public nsGSS::GsAuthBaseClient
{
protected:
	enum 
	{
		RESULT_SUCCESS   = 0,
	};

protected:
	TestCashManager m_TestCashManager;
	
	ioHashString	m_sHashKey;			//해쉬코드 만들때 키값
	ioHashString	m_sFillCashURL;			//충전시 호출
	ioHashString    m_sRedeemURL;			//인증 URL
	DWORD			m_dwAuthReqKey;			//인증시 사용하는 ReqKey
	DWORD			m_dwBillingReqKey;		//빌링에서 사용하는 ReqKey
	DWORD			m_HashStep;

public:
	virtual ioLocalManager::LocalType GetType();
	virtual void Init();

public:
	virtual void OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );
	virtual void OnCancelCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	
public:
	virtual void ThreadLogin( const ioData &rData, LoginManager &rLoginMgr );
	virtual void OnRecieveGetCash( const GTX_PK_GETBALANCE &rkResult );
	virtual void OnRecieveOutputCash( const GTX_PK_PURCHASEITEM &rkResult );
	virtual void OnReceiveUSAuth( DWORD memberId, CHAR* userId, int result, int identity );
	
	//hash 코드
	void MakeHashCode(  const char *rszCode, OUT char *szHashCode  );
	void GetHashString( const ioHashString& szMemberUID, int iBillingType, const int dwUserIndex, const ioHashString& szBillingGUID, const ioHashString& serverIP, int serverPort );
	

	virtual void OnFillCashUrl( ServerNode *pServerNode, SP2Packet &rkPacket );

	virtual void OnLogoutLog( ServerNode *pServerNode, SP2Packet &rkPacket );
	//virtual VOID recvTokenResult(UINT _memberId, CHAR* _userId, INT _result, INT _identity);
	//virtual VOID recvKickUser(UINT _memberId, INT _reason);
	//virtual VOID recvLogoutUserResult(UINT _memberId, BOOL _retType);

public:
	ioLocalUS(void);
	virtual ~ioLocalUS(void);
};

#endif // __ioLocalUS_h__
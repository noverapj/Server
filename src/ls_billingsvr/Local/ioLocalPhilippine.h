#ifndef __ioLocalPhilippine_h__
#define __ioLocalPhilippine_h__

#ifdef __OHTG_LOCAL_PHILIPPINE__

#include "ioLocalParent.h"

#include "../LoginManager/loginmanager.h"
#include "../NodeInfo/TestCashManager.h"
#include "../Philippine/BillInterfaces.h"

class ioLocalPhilippine : public ioLocalParent
{
protected:
	enum 
	{
		RESULT_SUCCESS   = 0,
	};
protected:
	ioHashString m_sLoginURL;
	ioHashString m_sBillingGetURL;
	ioHashString m_sBillingOutPutURL;
	ioHashString m_sPCRoomURL;
	DWORD			m_dwBillingReqKey;		//빌링에서 사용하는 ReqKey

protected:
	TestCashManager m_TestCashManager;
	BillInterfaces	m_BillInterfaces;

public:
	virtual ioLocalManager::LocalType GetType();

	virtual void Init();

	virtual void OnAutoupgradeLogin( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void OnLoginData( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );
	virtual void OnCancelCash( ServerNode *pServerNode, SP2Packet &rkPacket );

	virtual bool IsLoadGoodsNameList() { return false; }

public:
	virtual void OnRecieveGetCash( const GTX_PHL_PK_GETBALANCE &rkResult );
	virtual void OnRecieveOutputCash( const GTX_PHL_PK_PURCHASEITEM &rkResult );
	virtual void OnRecieveCancelCash( const GTX_PHL_PK_CNLPURCHASE &rkResult );

	virtual void OnLogoutLog( ServerNode *pServerNode, SP2Packet &rkPacket );
public:
	ioLocalPhilippine(void);
	virtual ~ioLocalPhilippine(void);
};
#endif //__OHTG_LOCAL_PHILIPPINE__

#endif // __ioLocalPhilippine_h__
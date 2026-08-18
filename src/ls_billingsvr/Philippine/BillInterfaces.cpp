
#include <comutil.h>
#include <objbase.h>

#include "BillInterfaces.h"

/**--------------------------------------------------------------
-- ProcedureName   : GetUserBalance
-- Description     : 잔액조회 인터페이스
-- Return Value    : long
--                   0 = 성공, 그외 = 에러발생
--
-- Copyright ⓒ 2008 by PayLetter Inc. All rights reserved.
---------------------------------------------------------------*/
long BillInterfaces::GetUserBalance
(
	const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
	long		lngUserNo,		// UserNo

	long*		lngRealCash,	// Real Cash 잔액
	long*		lngBonus		// Bonus Cash 잔액
)	
{
	CoInitialize(NULL);

	IBill*		pi = NULL;
	long		nTxResult = 0;

	// 초기화
	*lngRealCash	= 0;
	*lngBonus		= 0;

	HRESULT hr = CoCreateInstance(
		CLSID_Bill, 
		NULL, 
		CLSCTX_ALL, 
		IID_IBill, 
		(void**)&pi);

	if (FAILED(hr))
	{
		CoUninitialize();
		return (-1);
	}

	try
	{
		pi->PutHOST(_bstr_t(szBillServer));
		pi->PutTxCmd(_bstr_t(TXCMD_ACCOUNTINFO));

		pi->SetField(_bstr_t(BILL_SITECODE),	_bstr_t(MY_SITECODE));
		pi->SetFieldLong(_bstr_t(BILL_USERNO),		lngUserNo);

		nTxResult = pi->StartAction();
	}
	catch(...)
	{
		pi->Release();
		CoUninitialize();

		return (-2);
	}

	// 잔액 조회 성공, Real Cash, Bonus Cash, Mileage를 구한다.
	if (nTxResult == BILL_SUCCESS)
	{
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHREAL)),	lngRealCash);
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHBONUS)),	lngBonus);
	}

	pi->Release();
	CoUninitialize();

	return(nTxResult);
}

/**--------------------------------------------------------------
-- ProcedureName   : ChargeItem
-- Description     : 아이템 구매(차감) 인터페이스
-- Return Value    : long
--                   0 = 차감성공, 그외 = 에러발생

--
-- Copyright ⓒ 2008 by PayLetter Inc. All rights reserved.
---------------------------------------------------------------*/
long BillInterfaces::ChargeItem
(
	const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
	long		lngUserNo,		// UserNo
	const char* szUserID,		// UserID
	const char* szUserName,		// 이용자명
	const char*	szUserIP,		// 이용자 IP주소
	
	long		lngItemID,		// 구매 아이템 ID
	long		lngItemPrice,		// 아이템가격
	const char*	szProdName,		// 아이템명

	long		lngPresentFlag,	// 선물여부(1:일반구매, 2:선물)

	long*		lngRealCash,	// Real Cash 잔액
	long*		lngBonus,		// Bonus Cash 잔액
	char*		szChargeNo		// 과금번호(19 Bytes) (취소할때 필요함.)
)	
{
	CoInitialize(NULL);

	IBill*		pi = NULL;
	long		nTxResult = 0;

	// 초기화
	*lngRealCash	= 0;
	*lngBonus		= 0;

	HRESULT hr = CoCreateInstance(
		CLSID_Bill, 
		NULL, 
		CLSCTX_ALL, 
		IID_IBill, 
		(void**)&pi);

	if (FAILED(hr))
	{
		CoUninitialize();
		return (-1);
	}

	try
	{
		pi->PutHOST(_bstr_t(szBillServer));
		pi->PutTxCmd(_bstr_t(TXCMD_CHARGEGAMEITEM)); // chargegameitem

		pi->SetField(_bstr_t(BILL_SITECODE),	_bstr_t(MY_SITECODE));
		pi->SetField(_bstr_t(BILL_CPID),		_bstr_t(MY_CPID));
		pi->SetField(_bstr_t(BILL_USERID),		_bstr_t(szUserID));
		pi->SetField(_bstr_t(BILL_USERNAME),	_bstr_t(szUserName));
		pi->SetField(_bstr_t(BILL_IPADDR),		_bstr_t(szUserIP));
		pi->SetField(_bstr_t(BILL_PRODNAME),		_bstr_t(szProdName));
	
		pi->SetFieldLong(_bstr_t(BILL_USERNO),		lngUserNo);
		pi->SetFieldLong(_bstr_t(BILL_ITEMID),		lngItemID);
		pi->SetFieldLong(_bstr_t(BILL_ITEMCNT),		1);
		pi->SetFieldLong(_bstr_t(BILL_CHARGEAMT),		lngItemPrice);
		pi->SetFieldLong(_bstr_t(BILL_PRESENTFLAG),	lngPresentFlag);

		nTxResult = pi->StartAction();
	}
	catch(...)
	{
		pi->Release();
		CoUninitialize();

		return (-2);
	}

	// 아이템 구매(차감) 성공
	// 차감후 Real Cash, Bonus Cash, Mileage 잔액 및 과금번호를 구한다.
	if (nTxResult == BILL_SUCCESS)
	{
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHREAL)),	lngRealCash);
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHBONUS)),	lngBonus);

		szChargeNo[0] = NULL;
		if (szChargeNo != NULL)
			(void)WCToMB(pi->GetVal(_bstr_t(BILL_CHARGENO)),	szChargeNo, 20);
	}

	pi->Release();
	CoUninitialize();

	return(nTxResult);
}

/**--------------------------------------------------------------
-- ProcedureName   : UseCancelDirect
-- Description     : 아이템 구매취소 인터페이스
-- Return Value    : long
--                   0 = 차감성공, 그외 = 에러발생

--
-- Copyright ⓒ 2008 by PayLetter Inc. All rights reserved.
---------------------------------------------------------------*/
long BillInterfaces::UseCancelDirect
		(
			const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
			long		lngUserNo,		// UserNo
			const char*		szChargeNo,		// 과금번호(19 Bytes) (취소할때 필요함.)

			long*		lngRealCash,	// Real Cash 잔액
			long*		lngBonus		// Bonus Cash 잔액
		)
{
	CoInitialize(NULL);

	IBill*		pi = NULL;
	long		nTxResult = 0;

	// 초기화
	*lngRealCash	= 0;
	*lngBonus		= 0;

	HRESULT hr = CoCreateInstance(
		CLSID_Bill, 
		NULL, 
		CLSCTX_ALL, 
		IID_IBill, 
		(void**)&pi);

	if (FAILED(hr))
	{
		CoUninitialize();
		return (-1);
	}

	try
	{
		pi->PutHOST(_bstr_t(szBillServer));
		pi->PutTxCmd(_bstr_t(TXCMD_USECANCELDIRECT));

		pi->SetField(_bstr_t(BILL_SITECODE),	_bstr_t(MY_SITECODE));
		pi->SetField(_bstr_t(BILL_CHARGENO),	_bstr_t(szChargeNo));
		pi->SetFieldLong(_bstr_t(BILL_USERNO),		lngUserNo);

		nTxResult = pi->StartAction();
	}
	catch(...)
	{
		pi->Release();
		CoUninitialize();

		return (-2);
	}

	// 잔액 조회 성공, Real Cash, Bonus Cash, Mileage를 구한다.
	if (nTxResult == BILL_SUCCESS)
	{
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHREAL)),	lngRealCash);
		(void)WCToMB(pi->GetVal(_bstr_t(BILL_CASHBONUS)),	lngBonus);
	}

	pi->Release();
	CoUninitialize();

	return(nTxResult);
}


// Wide Char --> Multi-Bytes 변환
int BillInterfaces::WCToMB(LPCWSTR wszString, LPSTR szReturnString, int cbMultiByte)
{
	int len = 0;

	try
	{
		if (szReturnString != NULL)
			szReturnString[0] = NULL;

		len = WideCharToMultiByte(CP_ACP, 0, wszString, -1, szReturnString, cbMultiByte, NULL, NULL);

		if (len >= 0 && szReturnString && len < cbMultiByte)
			szReturnString[len] = NULL;
	}
	catch(...)
	{
		len = 0;
	}

	return (len);
}

// Wide Char --> Multi-Bytes 변환
int BillInterfaces::WCToMB(LPCWSTR wszString, long* lngValue)
{
	int		len = 0;
	char	szBuff[32];
	int		cbMultiByte = 32;

	try
	{
		len = WideCharToMultiByte(CP_ACP, 0, wszString, -1, szBuff, cbMultiByte, NULL, NULL);

		if (len >= 0 && szBuff && len < cbMultiByte)
			szBuff[len] = NULL;
	}
	catch(...)
	{
		len = 0;
	}

	*lngValue = atol(szBuff);

	return (len);
}

#ifndef __BILL_INTERFACES_H__
#define __BILL_INTERFACES_H__

// BOQ NDoors 통신 컴포넌트 Import
#import "ComponentStore\BOQN3MG.dll" no_namespace named_guids

// TX 명령어
#define	TXCMD_ACCOUNTINFO		"accountinfo"		// 잔액조회 명령어
#define	TXCMD_CHARGEGAMEITEM	"chargeitem"		// 과금(차감) 명령어
#define	TXCMD_USECANCELDIRECT	"usecanceldirect"	// 과금취소 명령어

#define	MY_SITECODE				"MICRO"				// 5 bytes
#define	MY_CPID					"10"				// CrossFire

#define BILL_SUCCESS			0

#define BILL_SITECODE			"sitecode"
#define BILL_CPID				"cpid"
#define BILL_USERNO				"userno"
#define BILL_USERID				"userid"
#define BILL_USERNAME			"username"
#define BILL_FUSERID			"fuserid"
#define BILL_ITEMID				"itemid"
#define BILL_ITEMCNT			"itemcnt"
#define BILL_PRESENTFLAG		"presentflag"
#define BILL_PRESENTCOMMENT		"presentcomment"
#define BILL_IPADDR				"ipaddr"
#define BILL_CASHREAL			"cashreal"
#define BILL_CASHBONUS			"cashbonus"
#define BILL_CHARGENO			"chargeno"
#define BILL_PRODNAME			"prodname"
#define BILL_CHARGEAMT			"chargeamt"

class BillInterfaces
{
public:
	// 잔액조회 인터페이스
	static long GetUserBalance
		(
			const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
			long		lngUserNo,		// UserNo

			long*		lngRealCash,	// Real Cash 잔액
			long*		lngBonus		// Bonus Cash 잔액
		);

	// 아이템 구매(차감) 인터페이스
	static	long ChargeItem
		(
			const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
			long		lngUserNo,		// UserNo
			const char* szUserID,		// UserID
			const char* szUserName,		// 이용자명
			const char*	szUserIP,		// 이용자 IP주소
			
			long		lngItemID,		// 구매 아이템 ID
			long		lngItemPrice,	// 아이템가격
			const char*	szProdName,		// 아이템명

			long		lngPresentFlag,	// 선물여부(1:일반구매, 2:선물)

			long*		lngRealCash,	// Real Cash 잔액
			long*		lngBonus,		// Bonus Cash 잔액
			char*		szChargeNo		// 과금번호(19 Bytes) (취소할때 필요함.)
		);

	// 과금취소 인터페이스
	static long UseCancelDirect
		(
			const char* szBillServer,	// BillServerIP:Port (111.111.111.111:50050)
			long		lngUserNo,		// UserNo
			const char*		szChargeNo,		// 과금번호(19 Bytes) (취소할때 필요함.)

			long*		lngRealCash,	// Real Cash 잔액
			long*		lngBonus		// Bonus Cash 잔액
		);

private:
	// WideCharToMultiByte
	static int WCToMB(LPCWSTR wszString, long* lngValue);
	static int WCToMB(LPCWSTR wszString, LPSTR szReturnString, int cbMultiByte);
};

#endif

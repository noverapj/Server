
#ifndef _ioUserPractice_h_
#define _ioUserPractice_h_

#include <chrono>

class User;
class CQueryResultData;


#define PRACTICERANK 99999999
#define PRACTICETIME 99999999

struct SPractice
{
	DWORD			m_dwID;
	DWORD			m_dwGrade;
	DWORD			m_dwCount;
	DWORD			m_dwTime;
	DWORD			m_dwRank;

	SPractice()
	{
		m_dwID			= 0;
		m_dwGrade		= 0;
		m_dwCount		= 0;
		m_dwTime		= PRACTICETIME;
		m_dwRank		= PRACTICERANK;
	}
};

struct SPracticePresent
{
	DWORD			m_dwPresentType;
	DWORD			m_dwCode;
	DWORD			m_dwValue;

};

const int PracticePresentCOUNT = 5;

struct SPracticeRank
{
	ioHashString		m_strStartDate;
	ioHashString		m_strEndDate;
	DWORD				m_dwID;
	DWORD				m_dwRank;
	std::vector< SPracticePresent > m_vSPracticePresent;

	SPracticeRank()
	{
		m_strStartDate = "";
		m_strEndDate = "";
		m_dwID			= 0;
		m_dwRank		= PRACTICERANK;
		m_vSPracticePresent.clear();
	}
};

typedef std::unordered_map<DWORD, SPractice> MAPPRACTICE;
typedef MAPPRACTICE::iterator MAPPRACTICE_iter;

class ioUserPractice
{
private:
	User*					m_pUser;
	DWORD					m_dwUserIndex;

	std::vector< SPracticeRank > m_vPracticeRankList;
	MAPPRACTICE				m_mapPracticeList;

	CTime					m_LastTime;			//로그인 한 시간.
	bool					m_bSendInfo;
	bool					m_bSendRank;
	bool					m_bApplyMove;

	std::chrono::system_clock::time_point m_PracticeEndTime;
	int						m_iAbusingCount;

public:

	void	Initialize( User* pUser );
	void	InitData();
	void	InitCount(DWORD dwAgentID, DWORD dwThreadID, DWORD dwUserIndex);

	DWORD	GetPracticeCount();
	DWORD	GetPracticeAdmissionCount();
	void	FillMoveData( SP2Packet &rkPacket );
	void	ApplyMoveData( SP2Packet &rkPacket, bool bDummyNode = false );

	void	AddPractice( SPractice& kPractice );
	void	GetPracticeList( MAPPRACTICE& kPracticeList );

	void	SetPractice( DWORD dwID, DWORD dwGrade, DWORD dwCount, DWORD dwTime, DWORD dwRank );
	void	SetPracticeCount( DWORD dwID, DWORD dwCount);
	void	SetPracticeTimeRank( DWORD dwID, DWORD dwCount, DWORD dwRank );

	void	AddPracticeRankDate( SPracticeRank& kPracticeRank );
	void	GetPracticeRankList( std::vector< SPracticeRank >& kPracticeList );

	DWORD	GetGrade( DWORD dwID );
	SPractice GetPractice( DWORD dwID );

	void	SetLastTime(CTime LastTime);
	CTime	GetLastTime();

	void	SetSendInfo(bool bSendInfo);
	bool	IsSendInfo();
	
	void	SetSendRank(bool bSendRank);
	bool	IsSendRank();

	void	SetApplyMove(bool bApplyMove);
	bool	IsApplyMove();

	void SetBoostPracticeEndTime( std::chrono::system_clock::time_point PracticeEndTime ){	m_PracticeEndTime = PracticeEndTime;	}
	std::chrono::system_clock::time_point GetBoostPracticeEndTime(){	return m_PracticeEndTime;	}

	void SetAbusingCount( int iAbusingCount ) { m_iAbusingCount = iAbusingCount; }
	int GetAbusingCount()	{	return m_iAbusingCount;		}

public:
	ioUserPractice();
	virtual ~ioUserPractice();
};

#endif


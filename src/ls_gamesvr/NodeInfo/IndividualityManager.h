
#ifndef _IndividualityManager_h_
#define _IndividualityManager_h_

#include "../Util/Singleton.h"

#define MAX_BASIC_TRAIT 8

class User;

class IndividualityManager : public Singleton< IndividualityManager >
{
protected:
	int m_iMaxLevel;
	int m_iNeedPeso;

	int m_iMaxInfo;
	int m_BasicMaxPoint[MAX_BASIC_TRAIT];

	int m_iMaxPointMain;
	int m_iMaxPointSub1;
	int m_iMaxPointSub2;
	int m_iLimitLevelMain;
	int m_iLimitLevelSub1;
	int m_iLimitLevelSub2;

public:
	void LoadINI();

protected:
	void ClearAllInfo();

public:
	int GetBasicMaxPoint( int iIndex ) const;

	int GetMaxLevel() const { return m_iMaxLevel; }
	int GetNeedPeso() const { return m_iNeedPeso; }
	int GetMaxInfo() const { return m_iMaxInfo; }
	int GetMaxPointMain() const { return m_iMaxPointMain; }
	int GetMaxPointSub1() const { return m_iMaxPointSub1; }
	int GetMaxPointSub2() const { return m_iMaxPointSub2; }
	int GetLimitLevelMain() const { return m_iLimitLevelMain; }
	int GetLimitLevelSub1() const { return m_iLimitLevelSub1; }
	int GetLimitLevelSub2() const { return m_iLimitLevelSub2; }

public:
	static IndividualityManager& GetSingleton();

public:
	IndividualityManager();
	virtual ~IndividualityManager();
};

#define g_IndividualityMgr IndividualityManager::GetSingleton()

#endif

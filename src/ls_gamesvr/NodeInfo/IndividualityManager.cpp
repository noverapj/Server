
#include "stdafx.h"
#include "IndividualityManager.h"

template<> IndividualityManager* Singleton< IndividualityManager >::ms_Singleton = 0;

IndividualityManager::IndividualityManager()
{
	ClearAllInfo();
}

IndividualityManager::~IndividualityManager()
{
	ClearAllInfo();
}

void IndividualityManager::ClearAllInfo()
{
	m_iMaxLevel = 0;
	m_iNeedPeso = 0;

	m_iMaxInfo = 0;
	memset( m_BasicMaxPoint, 0, sizeof(m_BasicMaxPoint) );

	m_iMaxPointMain = 0;
	m_iMaxPointSub1 = 0;
	m_iMaxPointSub2 = 0;
	m_iLimitLevelMain = 0;
	m_iLimitLevelSub1 = 0;
	m_iLimitLevelSub2 = 0;
}

IndividualityManager& IndividualityManager::GetSingleton()
{
	return Singleton< IndividualityManager >::GetSingleton();
}

void IndividualityManager::LoadINI()
{
	ClearAllInfo();

	ioINILoader kLoader( "config/individuality_info.ini" );

	kLoader.SetTitle( "common" );
	m_iMaxLevel = kLoader.LoadInt( "max_level", 100 );
	m_iNeedPeso = kLoader.LoadInt( "need_peso", 5000 );

	kLoader.SetTitle( "normal_info" );
	m_iMaxInfo = kLoader.LoadInt( "max_info", MAX_BASIC_TRAIT );
	for( int i = 0; i < MAX_BASIC_TRAIT; ++i )
	{
		char szKey[MAX_PATH] = "";
		sprintf_s( szKey, MAX_PATH, "basic_max_point%d", i + 1 );
		m_BasicMaxPoint[i] = kLoader.LoadInt( szKey, 0 );
	}

	kLoader.SetTitle( "core_info" );
	m_iMaxPointMain  = kLoader.LoadInt( "max_point_main", 1 );
	m_iMaxPointSub1  = kLoader.LoadInt( "max_point_sub1", 10 );
	m_iMaxPointSub2  = kLoader.LoadInt( "max_point_sub2", 1 );
	m_iLimitLevelMain = kLoader.LoadInt( "limit_level_main", 30 );
	m_iLimitLevelSub1 = kLoader.LoadInt( "limit_level_sub1", 50 );
	m_iLimitLevelSub2 = kLoader.LoadInt( "limit_level_sub2", 70 );
}

int IndividualityManager::GetBasicMaxPoint( int iIndex ) const
{
	if( iIndex < 0 || iIndex >= MAX_BASIC_TRAIT )
		return 0;
	return m_BasicMaxPoint[iIndex];
}

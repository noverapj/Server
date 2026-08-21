
#include "stdafx.h"
#include "ioUserIndividuality.h"

#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"
#include "../MainProcess.h"
#include "../EtcHelpFunc.h"
#include "../Util/cSerialize.h"

#include "User.h"

ioUserIndividuality::ioUserIndividuality()
{
	Initialize( NULL );
}

ioUserIndividuality::~ioUserIndividuality()
{
	ClearList();
}

void ioUserIndividuality::ClearList()
{
	m_IndividualityList.clear();
}

void ioUserIndividuality::Initialize( User *pUser )
{
	m_pUser = pUser;
	ClearList();
}

IndividualityDB* ioUserIndividuality::FindIndividuality( int iClassType )
{
	vIndividualityDB::iterator iter, iEnd;
	iEnd = m_IndividualityList.end();
	for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
	{
		if( iter->m_iClassType == iClassType )
			return &( *iter );
	}
	return NULL;
}

void ioUserIndividuality::SetChange( int iClassType )
{
	IndividualityDB *pData = FindIndividuality( iClassType );
	if( pData )
		pData->m_bChange = true;
}

void ioUserIndividuality::AddIndividuality( int iClassType )
{
	IndividualityDB kData;
	kData.m_iClassType = iClassType;
	m_IndividualityList.push_back( kData );
}

int ioUserIndividuality::GetBasicTrait( int iClassType, int iIndex )
{
	if( iIndex < 0 || iIndex >= MAX_BASIC_TRAIT )
		return 0;

	IndividualityDB *pData = FindIndividuality( iClassType );
	if( pData )
		return pData->m_BasicTrait[iIndex];
	return 0;
}

int ioUserIndividuality::GetCoreTrait( int iClassType, int iIndex )
{
	if( iIndex < 0 || iIndex >= MAX_CORE_TRAIT )
		return 0;

	IndividualityDB *pData = FindIndividuality( iClassType );
	if( pData )
		return pData->m_CoreTrait[iIndex];
	return 0;
}

void ioUserIndividuality::SetBasicTrait( int iClassType, int iIndex, int iValue )
{
	if( iIndex < 0 || iIndex >= MAX_BASIC_TRAIT )
		return;

	IndividualityDB *pData = FindIndividuality( iClassType );
	if( pData )
	{
		pData->m_BasicTrait[iIndex] = iValue;
		pData->m_bChange = true;
	}
}

void ioUserIndividuality::SetCoreTrait( int iClassType, int iIndex, int iValue )
{
	if( iIndex < 0 || iIndex >= MAX_CORE_TRAIT )
		return;

	IndividualityDB *pData = FindIndividuality( iClassType );
	if( pData )
	{
		pData->m_CoreTrait[iIndex] = iValue;
		pData->m_bChange = true;
	}
}

void ioUserIndividuality::InsertDBIndividuality( int iClassType )
{
	if( !m_pUser )
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::InsertDBIndividuality() User NULL!!" );
		return;
	}

	IndividualityDB *pData = FindIndividuality( iClassType );
	if( !pData )
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::InsertDBIndividuality() Data NOT FOUND : %d", iClassType );
		return;
	}

	cSerialize v_FT;
	v_FT.Write( m_pUser->GetUserIndex() );
	v_FT.Write( pData->m_iClassType );
	for( int i = 0; i < MAX_BASIC_TRAIT; ++i )
		v_FT.Write( pData->m_BasicTrait[i] );
	for( int i = 0; i < MAX_CORE_TRAIT; ++i )
		v_FT.Write( pData->m_CoreTrait[i] );

	g_DBClient.OnInsertIndividuality( m_pUser->GetUserDBAgentID(), m_pUser->GetAgentThreadID(), m_pUser->GetGUID(), m_pUser->GetPublicID(), m_pUser->GetUserIndex(), v_FT );
}

bool ioUserIndividuality::DBtoNewIndex( DWORD dwIndex )
{
	if( !m_pUser )
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::DBtoNewIndex() User NULL!!" );
		return false;
	}

	{
		bool bEmptyIndex = false;
		vIndividualityDB::iterator iter, iEnd;
		iEnd = m_IndividualityList.end();
		for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
		{
			if( iter->m_dwIndex == NEW_INDEX )
			{
				bEmptyIndex = true;
				break;
			}
		}

		if( !bEmptyIndex )
		{
			LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::DBtoNewIndex() None Empty Index : %s - %d", m_pUser->GetPublicID().c_str(), dwIndex );
			return false;
		}
	}

	{
		vIndividualityDB::iterator iter, iEnd;
		iEnd = m_IndividualityList.end();
		for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
		{
			if( iter->m_dwIndex == dwIndex )
			{
				LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::DBtoNewIndex() Already Index : %s - %d", m_pUser->GetPublicID().c_str(), dwIndex );
				return false;
			}
		}
	}

	{
		vIndividualityDB::iterator iter, iEnd;
		iEnd = m_IndividualityList.end();
		for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
		{
			if( iter->m_dwIndex == NEW_INDEX )
			{
				iter->m_dwIndex = dwIndex;
				LOG.PrintTimeAndLog( LOG_TEST_LEVEL, "ioUserIndividuality::DBtoNewIndex() Add : %s - %d", m_pUser->GetPublicID().c_str(), dwIndex );
				return true;
			}
		}
	}
	return false;
}

void ioUserIndividuality::DBtoData( CQueryResultData *query_data )
{
	if( !m_pUser )
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "%s User NULL!!", __FUNCTION__ );
		return;
	}

	Initialize( m_pUser );

	LOOP_GUARD();
	while( query_data->IsExist() )
	{
		IndividualityDB kData;
		PACKET_GUARD_BREAK( query_data->GetValue( kData.m_dwIndex, sizeof(int) ) );
		PACKET_GUARD_BREAK( query_data->GetValue( kData.m_iClassType, sizeof(int) ) );

		for( int i = 0; i < MAX_BASIC_TRAIT; ++i )
		{
			PACKET_GUARD_BREAK( query_data->GetValue( kData.m_BasicTrait[i], sizeof(int) ) );
		}

		for( int i = 0; i < MAX_CORE_TRAIT; ++i )
		{
			PACKET_GUARD_BREAK( query_data->GetValue( kData.m_CoreTrait[i], sizeof(int) ) );
		}

		m_IndividualityList.push_back( kData );
	}
	LOOP_GUARD_CLEAR();

	SendIndividualityData();
}

void ioUserIndividuality::SaveData()
{
	if( m_IndividualityList.empty() ) return;
	if( !m_pUser )
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "ioUserIndividuality::SaveData() User NULL!!" );
		return;
	}

	vIndividualityDB::iterator iter, iEnd;
	iEnd = m_IndividualityList.end();
	for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
	{
		IndividualityDB &kData = *iter;
		if( kData.m_bChange )
		{
			cSerialize v_FT;
			v_FT.Write( kData.m_dwIndex );
			v_FT.Write( kData.m_iClassType );
			for( int i = 0; i < MAX_BASIC_TRAIT; ++i )
			{
				v_FT.Write( kData.m_BasicTrait[i] );
			}
			for( int i = 0; i < MAX_CORE_TRAIT; ++i )
			{
				v_FT.Write( kData.m_CoreTrait[i] );
			}

			if( kData.m_dwIndex == NEW_INDEX )
			{
				LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "SaveIndividuality(%s:%d) None Index", m_pUser->GetPublicID().c_str(), kData.m_dwIndex );
			}
			else
			{
				g_DBClient.OnUpdateIndividuality( m_pUser->GetUserDBAgentID(), m_pUser->GetAgentThreadID(), m_pUser->GetUserIndex(), kData.m_dwIndex, v_FT );
				kData.m_bChange = false;
				LOG.PrintTimeAndLog( LOG_TEST_LEVEL, "SaveIndividuality(%s:%d)", m_pUser->GetPublicID().c_str(), kData.m_dwIndex );
			}
		}
	}
}

void ioUserIndividuality::FillMoveData( SP2Packet &rkPacket )
{
	rkPacket << (int)m_IndividualityList.size();

	vIndividualityDB::iterator iter, iEnd;
	iEnd = m_IndividualityList.end();
	for( iter = m_IndividualityList.begin(); iter != iEnd; ++iter )
	{
		IndividualityDB &kData = *iter;
		rkPacket << kData.m_dwIndex << kData.m_bChange << kData.m_iClassType;

		for( int i = 0; i < MAX_BASIC_TRAIT; ++i )
			rkPacket << kData.m_BasicTrait[i];

		for( int i = 0; i < MAX_CORE_TRAIT; ++i )
			rkPacket << kData.m_CoreTrait[i];
	}
}

void ioUserIndividuality::ApplyMoveData( SP2Packet &rkPacket, bool bDummyNode )
{
	ClearList();

	int iSize = 0;
	rkPacket >> iSize;

	for( int i = 0; i < iSize; ++i )
	{
		IndividualityDB kData;
		rkPacket >> kData.m_dwIndex >> kData.m_bChange >> kData.m_iClassType;

		for( int j = 0; j < MAX_BASIC_TRAIT; ++j )
			rkPacket >> kData.m_BasicTrait[j];

		for( int j = 0; j < MAX_CORE_TRAIT; ++j )
			rkPacket >> kData.m_CoreTrait[j];

		if( kData.m_dwIndex == NEW_INDEX && !bDummyNode )
		{
			g_DBClient.OnSelectIndividualityIndex( m_pUser->GetUserDBAgentID(), m_pUser->GetAgentThreadID(), m_pUser->GetGUID(), m_pUser->GetPublicID(), m_pUser->GetUserIndex() );
		}

		m_IndividualityList.push_back( kData );
	}
}

void ioUserIndividuality::SendIndividualityData()
{
	if( !m_pUser )
		return;

	SP2Packet kPacket( STPK_USER_INDIVIDUALITY_DATA );
	int iSize = (int)m_IndividualityList.size();
	kPacket << iSize;

	for( int i = 0; i < iSize; ++i )
	{
		IndividualityDB &kData = m_IndividualityList[i];
		kPacket << kData.m_iClassType;

		for( int j = 0; j < MAX_BASIC_TRAIT; ++j )
			kPacket << kData.m_BasicTrait[j];

		for( int j = 0; j < MAX_CORE_TRAIT; ++j )
			kPacket << kData.m_CoreTrait[j];
	}

	m_pUser->SendMessage( kPacket );
}

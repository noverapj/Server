#pragma once
#include "ioDBDataController.h"

#define MAX_BASIC_TRAIT	8
#define MAX_CORE_TRAIT	3

struct IndividualityDB
{
	DWORD m_dwIndex;
	bool m_bChange;
	int m_iClassType;
	int m_BasicTrait[MAX_BASIC_TRAIT];
	int m_CoreTrait[MAX_CORE_TRAIT];

	IndividualityDB()
	{
		m_dwIndex = ioDBDataController::NEW_INDEX;
		m_bChange = false;
		m_iClassType = 0;
		memset( m_BasicTrait, 0, sizeof(m_BasicTrait) );
		memset( m_CoreTrait, 0, sizeof(m_CoreTrait) );
	}

	void Init()
	{
		m_bChange = false;
		m_iClassType = 0;
		memset( m_BasicTrait, 0, sizeof(m_BasicTrait) );
		memset( m_CoreTrait, 0, sizeof(m_CoreTrait) );
	}
};

typedef std::vector< IndividualityDB > vIndividualityDB;

class ioUserIndividuality : public ioDBDataController
{
protected:
	vIndividualityDB m_IndividualityList;

public:
	virtual void Initialize( User *pUser );
	virtual bool DBtoNewIndex( DWORD dwIndex );
	virtual void DBtoData( CQueryResultData *query_data );
	virtual void SaveData();
	virtual void FillMoveData( SP2Packet &rkPacket );
	virtual void ApplyMoveData( SP2Packet &rkPacket, bool bDummyNode = false );

public:
	void ClearList();
	IndividualityDB* FindIndividuality( int iClassType );
	void SetChange( int iClassType );
	void AddIndividuality( int iClassType );
	void InsertDBIndividuality( int iClassType );

	int GetBasicTrait( int iClassType, int iIndex );
	int GetCoreTrait( int iClassType, int iIndex );
	void SetBasicTrait( int iClassType, int iIndex, int iValue );
	void SetCoreTrait( int iClassType, int iIndex, int iValue );

	void SendIndividualityData();

public:
	ioUserIndividuality();
	virtual ~ioUserIndividuality();
};

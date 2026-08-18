#include "stdafx.h"

#include "../DataBase/DBClient.h"
#include "../DataBase/LogDBClient.h"
#include "../Util/cSerialize.h"
#include "../EtcHelpFunc.h"

#include "User.h"
#include "Room.h"
#include "ioEtcItemManager.h"

#include "CustomMedal.h"
#include "ioMedalItemInfoManager.h"

CustomMedal::CustomMedal()
{
	Init();
}

CustomMedal::~CustomMedal()
{
	
}

void CustomMedal::Init()
{
	m_iItemCode  = 0;
	m_iEquipClass = 0;
	m_iPeriodType = 0;
	m_iLimitDate = 0;
	m_iLimitTime = 0;

	for( int i = 0; i < MEDAL_STAT; ++i )
		m_iGrowth[i]	= 0;

	for( int i = 0; i < MEDAL_STAT; ++i )
		m_bGrowth[i]	= false;

	m_iPresentIndex = 0;
	m_iPresentSlotIndex = 0;
	m_iItemIndex = 0;
	m_bStatSelectDone = false;
}

void CustomMedal::FillMoveData( SP2Packet &rkPacket )
{
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iItemCode);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iEquipClass);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iPeriodType)
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iLimitDate);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iLimitTime);
	for( int i = 0; i< MEDAL_STAT; i++)
	{
		PACKET_GUARD_VOID_WRITE(rkPacket, m_iGrowth[i]);
	}

	for( int i = 0; i< MEDAL_STAT; i++)
	{
		PACKET_GUARD_VOID_WRITE(rkPacket, m_bGrowth[i]);
	}

	PACKET_GUARD_VOID_WRITE(rkPacket, m_iPresentIndex);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iPresentSlotIndex);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_iItemIndex);
	PACKET_GUARD_VOID_WRITE(rkPacket, m_bStatSelectDone);

}

void CustomMedal::ApplyMoveData( SP2Packet &rkPacket )
{
	PACKET_GUARD_VOID_READ(rkPacket, m_iItemCode);
	PACKET_GUARD_VOID_READ(rkPacket, m_iEquipClass);
	PACKET_GUARD_VOID_READ(rkPacket, m_iPeriodType)
		PACKET_GUARD_VOID_READ(rkPacket, m_iLimitDate);
	PACKET_GUARD_VOID_READ(rkPacket, m_iLimitTime);
	for( int i = 0; i< MEDAL_STAT; i++)
	{
		PACKET_GUARD_VOID_READ(rkPacket, m_iGrowth[i]);
	}

	for( int i = 0; i< MEDAL_STAT; i++)
	{
		PACKET_GUARD_VOID_READ(rkPacket, m_bGrowth[i]);
	}

	PACKET_GUARD_VOID_READ(rkPacket, m_iPresentIndex);
	PACKET_GUARD_VOID_READ(rkPacket, m_iPresentSlotIndex);
	PACKET_GUARD_VOID_READ(rkPacket, m_iItemIndex);
	PACKET_GUARD_VOID_READ(rkPacket, m_bStatSelectDone);
}
#include "stdafx.h"

#include "ChannelCopyNode.h"
#include "ServerNode.h"

ChannelCopyNode::ChannelCopyNode()
{
	InitData();
}

ChannelCopyNode::~ChannelCopyNode()
{
}

void ChannelCopyNode::OnCreate( ServerNode *pCreator )
{
	CopyNodeParent::OnCreate( pCreator );
	m_eCopyType = CHANNEL_TYPE;
	InitData();
}

void ChannelCopyNode::OnDestroy()
{
	CopyNodeParent::OnDestroy();
}

void ChannelCopyNode::InitData()
{
	// 기본 정보
	m_dwIndex = 0;
}

void ChannelCopyNode::EnterChannel( const DWORD dwUserIndex, const ioHashString &szUserName )
{
	// 원본 채널로 패킷 전송
	SP2Packet kPacket( SSTPK_CHANNEL_SYNC );

	PACKET_GUARD_VOID_WRITE(kPacket, ChannelParent::CHANNEL_ENTER);
	PACKET_GUARD_VOID_WRITE(kPacket, GetIndex()); 
	PACKET_GUARD_VOID_WRITE(kPacket, dwUserIndex);
	PACKET_GUARD_VOID_WRITE(kPacket, szUserName);

	SendMessage( kPacket );
}

void ChannelCopyNode::LeaveChannel( const DWORD dwUserIndex, const ioHashString &szUserName )
{	
	// 원본 채널로 패킷 전송
	SP2Packet kPacket( SSTPK_CHANNEL_SYNC );

	PACKET_GUARD_VOID_WRITE(kPacket, ChannelParent::CHANNEL_LEAVE);
	PACKET_GUARD_VOID_WRITE(kPacket, GetIndex());
	PACKET_GUARD_VOID_WRITE(kPacket, dwUserIndex);
	PACKET_GUARD_VOID_WRITE(kPacket, szUserName);

	SendMessage( kPacket );
}

void ChannelCopyNode::SendPacketTcp( SP2Packet &rkPacket, DWORD dwUserIndex )
{
	// 원본 채널로 패킷 전송
	SP2Packet kPacket( SSTPK_CHANNEL_SYNC );

	PACKET_GUARD_VOID_WRITE(kPacket, ChannelParent::CHANNEL_TRANSFER);
	PACKET_GUARD_VOID_WRITE(kPacket, GetIndex());
	PACKET_GUARD_VOID_WRITE(kPacket, dwUserIndex);
	PACKET_GUARD_VOID_WRITE(kPacket, rkPacket.GetPacketID());

	kPacket.SetDataAdd( (char*)rkPacket.GetData(), rkPacket.GetDataSize() );
	SendMessage( kPacket );
}
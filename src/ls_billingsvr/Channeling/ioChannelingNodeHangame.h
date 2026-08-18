#pragma once

#include "ioChannelingNodeParent.h"

class ioChannelingNodeHangame : public ioChannelingNodeParent
{
public:
	ioChannelingNodeHangame();
	virtual ~ioChannelingNodeHangame();

	void Init();
	void Destroy();

protected:
	virtual void _OnGetCash( ServerNode *pServerNode, SP2Packet &rkPacket );
	virtual void _OnOutputCash( ServerNode *pServerNode, SP2Packet &rkPacket, DWORD dwGoodsNo, const ioHashString &rszGoodsName );

public:
	virtual void ThreadGetCash( const ioData &rData );
	virtual void ThreadOutputCash( const ioData &rData );

protected:
	BOOL ParseGetCash(const char* szReturnData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString);

	BOOL ParsePurchase(const char* szReturnData, ioHashString& szBuyNo, ioData& rData, int& iRealCash, int& iBonus, int& iResult, ioHashString& szErrString);

	void AnsiToUTF8( IN const char *szAnsi, OUT char *szUTF8, OUT int &riReturnUTF8Size, IN int iUTF8BufferSize );
	void UTF8ToAnsi( IN const char *szUTF8, OUT char *szAnsi, OUT int &riReturnAnsiSize, IN int iAnsiBufferSize );

public:
	virtual ChannelingType GetType() { return CNT_HANGAME; }

public:
	void TestGetURL();

protected:
	ioHashString m_szGetURL;
	ioHashString m_szBuyURL;
};
// QueryResultData.cpp: implementation of the QueryResultData class.
//
//////////////////////////////////////////////////////////////////////

#include "../stdafx.h"
#include "QueryResultData.h"

extern CLog LOG;

#ifdef __OHTG_LOCAL_PHILIPPINE__

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQueryResultData::CQueryResultData() : m_nValuePos(0)
{
}
CQueryResultData::~CQueryResultData()
{
	CQueryResult::Clear();
}

BOOL CQueryResultData::IsValid(const int nLength)
{
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid - failed");
		return FALSE;
	}

	if((m_nValuePos + nLength) > m_queryResultHeader.nResultBufferSize)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CQueryResultData::GetValue(bool &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[bool,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(int &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[int,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(long &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[long,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(DWORD &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[DWORD,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(char *value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[char,0x%x]", GetMsgType());
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::GetValue Size Error - m_nValuePos + nLength:%d > ResultBufferSize:%d", m_nValuePos + nLength, m_queryResultHeader.nResultBufferSize );

		return FALSE;
	}

	memcpy(value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(__int64 &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[int64,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(short &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[short,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(WORD &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[short,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::GetValue(ioHashString &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[string,0x%x]", GetMsgType());
		return FALSE;
	}

	char szBuf[MAX_PATH] = "";
	if( nLength < MAX_PATH )
	{
		memcpy(szBuf,&m_pBuffer[m_nValuePos],nLength);
		value = szBuf;
		m_nValuePos += nLength;	
		return TRUE;
	}	
	else
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"Result GetValue Size Error!![0x%x] : %d", GetMsgType(), nLength );
		return FALSE;
	}
}

BOOL CQueryResultData::GetValue(BYTE &value,int nLength)
{
	if(!IsValid(nLength))
	{
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL,"CQueryResultData::IsValid[BYTE,0x%x]", GetMsgType());
		return FALSE;
	}

	memcpy(&value,&m_pBuffer[m_nValuePos],nLength);
	m_nValuePos += nLength;
	return TRUE;
}

BOOL CQueryResultData::IsExist() 
{
	if( m_nValuePos < m_queryResultHeader.nResultBufferSize ) 
		return TRUE;
	return FALSE;
}

void CQueryResultData::SetBuffer(const char *buffer)
{
	memcpy(&m_queryResultHeader,buffer,sizeof(QueryResultHeader));
	buffer += sizeof(QueryResultHeader);
	m_pBuffer = new char[m_queryResultHeader.nResultBufferSize];
	memset(m_pBuffer,0,m_queryResultHeader.nResultBufferSize);
	memcpy(m_pBuffer,buffer,m_queryResultHeader.nResultBufferSize);
}

void CQueryResultData::SetResultData(
									 unsigned int nIndex,
									 int nMsgType,
									 int nResultType,
									 char *pResultData, 
									 int nResultSize,
									 int nResultCount )
{
	CQueryResult::Clear();
	m_queryResultHeader.nIndex				= nIndex;
	m_queryResultHeader.nMsgType			= nMsgType;
	m_queryResultHeader.nQueryResultType	= nResultType;
	m_queryResultHeader.nResultBufferSize	= nResultSize;
	m_queryResultHeader.nResultCount		= nResultCount;
	
	if(nResultSize != 0)
	{
		m_pBuffer = new char[nResultSize];
		memset(m_pBuffer,0,nResultSize);
		memcpy(m_pBuffer, pResultData, nResultSize);
	}
	else 
		m_pBuffer = NULL;
}

#else //__OHTG_LOCAL_PHILIPPINE__

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQueryResultData::CQueryResultData()
{
	m_pBuffer = NULL;
	CQueryResult::Clear();
	InitPos();
}
CQueryResultData::~CQueryResultData()
{
	CQueryResult::Clear();
}

bool CQueryResultData::IsExist() 
{
	if( m_nValuePos < m_queryResultHeader.nResultBufferSize ) 
		return true;
	return false;
}

void CQueryResultData::GetValue(bool &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(int &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(long &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(DWORD &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(char *value,int len)
{
	if((m_nValuePos + len) > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(__int64 &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::GetValue(short &value,int len)
{
	if(m_nValuePos + len > m_queryResultHeader.nResultBufferSize)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer OVER!!");
		return;
	}
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Result Buffer NULL POINTER!!");
		return;
	}
	memcpy(&value,&m_pBuffer[m_nValuePos],len);
	m_nValuePos += len;
}

void CQueryResultData::SetBuffer(const char *buffer)
{
	memcpy(&m_queryResultHeader,buffer,sizeof(QueryResultHeader));
	buffer += sizeof(QueryResultHeader);
	m_pBuffer = new char[m_queryResultHeader.nResultBufferSize];
	memset(m_pBuffer,0,m_queryResultHeader.nResultBufferSize);
	memcpy(m_pBuffer,buffer,m_queryResultHeader.nResultBufferSize);
}

void CQueryResultData::SetResultData(unsigned int socket,int nMsgType, int nResultSize,
									 int nResultType,int SetValueCnt,
									 char *pResultData)
{
	CQueryResult::Clear();
	m_queryResultHeader.csocket          = socket;
	m_queryResultHeader.nMsgType		 = nMsgType;
	m_queryResultHeader.nQueryResultType = nResultType;
	m_queryResultHeader.SetValueCnt      = SetValueCnt;
	m_queryResultHeader.nResultBufferSize= nResultSize;
	
	if(nResultSize != 0)
	{
		m_pBuffer = new char[nResultSize];
		memset(m_pBuffer,0,nResultSize);
		memcpy(m_pBuffer,pResultData,sizeof(char) * nResultSize);
	}
	else 
		m_pBuffer = NULL;
}
#endif //__OHTG_LOCAL_PHILIPPINE__
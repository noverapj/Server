// QueryData.cpp: implementation of the CQueryData class.
//
//////////////////////////////////////////////////////////////////////

#include "../stdafx.h"
#include "../util/cSerialize.h"
#include "QueryData.h"

extern CLog LOG;

#ifdef __OHTG_LOCAL_PHILIPPINE__
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQueryData::CQueryData()
{
	memset( m_szReturnBuf, 0, sizeof( m_szReturnBuf ) );
	m_iReturnLength = 0;

	m_pBuffer = NULL;
	CQuery::Clear();
}

CQueryData::~CQueryData()
{
	CQuery::Clear();
}

void CQueryData::GetFields(cSerialize& fieldTypes)
{
	if(GetFieldSize() == 0) 
		return;

	if(m_pBuffer)
	{
		uint32 index = sizeof(int);

		fieldTypes.Reset();
		fieldTypes.SetBuffer( (uint8*)(m_pBuffer + index), GetFieldSize() );
	}
}

void CQueryData::GetResults(vVALUETYPE& valueTypes)
{
	valueTypes.clear();

	if(GetResultSize() == 0) 
		return;

	if(m_pBuffer)
	{
		uint32 index = sizeof(int) + GetFieldSize();
		for(int32 n = 0 ; n < GetResultSize() ; n += sizeof(ValueType) )
		{
			ValueType* type = (ValueType*)(m_pBuffer + index + n);
			valueTypes.push_back( *type );
		}
	}
}

void CQueryData::GetReturns(char* buffer, int &size)
{
	if(m_pBuffer)
	{
		uint32 index = sizeof(int) + GetFieldSize() + GetResultSize();
		size = GetReturnSize();
		memcpy( buffer, m_pBuffer + index, size );
	}
}

void CQueryData::SetReturnData( const void *pData, int iSize )
{
	memcpy( &m_szReturnBuf[m_iReturnLength], pData, iSize );
	m_iReturnLength += iSize;

	if( m_iReturnLength >= MAX_BUFFER )
		LOG.PrintTimeAndLog( LOG_DEBUG_LEVEL, "CQueryData::SetReturnData Return Size : %d", m_iReturnLength );
}

void CQueryData::SetBuffer(const char *buffer)
{
	memcpy(&m_queryHeader,buffer,sizeof(QueryHeader));
	buffer+= sizeof(QueryHeader);
	m_pBuffer = new char[m_queryHeader.nQueryBufferSize];
	memcpy(m_pBuffer,buffer,m_queryHeader.nQueryBufferSize);		
}

void CQueryData::SetData(
						 unsigned int nIndex,
						 int nResultType,
						 int nMsgType, 
						 int nQueryType,
						 int nQueryId,
						 cSerialize& fieldTypes,
						 vVALUETYPE& valueTypes, 
						 int nSetValue)
{
	CQuery::Clear();
	//헤더
	m_queryHeader.nIndex            = nIndex;
	m_queryHeader.nResultType		= nResultType;
	m_queryHeader.nMsgType			= nMsgType;
	m_queryHeader.nQueryType		= nQueryType;
	m_queryHeader.nFieldLength		= fieldTypes.GetLength();
	m_queryHeader.nResultLength		= sizeof(ValueType) * valueTypes.size();
	m_queryHeader.nValueTypeCnt		= 0;
	m_queryHeader.nReturnLength		= m_iReturnLength;
	m_queryHeader.nSetValue         = nSetValue;
	m_queryHeader.nQueryId			= nQueryId;
	m_queryHeader.nDatabaseId		= 1;
	m_queryHeader.nQueryBufferSize  = sizeof(nQueryId) + m_queryHeader.nFieldLength + m_queryHeader.nResultLength + m_queryHeader.nReturnLength;	

	//버퍼 메모리 할당.
	if(m_queryHeader.nQueryBufferSize != 0)
	{
		m_pBuffer = new char[m_queryHeader.nQueryBufferSize];
		//데이터.
		int nSize = 0;                     //버퍼의 위치
		int nlen = 0;
		if(0 != nQueryId)
		{
			nlen = sizeof(nQueryId);        //카피할 데이터 사이즈
			memcpy(&m_pBuffer[nSize], &nQueryId, nlen);					 //쿼리 내용
		}
		if(fieldTypes.GetLength() > 0)
		{
			nSize += nlen;                      
			nlen = m_queryHeader.nFieldLength;
			memcpy(&m_pBuffer[nSize], fieldTypes.GetBuffer(), nlen);      // 필드 타입
		}
		if(valueTypes.size() > 0)
		{
			nSize += nlen;                      
			nlen = m_queryHeader.nResultLength;
			memcpy(&m_pBuffer[nSize], &valueTypes[0], nlen);      // 결과 타입
		}
		if(m_iReturnLength > 0)
		{
			nSize += nlen;
			memcpy(&m_pBuffer[nSize], m_szReturnBuf, m_iReturnLength);   //돌려받을 데이터.
		}
	}
}

void CQueryData::SetData( unsigned int nIndex, int nMsgType, int nQueryID, cSerialize& fieldTypes, int DBID /*= 1*/ )
{
	m_queryHeader.Clear();
	m_queryHeader.nIndex				= nIndex;
	m_queryHeader.nMsgType			= nMsgType;
	m_queryHeader.nFieldLength		= fieldTypes.GetLength();
	//m_queryHeader.nReturnLength		= m_Return->GetLength();
	m_queryHeader.nQueryId			= nQueryID;
	m_queryHeader.nQueryBufferSize	= sizeof(nQueryID) + m_queryHeader.nFieldLength + m_queryHeader.nReturnLength;	
	m_queryHeader.nDatabaseId				= DBID;

	m_pBuffer = new char[m_queryHeader.nQueryBufferSize];

	// 버퍼 메모리 할당
	if(m_queryHeader.nQueryBufferSize != 0)
	{
		m_pBuffer = new char[m_queryHeader.nQueryBufferSize];
		//데이터.
		int nSize = 0;                     //버퍼의 위치
		int nlen = 0;
		if(0 != nQueryID)
		{
			nlen = sizeof(nQueryID);        //카피할 데이터 사이즈
			memcpy(&m_pBuffer[nSize], &nQueryID, nlen);					 //쿼리 내용
		}
		if(fieldTypes.GetLength() > 0)
		{
			nSize += nlen;                      
			nlen = m_queryHeader.nFieldLength;
			memcpy(&m_pBuffer[nSize], fieldTypes.GetBuffer(), nlen);      // 필드 타입
		}
		if(m_iReturnLength > 0)
		{
			nSize += nlen;
			memcpy(&m_pBuffer[nSize], m_szReturnBuf, m_iReturnLength);   //돌려받을 데이터.
		}
	}
}

void CQueryData::Copy( CQueryData &queryData )
{
	if(m_pBuffer)
	{
		delete []m_pBuffer;
	}

	m_pBuffer = new char[queryData.GetBufferSize()];
	if(m_pBuffer!=NULL)
	{
		m_queryHeader = queryData.m_queryHeader;
		memcpy( m_pBuffer, queryData.m_pBuffer, m_queryHeader.nQueryBufferSize );
	}
}

#else //__OHTG_LOCAL_PHILIPPINE__
// QueryData.cpp: implementation of the CQueryData class.
//
//////////////////////////////////////////////////////////////////////

#include "../stdafx.h"
#include "QueryData.h"

extern CLog LOG;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQueryData::CQueryData()
{
	memset( m_szReturnBuf, 0, sizeof( m_szReturnBuf ) );
	m_iReturnCount = 0;

	m_pBuffer = NULL;
	CQuery::Clear();
}

CQueryData::~CQueryData()
{
	CQuery::Clear();
}

char *CQueryData::NewResultBuffer(int &nSize)
{
	//무조건 돌려줘야할 데이터 사이즈.
	nSize = GetBufferSize();
	nSize -= ((sizeof(ValueType) * GetValueCnt()) + GetQuerySize());
	//데이터를 뽑아서 돌려야할 사이즈.
	for(int i = 0;i < GetValueCnt();i++)
		nSize += GetValueType(i).size;
	
	return new char[nSize];
}

char *CQueryData::GetReturnResultData(int &nSize)
{
	char *pBuffer = GetBuffer();
	pBuffer += GetQuerySize();      //앞의 쿼리부분 넘김.
	pBuffer += (sizeof(ValueType) * GetValueCnt());  //데이터 타입 부분 넘김.
	nSize = GetBufferSize();
	nSize -= ((sizeof(ValueType) * GetValueCnt()) + GetQuerySize());
	return pBuffer;
}

char *CQueryData::GetQuery(int &nLen)
{
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Query Buffer NULL POINTER!!");
		return NULL;
	}
	nLen = m_queryHeader.nQuerySize;
	
	return &m_pBuffer[0];
}
ValueType CQueryData::GetValueType(int nPos)
{
	ValueType vt;
	if(m_pBuffer == NULL)
	{
		LOG.PrintTimeAndLog(0,"Query Buffer NULL POINTER!!");
		return vt;
	}
	int nCnt = (sizeof(ValueType)*nPos)+m_queryHeader.nQuerySize;
	memcpy(&vt,&m_pBuffer[nCnt],sizeof(ValueType));
	
	return vt;
}

void CQueryData::SetReturnData( const void *pData, int iSize )
{
	memcpy( &m_szReturnBuf[m_iReturnCount], pData, iSize );
	m_iReturnCount += iSize;

	if( m_iReturnCount >= MAX_BUFFER )
		LOG.PrintTimeAndLog( 0, "CQueryData::SetReturnData Return Size : %d", m_iReturnCount );
}

void CQueryData::SetBuffer(const char *buffer)
{
	memcpy(&m_queryHeader,buffer,sizeof(QueryHeader));
	buffer+= sizeof(QueryHeader);
	m_pBuffer = new char[m_queryHeader.nQueryBufferSize];
	memcpy(m_pBuffer,buffer,m_queryHeader.nQueryBufferSize);		
}

void CQueryData::SetData(unsigned int socket,int nResult_Type,int nMsgType, int nQueryType,
						 char *szQuery, int nQuerySize,
						 ValueType* pTypes, int nTypeCount,int nSetValue)
{
	CQuery::Clear();
	//헤더
	m_queryHeader.csocket           = socket;
	m_queryHeader.nResult_Type      = nResult_Type;
	m_queryHeader.nMsgType			= nMsgType;
	m_queryHeader.nQueryType		= nQueryType;
	m_queryHeader.nQuerySize		= nQuerySize;
	m_queryHeader.nValueTypeCnt		= nTypeCount;
	m_queryHeader.nReturnValueSize	= m_iReturnCount;
	m_queryHeader.nSetValue         = nSetValue;
	m_queryHeader.nQueryBufferSize  = nQuerySize + (sizeof(ValueType) * nTypeCount) + m_iReturnCount;	
	//버퍼 메모리 할당.
	if(m_queryHeader.nQueryBufferSize != 0)
	{
		m_pBuffer = new char[m_queryHeader.nQueryBufferSize];
		//데이터.
		int nSize = 0;                     //버퍼의 위치
		int nlen = 0;
		if(szQuery != NULL)
		{
			nlen = strlen(szQuery);        //카피할 데이터 사이즈
			memcpy(&m_pBuffer[nSize],szQuery,nlen);					 //쿼리 내용
		}
		if(pTypes != NULL)
		{
			nSize += nlen;                      
			nlen = sizeof(ValueType)*nTypeCount;
			memcpy(&m_pBuffer[nSize],pTypes,nlen);      //결과 타입.
		}
		if(m_iReturnCount > 0)
		{
			nSize += nlen;
			memcpy(&m_pBuffer[nSize],m_szReturnBuf,m_iReturnCount);   //돌려받을 데이터.
		}
	}
}

#endif //__OHTG_LOCAL_PHILIPPINE__
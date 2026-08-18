// QueryResultData.h: interface for the QueryResultData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_QUERYRESULTDATA_H__515580C5_4613_4052_B653_A37861F46C97__INCLUDED_)
#define AFX_QUERYRESULTDATA_H__515580C5_4613_4052_B653_A37861F46C97__INCLUDED_

#ifdef __OHTG_LOCAL_PHILIPPINE__

#pragma once

#include "../util/cSerialize.h"

struct QueryResultHeader
{
	QueryResultHeader()
	{
		nMsgType			= -1;
		nQueryResultType	= -1;
		nResultBufferSize	= 0;
		nResultCount		= 0;
		nIndex				= 0;
	}
	int nMsgType;
	int nQueryResultType;
	int nResultBufferSize;
	int nResultCount;
	unsigned int nIndex;
};

class CQueryResult
{
protected:
	QueryResultHeader m_queryResultHeader;
	char *m_pBuffer;

public:
	CQueryResult() : m_pBuffer(NULL)
	{
		Clear();
	}

	void Clear()
	{
		memset(&m_queryResultHeader,0,sizeof(QueryResultHeader));
		if(m_pBuffer != NULL)
		{
			delete[] m_pBuffer;
			m_pBuffer = NULL;
		}
	}
};


class CQueryResultData : public CQueryResult
{
	int m_nValuePos;
public:
	//HEADER
	int GetMsgType()			{ return m_queryResultHeader.nMsgType; }
	int GetResultType()			{ return m_queryResultHeader.nQueryResultType; }
	int GetResultBufferSize()	{ return m_queryResultHeader.nResultBufferSize; }
	int GetResultCount()		{ return m_queryResultHeader.nResultCount; }
	unsigned int GetIndex()		{ return m_queryResultHeader.nIndex; }

	//DATA
	BOOL IsValid(const int nLength);
	BOOL GetValue(bool &value,int nLength);
	BOOL GetValue(int &value,int nLength);
	BOOL GetValue(long &value,int nLength);
	BOOL GetValue(DWORD &value,int nLength);
	BOOL GetValue(char *value,int nLength);
	BOOL GetValue(__int64 &value,int nLength);
	BOOL GetValue(short &value,int nLength);
	BOOL GetValue(WORD &value, int nLength);
	BOOL GetValue(ioHashString &value,int nLength);
	BOOL GetValue( BYTE &value, int nLength );

	BOOL IsExist();

	QueryResultHeader *GetHeader(){ return &m_queryResultHeader; }
	char *GetBuffer(){ return m_pBuffer; }
	
public://SET
	void SetResultData(
		unsigned int nIndex,
		int nMsgType,
		int nResultType,
		char *pResultData, 
		int nResultSize,
		int nResultCount);

	void SetBuffer(const char *buffer);

public:
	CQueryResultData();
	virtual ~CQueryResultData();
};

#else //__OHTG_LOCAL_PHILIPPINE__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct tagQueryResultHeader
{
	int nMsgType;
	int nQueryResultType;
	int SetValueCnt;
	int nResultBufferSize;
	unsigned int csocket;
	tagQueryResultHeader()
	{
		nMsgType		 = -1;
		nQueryResultType = -1;
		SetValueCnt		 = -1;
		nResultBufferSize= -1;
		csocket			 = -1;
	}
}QueryResultHeader;


class CQueryResult
{
protected:
	QueryResultHeader	m_queryResultHeader;
	char		*m_pBuffer;
public:
	void Clear()
	{
		memset(&m_queryResultHeader,0,sizeof(QueryResultHeader));
		if(m_pBuffer != NULL)
		{
			delete[] m_pBuffer;
			m_pBuffer = NULL;
		}
	}
};

class CQueryResultData : public CQueryResult
{
	int m_nValuePos;
public://GET
	//HEADER
	int GetMsgType(){ return m_queryResultHeader.nMsgType; }
	int GetResultType(){ return m_queryResultHeader.nQueryResultType; }
	int GetValueCnt(){ return m_queryResultHeader.SetValueCnt;}
	int GetResultBufferSize(){ return m_queryResultHeader.nResultBufferSize; }
	unsigned int GetSocket(){ return m_queryResultHeader.csocket; }
	//DATA
	void GetValue(bool &vlaue,int len);
	void GetValue(int &vlaue,int len);
	void GetValue(long &vlaue,int len);
	void GetValue(DWORD &vlaue,int len);
	void GetValue(char *vlaue,int len);
	void GetValue(__int64 &value,int len);
	void GetValue(short &value,int len);
	bool IsExist();

	QueryResultHeader *GetHeader(){ return &m_queryResultHeader; }
	char *GetBuffer(){ return m_pBuffer; }
	
public://SET
	void SetResultData(unsigned int socket,int nMsgType, int nResultSize,
					   int nResultType,int SetValueCnt,char *pResultData);
	void SetBuffer(const char *buffer);
	
public:
	void InitPos(){ m_nValuePos = 0; }
	
public:
	CQueryResultData();
	virtual ~CQueryResultData();

};

#endif //__OHTG_LOCAL_PHILIPPINE__

#endif // !defined(AFX_QUERYRESULTDATA_H__515580C5_4613_4052_B653_A37861F46C97__INCLUDED_)

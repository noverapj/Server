#pragma once

class OpMemPool
{
public:
	OpMemPool(void);
	virtual ~OpMemPool(void);

public:
	virtual void Init();
	
public:
	int GetSize()
	{
		return 0;
	}

	template<typename TYPE>
	void Push(TYPE* data)
	{
		IncrementCount();
		int dataSize = sizeof(*data);
		memset((TCHAR*)data,0,dataSize);
		free((void*)data);
	}

	void* Pop(int dataSize)
	{
		DecrementCount();
		return malloc(dataSize);
	}

protected:
	void DecrementCount();
	void IncrementCount();
};


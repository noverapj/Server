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
		Increment();
		int dataSize = sizeof(*data);
		memset((TCHAR*)data,0,dataSize);
		free((void*)data);
	}

	void* Pop(int dataSize)
	{
		Decrement();
		return malloc(dataSize);
	}

protected:
	void Decrement();
	void Increment();
};

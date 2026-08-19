#pragma once

template<class T>
class BoostPooler
{
public:
	static void* operator new(size_t size)
	{
		return malloc(size);
	}
	static void operator delete(void* p)
	{
		free(p);
	}
};

template<class T>
class BoostPoolerL
{
public:
	static void* operator new(size_t size)
	{
		return malloc(size);
	}
	static void operator delete(void* p)
	{
		free(p);
	}
};

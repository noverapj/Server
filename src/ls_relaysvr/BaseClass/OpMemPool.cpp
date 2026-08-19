#include "StdAfx.h"
#include "OpMemPool.h"
 

OpMemPool::OpMemPool(void)
{
}

OpMemPool::~OpMemPool(void)
{
}

void OpMemPool::Init()
{
	 
}

void OpMemPool::Increment()
{
	g_State()->IncrementOPPool();
}

void OpMemPool::Decrement()
{
	g_State()->DecrementOPPool();
}
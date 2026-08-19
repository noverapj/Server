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

void OpMemPool::IncrementCount()
{
	g_State()->IncrementOPPool();
}

void OpMemPool::DecrementCount()
{
	g_State()->DecrementOPPool();
}
#include "MemoryManager.h"

#define MAX_TEST_NUM 1000

MemoryManager* g_pMemoryManager = MemoryManager::GetInstancePtr();

void TestUsingPagedMemory(const size_t size[] , size_t num)
{
	for(size_t i = 0; i < num; ++i)
		g_pMemoryManager->Allocate(size[i])
}

void TestUsingNormal(const size_t size[] , size_t num)
{
	for(size_t i = 0; i < num; ++i)
		malloc(size[i]);
}

int main()
{
	srand(time(NULL));
	g_pMemoryManager->Initialize();
	size_t size[MAX_TEST_NUM];
	for(size_t i = 0; i < MAX_TEST_NUM; ++i)
		size[i] = 1 + rand() % 10239;

	TestUsingPagedMemory(size , MAX_TEST_NUM);
	TestUsingNormal(size , MAX_TEST_NUM);

	//g_pMemoryManager->DumpMemLeak();
	//g_pMemoryManager->Report();
	g_pMemoryManager->Release();
	return 0;
}

void leakfunc1()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(1024);
	sprintf_s(pBuff , 507 , "leakfunc1 - Memory Leak here!\0");
	leakfunc2();
}

void leakfunc2()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(3101);
	sprintf_s(pBuff , 3100 , "leakfunc2 - Memory Leak here!\0");
	leakfunc3();
	g_pMemoryManager->Deallocate(pBuff);
}

void leakfunc3()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(1921);
	sprintf_s(pBuff , 1920 , "leakfunc3 - Memory Leak here!\0");
}

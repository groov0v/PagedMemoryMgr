#include "MemoryManager.h"
#include <windows.h>
#include <time.h>

#define MAX_TEST_NUM 50000

MemoryManager* g_pMemoryManager = MemoryManager::GetInstancePtr();

void TestUsingPagedMemory(const size_t size[] , size_t num)
{
	DWORD time = timeGetTime();
	for(size_t i = 0; i < num; ++i)
		g_pMemoryManager->Allocate(size[i]);
	printf("TestUsingPagedMemory : %d\n" , timeGetTime() - time);
}

void TestUsingNormal(const size_t size[] , size_t num)
{
	DWORD time = timeGetTime();
	for(size_t i = 0; i < num; ++i)
		malloc(size[i]);
	printf("TestUsingNormal : %d\n" , timeGetTime() - time);
}

int main()
{
	srand(time(0));
	g_pMemoryManager->Initialize();
	size_t size[MAX_TEST_NUM];
	for(size_t i = 0; i < MAX_TEST_NUM; ++i)
		size[i] = 1 + rand() % 10239;

	TestUsingPagedMemory(size , MAX_TEST_NUM);
	TestUsingNormal(size , MAX_TEST_NUM);

	//printf("-------Test Data-------\n");
	//for(size_t i = 0; i < MAX_TEST_NUM; ++i)
	//	printf("%d " , size[i]);

	//g_pMemoryManager->DumpMemLeak();
	//g_pMemoryManager->Report();
	g_pMemoryManager->Release();
	return 0;
}
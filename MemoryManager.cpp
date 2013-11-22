#include "MemoryManager.h"
#include <windows.h>
#include <Dbghelp.h>
#include <math.h>

#define BACKTRACE_CALLSTACK(frame) \
	do{ \
	unsigned long _ebp , _esp; \
	size_t index = 0; \
	__asm \
		{ \
		__asm mov _ebp , ebp \
		__asm mov _esp , esp \
		}\
		for(; index < MAX_STACK_NUM; ++index) \
		{ \
		frame[index] = (void*)(*((unsigned long*)_ebp + 1)); \
		_ebp = *((unsigned long*)_ebp); \
		if(_ebp == 0 || _ebp < _esp) \
		break; \
		} \
		for(; index < MAX_STACK_NUM; ++index) \
		frame[index] = NULL; \
	}while(0);

MemoryManager MemoryManager::_sInstance;
const uint MemoryManager::BLOCK_INCRE_SPAN[] = {512 , 1024 , 5120 , 10240};
const uint MemoryManager::BLOCK_INCRE_STEP[] = {8 , 16 , 32 , 128};

void MemoryManager::Initialize()
{
	uint blockSize = BLOCK_INIT_SIZE;
	size_t spanIdx = 0;
	size_t stepIdx = 0;
	size_t spanNum = sizeof(BLOCK_INCRE_SPAN) / sizeof(uint);
	for(size_t i = 0; i < PAGE_POOL_NUM; ++i)
	{
		_pPageTable[i] = (BlockPage*)VirtualAlloc(NULL , sizeof(BlockPage) , MEM_COMMIT , PAGE_READWRITE);
		memset(&_pPageTable[i]->tail , 0 , sizeof(BlockTail));
		_pPageTable[i]->tail.freeBlockCount = blockSize;
		_pPageTable[i]->blockSize = blockSize;
		_pPageTable[i]->block = (uchar*)VirtualAlloc(NULL , blockSize * PAGE_BLOCK_NUM , MEM_COMMIT , PAGE_READWRITE);
		if(blockSize >= BLOCK_INCRE_SPAN[spanIdx]) {++spanIdx;++stepIdx;}
		if(spanIdx >= spanNum) break;
		blockSize += BLOCK_INCRE_STEP[stepIdx];
	}
}

void MemoryManager::Release()
{
	BlockPage* pNext;
	for(size_t i = 0; i < PAGE_POOL_NUM; ++i)
	{
		pNext = _pPageTable[i];
		while(pNext)
		{
			pNext = (BlockPage*)_pPageTable[i]->tail.pNextPage;
			VirtualFree((LPVOID)_pPageTable[i]->block , 0 , MEM_RELEASE);
			VirtualFree((LPVOID)_pPageTable[i] , 0 , MEM_RELEASE);
			_pPageTable[i] = pNext;
		}
	}
}

int MemoryManager::GetPageIndex(size_t size)
{
	int idx = 0;
	int span = 8;

	for(size_t i = 0; i < MAX_SPAN; ++i)
	{
		if(size <= BLOCK_INCRE_SPAN[i])
		{
			idx += (size - span) / BLOCK_INCRE_STEP[i] + ((size - span) % BLOCK_INCRE_STEP[i] == 0 ? 0 : 1);
			break;
		}
		else
		{
			idx += (BLOCK_INCRE_SPAN[i] - span) / BLOCK_INCRE_STEP[i];
			span = BLOCK_INCRE_SPAN[i];
		}
	}

	return idx;
}

MemoryManager::BlockPage* MemoryManager::AllocNewPage(int pageIdx)
{
	int blockSize = _pPageTable[pageIdx]->blockSize;
	BlockPage* pPage = (BlockPage*)VirtualAlloc(NULL , sizeof(BlockPage) , MEM_COMMIT , PAGE_READWRITE);
	memset(&pPage->tail , 0 , sizeof(BlockTail));
	pPage->tail.freeBlockCount = blockSize;
	pPage->blockSize = blockSize;
	pPage->block = (uchar*)VirtualAlloc(NULL , blockSize * PAGE_BLOCK_NUM , MEM_COMMIT , PAGE_READWRITE);
	pPage->tail.pNextPage = _pPageTable[pageIdx];
	_pPageTable[pageIdx] = pPage;

	return pPage;
}

MemoryManager* MemoryManager::GetInstancePtr()
{
	return &_sInstance;
}

void* MemoryManager::Allocate(size_t size)
{
	size_t allocSize = size + MEM_SIZE_BYTE;	/// head 4 byte for page address

	uchar* pBlock = NULL;
	if(allocSize <= BLOCK_INCRE_SPAN[MAX_SPAN - 1])
	{
		int pageIdx = GetPageIndex(allocSize);
		BlockPage* pPage = _pPageTable[pageIdx];
		if(pPage->tail.freeBlockCount == 0)
		{
			while(pPage)
			{
				if(pPage->tail.freeBlockCount > 0) break;
				pPage = (BlockPage*)pPage->tail.pNextPage;
			}
			if(!pPage) pPage = AllocNewPage(pageIdx);
		}
		pBlock = &pPage->block[pPage->tail.firstFreeBlockIndex * pPage->blockSize];
		*(BlockPage**)pBlock = pPage;
		--pPage->tail.freeBlockCount;
		pPage->tail.freeBlockBitMask[pPage->tail.firstFreeBlockIndex / 8] |= 0x1 << (pPage->tail.firstFreeBlockIndex % 8);
		if(pPage->tail.freeBlockCount > 0)
		{
			for(size_t i = pPage->tail.firstFreeBlockIndex; i < PAGE_BLOCK_NUM; ++i)
			{
				if(((pPage->tail.freeBlockBitMask[pPage->tail.firstFreeBlockIndex / 8] >> (pPage->tail.firstFreeBlockIndex % 8)) & 0x1) > 0)
				{
					pPage->tail.firstFreeBlockIndex = i;
					break;
				}
			}
		}
	}
	else	/// using OS memory allocator
	{
		pBlock = (uchar*)VirtualAlloc(NULL , allocSize , MEM_COMMIT , PAGE_READWRITE);
		*(BlockPage**)pBlock = (BlockPage*)INVALID_PAGE_ADDR;
	}

	pBlock += MEM_SIZE_BYTE;
	Chunk* pChunk = (Chunk*)VirtualAlloc(NULL , sizeof(Chunk) , MEM_COMMIT , PAGE_READWRITE);
	pChunk->pAddr = pBlock;
	pChunk->size = size;
	BACKTRACE_CALLSTACK(pChunk->pStackFrame);

	AddChunk(pChunk);

	return pBlock;
}

void MemoryManager::Deallocate(void* block)
{
	uchar* pAllocBlock = (uchar*)block - MEM_SIZE_BYTE;
	BlockPage* pPage = *(BlockPage**)pAllocBlock;
	if(pPage != (BlockPage*)INVALID_PAGE_ADDR)
	{
		size_t blockIdx = (pAllocBlock - (uchar*)pPage) / pPage->blockSize;
		pPage->tail.firstFreeBlockIndex = blockIdx;
		pPage->tail.freeBlockBitMask[blockIdx / 8] &= ~(0x1 << (blockIdx % 8));
		++pPage->tail.freeBlockCount;
	}
	else
	{
		VirtualFree(pAllocBlock , 0 , MEM_RELEASE);
	}

	RemoveChunk((unsigned long)block);	
}

void MemoryManager::AddChunk(Chunk* pChunk)
{
	m_hmChucks.insert(std::make_pair((unsigned long)pChunk->pAddr , pChunk));
}

void MemoryManager::RemoveChunk(unsigned long addr)
{
	CHUNCK_MAP_ITER iter = m_hmChucks.find(addr);
	if(iter != m_hmChucks.end())
	{
		VirtualFree(iter->second , sizeof(Chunk) , MEM_RELEASE);
		m_hmChucks.erase(iter);
	}
}

void MemoryManager::Report()
{
	//printf("----------------Memory Report----------------\n");
	//SmallBlockPage* pNext = _pSmallBlockPages;
	//int i = 1;
	//while(pNext)
	//{
	//	printf("Page %d\n" , i++);
	//	printf("\t Free Count:%d\n" , pNext->tail.freeBlockCount);
	//	pNext = (SmallBlockPage*)pNext->tail.pNextPage;
	//}
}

void MemoryManager::DumpMemLeak()
{
	printf("----------------Memory Leak Dump----------------\n");
	if(m_hmChucks.empty())
		printf("No leak is detected!\n");
	else
	{
		HANDLE hProcess;
		SymSetOptions(SYMOPT_DEBUG);
		hProcess = GetCurrentProcess();

		if(SymInitialize(hProcess , NULL , TRUE))
		{	
			DWORD64 dwDisplacement64 = 0;
			DWORD   dwDisplacement = 0;
			char buffer[sizeof(SYMBOL_INFO)+ MAX_SYM_NAME * sizeof(TCHAR)];
			memset(buffer , 0 , sizeof(buffer));
			PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;

			_IMAGEHLP_LINE line;
			line.SizeOfStruct = sizeof(_IMAGEHLP_LINE);

			CHUNCK_MAP_ITER iter = m_hmChucks.begin();
			for(; iter != m_hmChucks.end(); ++iter)
			{
				printf("Leadk at address : 0x%08x , size : %d\n" , iter->second->pAddr , iter->second->size);
				for(size_t i = 0; i < MAX_STACK_NUM && iter->second->pStackFrame[i]; ++i)
				{
					if(SymFromAddr(hProcess , (DWORD)iter->second->pStackFrame[i] , &dwDisplacement64 , pSymbol))
					{
						SymGetLineFromAddr(hProcess , (DWORD)iter->second->pStackFrame[i] , &dwDisplacement , &line);
						printf("\t[%u] %s(0x%08x) Line %u in %s\n" , i , pSymbol->Name , iter->second->pStackFrame[i] , line.LineNumber , line.FileName);
					}
					else
						printf("\tSymFromAddr() failed. Error code: %u \n", GetLastError());
				}
			}
		}
		else
		{
			DWORD dwError = GetLastError();
			printf("SymInitialize returned error : %u\n", dwError);
		}

		SymCleanup(hProcess);
	}
	printf("------------------------------------------------\n");
}
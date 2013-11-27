#ifndef __MEMORYMANAGER_H
#define __MEMORYMANAGER_H

#include <hash_map>

typedef unsigned int uint;
typedef unsigned char uchar;

#define MAX_STACK_NUM 32
#define INVALID_PAGE_ADDR 0xFFFFFFFF

class MemoryManager
{
private:
	static const uint MEM_SIZE_BYTE = 4;
	static const uint PAGE_BLOCK_NUM = 128;
	static const uint PAGE_POOL_NUM = 264;
	static const uint BLOCK_INIT_SIZE = 8;
	static const uint MAX_SPAN = 4;
	static const uint BLOCK_INCRE_SPAN[];
	static const uint BLOCK_INCRE_STEP[];

	struct __declspec(align(4)) BlockTail
	{
		uchar freeBlockBitMask[PAGE_BLOCK_NUM / 8];	///< each bit for one block
		uchar freeBlockCount;
		uchar firstFreeBlockIndex;
		void* pPrevPage;
		void* pNextPage;
	};

	struct __declspec(align(4)) BlockPage
	{
		uchar* block;
		uint blockSize;
		BlockTail tail;
	};

	BlockPage* _pPageTable[PAGE_POOL_NUM];

#ifdef MEM_TRACK
	struct __declspec(align(4)) Chunk
	{
		void* pAddr;
		size_t size;
		void* pStackFrame[MAX_STACK_NUM];
	};
#endif

	static MemoryManager _sInstance;
private:
	MemoryManager() {}
	~MemoryManager() {}
#ifdef MEM_TRACK
	void AddChunk(Chunk* pChunk);
	void RemoveChunk(unsigned long addr);
#endif
	int GetPageIndex(size_t size);
	BlockPage* AllocNewPage(int pageIdx);
public:
	void Initialize();
	void Release();
	static MemoryManager* GetInstancePtr();
	void* Allocate(size_t size);
	void Deallocate(void* block);
	void Report();
#ifdef MEM_TRACK
	void DumpMemLeak();
protected:
	stdext::hash_map<unsigned long , Chunk*> m_hmChucks;
	typedef stdext::hash_map<unsigned long , Chunk*>::iterator CHUNCK_MAP_ITER;
#endif
};

#endif
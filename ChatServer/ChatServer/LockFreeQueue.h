#ifndef _CHATSERVER_MEMORY_QUEUE_H_
#define _CHATSERVER_MEMORY_QUEUE_H_

#include "MemoryPool.h"

template<class Type>
class CLockFreeQueue
{
	typedef struct st_MemoryBlock
	{
		st_MemoryBlock()
		{
			pNextblock = nullptr;
		}
		st_MemoryBlock * pNextblock;
		Type data;
	}BLOCK;

	typedef struct st_FreeNode
	{
		st_MemoryBlock * pTopnode;
		LONG64 uniquenum;
	}TOP;
	
public:
	CLockFreeQueue()
	{
		_queueusecount = 0;
		_queuememorypool = new CMemoryPool<BLOCK>;

		_pHead = (TOP*)_aligned_malloc(sizeof(TOP), 16);
		_pHead->pTopnode = _queuememorypool->Alloc();
		_pHead->pTopnode->pNextblock = nullptr;
		_pHead->uniquenum = 0;

		_pTail = (TOP*)_aligned_malloc(sizeof(TOP), 16);
		_pTail->pTopnode = _pHead->pTopnode;
		_pTail->uniquenum = 0;
	}

	virtual ~CLockFreeQueue()
	{
		while (_pHead->pTopnode != _pTail->pTopnode)
		{
			BLOCK * pNode = _pHead->pTopnode;
			_pHead->pTopnode = pNode->pNextblock;
			delete pNode;
		}

		_queuememorypool->Free(_pHead->pTopnode);
		_aligned_free(_pHead);
		_aligned_free(_pTail);
		delete _queuememorypool;
	}

	void Enqueue(Type InData)
	{
		BLOCK * pNode = _queuememorypool->Alloc();
		pNode->pNextblock = nullptr;
		pNode->data = InData;

		TOP tail;
		tail.uniquenum = InterlockedIncrement64(&_pTail->uniquenum);
		while (1)
		{
			tail.pTopnode = _pTail->pTopnode;
			BLOCK * pNext = tail.pTopnode->pNextblock;

			if (NULL == pNext)
			{
				if (nullptr == InterlockedCompareExchangePointer((PVOID*)&tail.pTopnode->pNextblock,
					pNode, pNext))
				{
					InterlockedCompareExchange128((LONG64*)_pTail, tail.uniquenum,
						(LONG64)pNode, (LONG64*)&tail);
					break;
				}
			}
			else
			{
				InterlockedCompareExchange128((LONG64*)_pTail, tail.uniquenum,
					(LONG64)tail.pTopnode->pNextblock, (LONG64*)&tail);
			}
		}
		InterlockedIncrement(&_queueusecount);
		return;
	}

	bool Dequeue(Type &OutData)
	{
		if (InterlockedDecrement(&_queueusecount) < 0)
		{
			InterlockedIncrement(&_queueusecount);
			OutData = nullptr;
			return false;
		}

		TOP head;
		TOP tail;
		head.uniquenum = InterlockedIncrement64(&(_pHead->uniquenum));
		while (1)
		{
			head.pTopnode = _pHead->pTopnode;
			tail.pTopnode = _pTail->pTopnode;
			BLOCK * pNode = head.pTopnode->pNextblock;
			if (nullptr == pNode)
				continue;

			if (head.pTopnode == tail.pTopnode)
			{
				if (0 == InterlockedCompareExchange128((LONG64*)_pTail, tail.uniquenum,
					(LONG64)tail.pTopnode->pNextblock, (LONG64*)&tail))
					continue;
			}

			OutData = pNode->data;
			if (1 == InterlockedCompareExchange128((LONG64*)_pHead, head.uniquenum,
				(LONG64)head.pTopnode->pNextblock, (LONG64*)&head))
			{
				_queuememorypool->Free(head.pTopnode);
				break;
			}
		}
		return true;
	}

	long GetUseCount() { return _queueusecount; }
	long GetQueueMemoryPoolUseCount() { return _queuememorypool->GetUseCount(); }
	long GetQueueMemoryPoolAllocCount() { return _queuememorypool->GetAllocCount(); }
	
private:
	long				_queueusecount;
	CMemoryPool<BLOCK> * _queuememorypool;
	TOP * _pHead;
	TOP * _pTail;
};



#endif _CHATSERVER_MEMORY_QUEUE_H_
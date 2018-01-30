#ifndef _CHATSERVER_MEMORY_QUEUE_H_
#define _CHATSERVER_MEMORY_QUEUE_H_

#include "MemoryPool.h"

template<class Type>
class CLockFreeQueue
{
	struct st_NODE
	{
		st_NODE				*pNext;
		Type				Data;

		st_NODE() :
			pNext(nullptr),
			Data(NULL) {}
	};
	struct st_TOP
	{
		st_NODE				*pNode;
		unsigned __int64	iCount;

		st_TOP() :
			pNode(nullptr),
			iCount(NULL) {}
	};
	
public:
	CLockFreeQueue();
	~CLockFreeQueue();

	void Enqueue(Type Data);
	bool Dequeue(Type *pData);

	long GetUseCount();
	
private:
	long				m_lUseCount;
	st_TOP				*m_pFront;
	st_TOP				*m_pRear;
	CMemoryPool<st_NODE>	m_FreeList;
};

template<class Type>
inline CLockFreeQueue<Type>::CLockFreeQueue()
{
	m_lUseCount = 0;

	m_pFront = (st_TOP*)_aligned_malloc(sizeof(st_TOP), 16);
	m_pFront->pNode = m_FreeList.Alloc();
	m_pFront->pNode->pNext = nullptr;
	m_pFront->iCount = 0;

	m_pRear = (st_TOP*)_aligned_malloc(sizeof(st_TOP), 16);
	m_pRear->pNode = m_pFront->pNode;
	m_pRear->iCount = 0;
}

template<class Type>
inline CLockFreeQueue<Type>::~CLockFreeQueue()
{
	st_NODE* _pNode = m_pFront->pNode;
	while (_pNode)
	{
		st_NODE *_pNext = _pNode->pNext;
		m_FreeList.Free(_pNode);
		_pNode = _pNext;
		InterlockedDecrement(&m_lUseCount);
	}
	_aligned_free(m_pFront);
	_aligned_free(m_pRear);
}

template<class Type>
inline void CLockFreeQueue<Type>::Enqueue(Type Data)
{
	st_NODE *_pNewNode = m_FreeList.Alloc();
	_pNewNode->pNext = nullptr;
	_pNewNode->Data = Data;

	st_TOP _NewRear;
	_NewRear.pNode = m_pRear->pNode;
	_NewRear.iCount = m_pRear->iCount;
	while (1)
	{
		if (nullptr == _NewRear.pNode->pNext)
		{
			if (nullptr == InterlockedCompareExchangePointer((PVOID*)&_NewRear.pNode->pNext,
				_pNewNode, nullptr))
			{
				InterlockedCompareExchange128((LONG64*)m_pRear, _NewRear.iCount + 1, 
											(LONG64)_pNewNode, (LONG64*)&_NewRear);
				break;
			}
		}
		InterlockedCompareExchange128((LONG64*)m_pRear, _NewRear.iCount + 1, 
									(LONG64)_NewRear.pNode->pNext, (LONG64*)&_NewRear);
	}
	InterlockedIncrement(&m_lUseCount);
}

template<class Type>
inline bool CLockFreeQueue<Type>::Dequeue(Type *pData)
{
	if (InterlockedDecrement(&m_lUseCount) < 0)
	{
		InterlockedIncrement(&m_lUseCount);
		return false;
	}
	st_TOP _Front;
	_Front.pNode = m_pFront->pNode;
	_Front.iCount = m_pFront->iCount;
	while (1)
	{
		st_NODE *_pNext = _Front.pNode->pNext;
		if (nullptr == _pNext)
		{
			_Front.pNode = m_pFront->pNode;
			_Front.iCount = m_pFront->iCount;
			continue;
		}
		*pData = _pNext->Data;
		if (InterlockedCompareExchange128((LONG64*)m_pFront, _Front.iCount + 1, 
										(LONG64)_Front.pNode->pNext, (LONG64*)&_Front))
		{
			m_FreeList.Free(_Front.pNode);
			break;
		}
	}
	return true;
}

template<class Type>
inline LONG CLockFreeQueue<Type>::GetUseCount()
{
	return m_lUseCount;
}

#endif _CHATSERVER_MEMORY_QUEUE_H_
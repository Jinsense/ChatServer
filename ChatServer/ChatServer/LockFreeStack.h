#ifndef _CHATSERVER_MEMORY_STACK_H_
#define _CHATSERVER_MEMORY_STACK_H_

#include "MemoryPool.h"

template<class Type>
class CLockFreeStack
{
	struct st_NODE
	{
		st_NODE*			pNext;
		Type				Data;

		st_NODE() :
			pNext(nullptr),
			Data(NULL) {}
	};
	struct st_TOP
	{
		st_NODE*			pNode;
		unsigned __int64	iCount;

		st_TOP() :
			pNode(nullptr),
			iCount(NULL) {}
	};

public:
	CLockFreeStack();
	~CLockFreeStack();

	void Push(Type Data);
	void Pop(Type *pData);

	long GetUseCount();

private:
	long				m_lUseCount;
	st_TOP				*m_pTop;
	CMemoryPool<st_NODE>	m_FreeList;
};

template<class Type>
inline CLockFreeStack<Type>::CLockFreeStack()
{
	m_lUseCount = 0;

	m_pTop = (st_TOP*)_aligned_malloc(sizeof(st_TOP), 16);
	m_pTop->pNode = nullptr;
	m_pTop->iCount = 0;
}

template<class Type>
inline CLockFreeStack<Type>::~CLockFreeStack()
{
	st_NODE *_pNode = m_pTop->pNode;
	while (_pNode)
	{
		st_NODE *_pNext = _pNode->pNext;
		m_FreeList.Free(_pNode);
		_pNode = _pNext;
		InterlockedDecrement(&m_lUseCount);
	}
	_aligned_free(m_pTop);
}

template<class Type>
inline void CLockFreeStack<Type>::Push(Type Data)
{
	st_NODE *_pNode = m_FreeList.Alloc();
	_pNode->Data = Data;

	st_TOP _Top;
	_Top.pNode = m_pTop->pNode;
	_Top.iCount = m_pTop->iCount;
	for (;;)
	{
		_pNode->pNext = _Top.pNode;
		if (InterlockedCompareExchange128((LONG64*)m_pTop, _Top.iCount + 1, 
										(LONG64)_pNode, (LONG64*)&_Top))
		{
			InterlockedIncrement(&m_lUseCount);
			return;
		}
	}
}

template<class Type>
inline void CLockFreeStack<Type>::Pop(Type *pData)
{
	if (InterlockedDecrement(&m_lUseCount) < 0)
	{
		InterlockedIncrement(&m_lUseCount);
		*pData = nullptr;
		return;
	}
	st_TOP _Top;
	_Top.pNode = m_pTop->pNode;
	_Top.iCount = m_pTop->iCount;
	for (;;)
	{
		if (InterlockedCompareExchange128((LONG64*)m_pTop, _Top.iCount + 1, 
									(LONG64)_Top.pNode->pNext, (LONG64*)&_Top))
		{
			*pData = _Top.pNode->Data;
			m_FreeList.Free(_Top.pNode);
			return;
		}
	}
}

template<class Type>
inline long CLockFreeStack<Type>::GetUseCount()
{
	return m_lUseCount;
}

#endif _CHATSERVER_MEMORY_STACK_H_
#ifndef _CHATSERVER_MEMORY_POOL_H_
#define _CHATSERVER_MEMORY_POOL_H_

#include <Windows.h>

template<class Type>
class CMemoryPool
{
private:
	struct st_NODE
	{
		st_NODE				*pNext;
		Type				Data;

		st_NODE() :
			pNext(nullptr)
		{}
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
	CMemoryPool();
	~CMemoryPool();

	Type *Alloc();
	void Free(Type *pData);

	long GetUseCount() { return m_lUseCount; }
	long GetAllocCount() { return m_lAllocCount; }

private:
	long	m_lUseCount;
	long	m_lAllocCount;
	st_TOP	*m_pTop;
};

template<class Type>
inline CMemoryPool<Type>::CMemoryPool()
{
	m_lUseCount = 0;
	m_lAllocCount = 0;

	m_pTop = (st_TOP*)_aligned_malloc(sizeof(st_TOP), 16);
	m_pTop->pNode = nullptr;
	m_pTop->iCount = 0;
}

template<class Type>
inline CMemoryPool<Type>::~CMemoryPool()
{
	st_NODE *_pNode = m_pTop->pNode;
	while (_pNode)
	{
		st_NODE* _pNext = _pNode->pNext;
		delete _pNode;
		_pNode = _pNext;
		InterlockedDecrement(&m_lUseCount);
	}
	_aligned_free(m_pTop);
}

template<class Type>
inline Type * CMemoryPool<Type>::Alloc()
{
	long curalloc = m_lAllocCount;
	long curuse = InterlockedIncrement(&m_lUseCount);

	if (curuse > curalloc)
	{
		InterlockedIncrement(&m_lAllocCount);
		return &((new st_NODE)->Data);
	}

	st_TOP _Top;

	for (; ; )
	{
		_Top.pNode = m_pTop->pNode;
		_Top.iCount = m_pTop->iCount;

		if (InterlockedCompareExchange128((LONG64*)m_pTop, _Top.iCount + 1,
			(LONG64)_Top.pNode->pNext, (LONG64*)&_Top))
		{
			return &(_Top.pNode->Data);
		}
	}
}

template<class Type>
inline void CMemoryPool<Type>::Free(Type *pData)
{

	st_NODE *_pNode = (st_NODE*)((char*)pData - sizeof(st_NODE*));

	st_TOP _Top;
	for (; ; )
	{
		_Top.iCount = m_pTop->iCount;
		_Top.pNode = m_pTop->pNode;
		_pNode->pNext = _Top.pNode;
		if (InterlockedCompareExchange128((LONG64*)m_pTop, _Top.iCount + 1,
			(LONG64)_pNode, (LONG64*)&_Top))
		{
			InterlockedDecrement(&m_lUseCount);
			return;
		}
	}
}

template<class Type>
class CMemoryPoolTLS
{
private:
	enum en_BLOCK
	{
		eNUM_CHUNKBLOCK = 200,
		eNUM_DEFAULT = 10,
	};

	class CChunk
	{
	public:

		struct st_NODE
		{
			Type				Data;
			CChunk				*pChunkNode;
			st_NODE				*pNext;

			st_NODE() :
				pChunkNode(nullptr),
				pNext(nullptr)
			{}
		};

	public:
		CChunk()
		{
			_pNodeArr = new st_NODE[eNUM_CHUNKBLOCK];
			for (auto iCnt = 0; iCnt < eNUM_CHUNKBLOCK; iCnt++)
			{
				_pNodeArr[iCnt].pChunkNode = this;
				if (eNUM_CHUNKBLOCK == iCnt)
					_pNodeArr[iCnt].pNext = nullptr;
				else
					_pNodeArr[iCnt].pNext = &_pNodeArr[iCnt + 1];
			}
			_FreeCount = eNUM_CHUNKBLOCK;
			_AllocCount = eNUM_CHUNKBLOCK;
			_pTop = &_pNodeArr[0];
		}

		~CChunk()
		{
			delete[] _pNodeArr;
		}

		void	Initialize()
		{
			_pTop = &_pNodeArr[0];
			_AllocCount = eNUM_CHUNKBLOCK;
			_FreeCount = eNUM_CHUNKBLOCK;
		}

		Type *Alloc()
		{
			InterlockedDecrement(&_AllocCount);
			st_NODE *pTemp = _pTop;
			_pTop = _pTop->pNext;
			return &pTemp->Data;
		}

		long Free()
		{
			return InterlockedDecrement(&_FreeCount);
		}

	private:
		//		const st_NODE * const	_pEnd;
		st_NODE		*_pNodeArr;
		st_NODE		*_pTop;
		long		_AllocCount;
		long		_FreeCount;
		friend class CMemoryPoolTLS;
	};

public:
	CMemoryPoolTLS()
	{
		_pMemPool = new CMemoryPool<CChunk>;
		_TLSIndex = TlsAlloc();
//		_UseCount = 0;
		_AllocCount = 0;
	}
	~CMemoryPoolTLS()
	{
		TlsFree(_TLSIndex);
	}

	Type *Alloc()
	{
//		InterlockedIncrement(&_UseCount);

		CChunk *pChunk;

		if (NULL == TlsGetValue(_TLSIndex))
		{
			pChunk = _pMemPool->Alloc();
			pChunk->Initialize();
			TlsSetValue(_TLSIndex, pChunk);
		}
		else
			pChunk = (CChunk*)TlsGetValue(_TLSIndex);

		Type *pPacket = pChunk->Alloc();

		if (0 == pChunk->_AllocCount)
		{
			TlsSetValue(_TLSIndex, NULL);
		}

		return pPacket;
	}

	void Free(Type *pData)
	{
//		InterlockedDecrement(&_UseCount);

		CChunk* pChunk = ((CChunk::st_NODE*)pData)->pChunkNode;

		if (0 == pChunk->Free())
		{
			_pMemPool->Free(pChunk);
		}
		return;
	}

//	long	GetUseCount() { return _UseCount; }
	long	GetAllocCount() { return _pMemPool->GetAllocCount() * eNUM_CHUNKBLOCK; }
private:
	DWORD	_TLSIndex;
//	long	_UseCount;
	long	_AllocCount;

	CMemoryPool<CChunk>		*_pMemPool;
};

#endif _CHATSERVER_MEMORY_POOL_H_

//		CChunk::st_NODE *pNode = (CChunk::st_NODE*)((char*)pData - (sizeof(CChunk::st_NODE*) * 2));
//		CChunk *pChunk = pNode->pChunkNode;
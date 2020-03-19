/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _MX_ARRAYLIST_H
#define _MX_ARRAYLIST_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

template <typename TType, SIZE_T nGranularity = 32>
class TArrayList : public virtual CBaseMemObj
{
public:
  typedef TArrayList<TType, nGranularity> _list;

  TArrayList() : CBaseMemObj()
    {
    lpItems = NULL;
    nCount = nSize = 0;
    return;
    };

  virtual ~TArrayList()
    {
    RemoveAllElements();
    return;
    };

  virtual BOOL AddElement(_In_ TType elem)
    {
    return InsertElementAt(elem, (SIZE_T)-1);
    };

  virtual BOOL SetElementAt(_In_ TType elem, _In_ SIZE_T nIndex)
    {
    if (nIndex >= nCount)
      return FALSE;
    lpItems[nIndex] = elem;
    return TRUE;
    };

  virtual BOOL InsertElementAt(_In_ TType elem, _In_ SIZE_T nIndex = (SIZE_T)-1)
    {
    if (nIndex >= nCount)
      nIndex = nCount;
    if (SetSize(nCount + 1) == FALSE)
      return FALSE;
    ::MxMemMove(&lpItems[nIndex + 1], &lpItems[nIndex], (nCount - nIndex) * sizeof(TType));
    lpItems[nIndex] = elem;
    nCount++;
    return TRUE;
    };

  template<class _Comparator>
  BOOL SortedInsert(_In_ TType elem, _In_ _Comparator lpCompareFunc, _In_opt_ LPVOID lpContext = NULL,
                    _In_opt_ BOOL bDontInsertDuplicates = FALSE, _Out_opt_ LPBOOL lpbAlreadyOnList = NULL)
    {
    SIZE_T nIndex;

    if (lpbAlreadyOnList != NULL)
      *lpbAlreadyOnList = FALSE;
    if (lpCompareFunc == NULL)
      return FALSE;

    if (SetSize(nCount + 1) == FALSE)
      return FALSE;
    nIndex = nCount;
    while (nIndex > 0)
    {
      int comp = lpCompareFunc(lpContext, &elem, &lpItems[nIndex - 1]);
      if (comp == 0)
      {
        if (bDontInsertDuplicates != FALSE)
        {
          if (lpbAlreadyOnList != NULL)
            *lpbAlreadyOnList = TRUE;
          return TRUE;
        }
      }
      if (comp >= 0)
        break;
      lpItems[nIndex] = lpItems[nIndex - 1];
      nIndex--;
    }
    lpItems[nIndex] = elem;
    nCount++;
    return TRUE;
    };

  template<class _Comparator, class _KeyType>
  TType* BinarySearchPtr(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc, _In_opt_ LPVOID lpContext = NULL)
    {
    SIZE_T nIndex = BinarySearch(_key, lpSearchFunc, lpContext);
    return (nIndex != (SIZE_T)-1) ? lpItems + nIndex : NULL;
    };

  template<class _Comparator, class _KeyType>
  SIZE_T BinarySearch(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc, _In_opt_ LPVOID lpContext = NULL)
    {
    SIZE_T nBase, n;

    if (lpSearchFunc == NULL)
      return (SIZE_T)-1;
    nBase = 0;
    n = nCount;
    while (n > 0)
    {
      SIZE_T nMid = nBase + (n >> 1);

      int comp = lpSearchFunc(lpContext, _key, &lpItems[nMid]);
      if (comp == 0)
        return nMid;
      if (comp > 0)
      {
        nBase = nMid + 1;
        n--;
      }
      n >>= 1;
    }
    return (SIZE_T)-1;
    };

  virtual TType GetElementAt(_In_ SIZE_T nIndex) const
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };

  virtual TType& operator[](_In_ SIZE_T nIndex)
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };
  virtual const TType& operator[](_In_ SIZE_T nIndex) const
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };

  virtual SIZE_T GetCount() const
    {
    return nCount;
    };

  virtual BOOL SetCount(_In_ SIZE_T _nCount)
    {
    SIZE_T i;

    if (_nCount > nCount)
    {
      if (SetSize(_nCount) == FALSE)
        return FALSE;
    }
    if (_nCount < nCount)
    {
      for (i = _nCount; i < nCount; i++)
        OnDeleteItem(lpItems[i]);
    }
    nCount = _nCount;
    return TRUE;
    };

  virtual SIZE_T GetSize() const
    {
    return nSize;
    };

  virtual BOOL SetSize(_In_ SIZE_T _nSize, _In_ BOOL bForceRealloc = FALSE)
    {
    TType *lpNewItems;
    SIZE_T i;

    if (_nSize != nSize)
    {
      if (_nSize > nSize || bForceRealloc != FALSE)
      {
        //first try to allocate new block (or relocated one)
        if (_nSize > 0)
        {
          if (nSize > 0 && bForceRealloc == FALSE)
          {
            //don't granulate on first allocation
            _nSize = (_nSize + nGranularity - 1) / nGranularity;
            _nSize *= nGranularity;
          }
          lpNewItems = (TType*)MX_MALLOC(_nSize * sizeof(TType));
          if (lpNewItems == NULL)
            return FALSE;
        }
        else
          lpNewItems = NULL;
        //free old pointers if needed
        if (_nSize < nCount)
        {
          for (i = _nSize; i < nCount; i++)
            OnDeleteItem(lpItems[i]);
          MxMemSet(&lpItems[_nSize], 0, (nCount - _nSize) * sizeof(TType));
        }
        //move old data to new
        if (_nSize > 0)
        {
          //copy old
          i = (nCount <= _nSize) ? nCount : _nSize;
          MxMemCopy(lpNewItems, lpItems, i * sizeof(TType));
          //initialize the rest to null
          if (_nSize > nCount)
            MxMemSet(&lpNewItems[nCount], 0, (_nSize - nCount) * sizeof(TType));
        }
        //release old pointer
        MX_FREE(lpItems);
        lpItems = lpNewItems;
        nSize = _nSize;
        if (nCount > nSize)
          nCount = nSize;
      }
    }
    return TRUE;
    };

  virtual BOOL IsEmpty() const
    {
    return (nCount == 0) ? TRUE : FALSE;
    };

  virtual SIZE_T GetIndexOf(_In_ TType elem, _In_ SIZE_T nStartIndex = 0) const
    {
    SIZE_T i;

    for (i = nStartIndex; i < nCount; i++)
    {
      if (elem == lpItems[i])
        return i;
    }
    return (SIZE_T)-1;
    };

  virtual BOOL Contains(_In_ TType elem) const
    {
    return (GetIndexOf(elem) != (SIZE_T)-1) ? TRUE : FALSE;
    };

  virtual TType GetLastElement() const
    {
    return (nCount > 0) ? GetElementAt(nCount - 1) : TType{};
    };

  virtual TType* GetBuffer() const
    {
    return lpItems;
    };

  virtual VOID Attach(_In_ TType *lpNewList, _In_ SIZE_T _nCount)
    {
    RemoveAllElements();
    if ((lpItems = lpNewList) != NULL)
      nCount = nSize = _nCount;
    return;
    };

  virtual TType* Detach()
    {
    TType* _lpItems = lpItems;
    lpItems = NULL;
    nCount = nSize = 0;
    return _lpItems;
    };

  virtual VOID Transfer(_In_ _list &cSrcList)
    {
    SIZE_T nCount;

    nCount = cSrcList.GetCount();
    Attach(cSrcList.Detach(), nCount);
    return;
    };

  virtual BOOL RemoveElementAt(_In_ SIZE_T nIndex, _In_ SIZE_T nItemsCount = 1, _In_ BOOL bDoFinalize = TRUE)
    {
    SIZE_T i;

    if (nIndex >= nCount || nItemsCount == 0 || nIndex + nItemsCount < nIndex)
      return FALSE;
    if (nIndex + nItemsCount > nCount)
      nItemsCount = nCount - nIndex;
    nCount -= nItemsCount;
    if (bDoFinalize != FALSE)
    {
      for (i = 0; i < nItemsCount; i++)
        OnDeleteItem(lpItems[nIndex + i]);
    }
    MxMemMove(&lpItems[nIndex], &lpItems[nIndex + nItemsCount], (nCount - nIndex) * sizeof(TType));
    return TRUE;
    };

  virtual VOID RemoveAllElements()
    {
    SIZE_T i;

    for (i = 0; i < nCount; i++)
      OnDeleteItem(lpItems[i]);
    MX_FREE(lpItems);
    lpItems = NULL;
    nCount = nSize = 0;
    return;
    };

  virtual TType* ReserveBlock(_In_ SIZE_T nItemsCount, _In_ SIZE_T nIndex = (SIZE_T)-1)
    {
    if (SetSize(nCount + nItemsCount) == FALSE)
      return FALSE;
    if (nIndex > nCount)
      nIndex = nCount;
    MxMemMove(&lpItems[nIndex + nItemsCount], &lpItems[nIndex], (nCount - nIndex) * sizeof(TType));
    MxMemSet(&lpItems[nIndex], 0, nItemsCount * sizeof(TType));
    nCount += nItemsCount;
    return &lpItems[nIndex];
    };

  virtual TType* PopBlock(_In_ SIZE_T nItemsCount)
    {
    if (nItemsCount > nCount)
      return NULL;
    nCount -= nItemsCount;
    return &lpItems[nCount];
    };

protected:
  virtual VOID OnDeleteItem(_Inout_ TType& item)
    {
    return;
    };

private:
  TType *lpItems;
  SIZE_T nCount, nSize;
};

//-----------------------------------------------------------

template <typename TType, SIZE_T nGranularity = 32>
class TArrayListWithFree : public TArrayList<TType, nGranularity>
{
public:
  virtual ~TArrayListWithFree()
    {
    TArrayList<TType, nGranularity>::RemoveAllElements();
    return;
    };

protected:
  VOID OnDeleteItem(_Inout_ TType& item)
    {
    MX_FREE(item);
    return;
    };
};

//-----------------------------------------------------------

template <typename TType, SIZE_T nGranularity = 32>
class TArrayListWithRelease : public TArrayList<TType, nGranularity>
{
public:
  virtual ~TArrayListWithRelease()
    {
    TArrayList<TType, nGranularity>::RemoveAllElements();
    return;
    };

protected:
  VOID OnDeleteItem(_Inout_ TType& item)
    {
    if (item != NULL)
      item->Release();
    return;
    };
};

//-----------------------------------------------------------

template <typename TType, SIZE_T nGranularity = 32>
class TArrayListWithDelete : public TArrayList<TType, nGranularity>
{
public:
  virtual ~TArrayListWithDelete()
    {
    TArrayList<TType, nGranularity>::RemoveAllElements();
    return;
    };

protected:
  VOID OnDeleteItem(_Inout_ TType& item)
    {
    if (item != NULL)
      delete item;
    return;
    };
};

//-----------------------------------------------------------

template <typename TType, SIZE_T nGranularity = 32>
class TArrayList4Structs : public virtual CBaseMemObj
{
public:
  TArrayList4Structs() : CBaseMemObj()
    {
    lpItems = NULL;
    nCount = nSize = 0;
    return;
    };

  virtual ~TArrayList4Structs()
    {
    RemoveAllElements();
    return;
    };

  virtual BOOL AddElement(_In_ TType *lpElem)
    {
    return InsertElementAt(lpElem, (SIZE_T)-1);
    };

  virtual BOOL SetElementAt(_In_ TType *lpElem, _In_ SIZE_T nIndex)
    {
    if (nIndex >= nCount)
      return FALSE;
    MxMemCopy(&lpItems[nIndex], lpElem, sizeof(TType));
    return TRUE;
    };

  virtual BOOL InsertElementAt(_In_ TType *lpElem, _In_ SIZE_T nIndex = (SIZE_T)-1)
    {
    if (nIndex >= nCount)
      nIndex = nCount;
    if (SetSize(nCount + 1) == FALSE)
      return FALSE;
    MxMemMove(&lpItems[nIndex + 1], &lpItems[nIndex], (nCount - nIndex) * sizeof(TType));
    MxMemCopy(&lpItems[nIndex], lpElem, sizeof(TType));
    nCount++;
    return TRUE;
    };

  template<class _Comparator>
  BOOL SortedInsert(_In_ TType *lpElem, _In_ _Comparator lpCompareFunc, _In_opt_ LPVOID lpContext = NULL,
                    _In_opt_ BOOL bDontInsertDuplicates = FALSE, _Out_opt_ LPBOOL lpbAlreadyOnList = NULL)
    {
    SIZE_T nIndex;

    if (lpbAlreadyOnList != NULL)
      *lpbAlreadyOnList = FALSE;
    if (lpElem == NULL && lpCompareFunc == NULL)
      return FALSE;

    if (SetSize(nCount + 1) == FALSE)
      return FALSE;
    nIndex = nCount;
    while (nIndex > 0)
    {
      int comp = lpCompareFunc(lpContext, lpElem, &lpItems[nIndex - 1]);
      if (comp == 0)
      {
        if (bDontInsertDuplicates != FALSE)
        {
          if (lpbAlreadyOnList != NULL)
            *lpbAlreadyOnList = TRUE;
          return TRUE;
        }
      }
      if (comp >= 0)
        break;
      MxMemCopy(&lpItems[nIndex], &lpItems[nIndex - 1], sizeof(TType));
      nIndex--;
    }
    MxMemCopy(&lpItems[nIndex], lpElem, sizeof(TType));
    nCount++;
    return TRUE;
    };

  template<class _Comparator, class _KeyType>
  TType* BinarySearchPtr(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc, _In_opt_ LPVOID lpContext = NULL)
    {
    SIZE_T nIndex = BinarySearch(_key, lpSearchFunc, lpContext);
    return (nIndex != (SIZE_T)-1) ? lpItems + nIndex : NULL;
    };

  template<class _Comparator, class _KeyType>
  SIZE_T BinarySearch(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc, _In_opt_ LPVOID lpContext = NULL)
    {
    SIZE_T nBase, n;

    if (_key == NULL || lpSearchFunc == NULL)
      return (SIZE_T)-1;
    nBase = 0;
    n = nCount;
    while (n > 0)
    {
      SIZE_T nMid = nBase + (n >> 1);

      int comp = lpSearchFunc(lpContext, _key, &lpItems[nMid]);
      if (comp == 0)
        return nMid;
      if (comp > 0)
      {
        nBase = nMid + 1;
        n--;
      }
      n >>= 1;
    }
    return (SIZE_T)-1;
    };

  virtual TType& GetElementAt(_In_ SIZE_T nIndex) const
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };

  virtual TType& operator[](_In_ SIZE_T nIndex)
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };
  virtual const TType& operator[](_In_ SIZE_T nIndex) const
    {
    MX_ASSERT(nIndex < nCount);
    return lpItems[nIndex];
    };

  virtual TType* GetBuffer() const
    {
    return lpItems;
    };

  virtual SIZE_T GetCount() const
    {
    return nCount;
    };

  virtual BOOL SetCount(_In_ SIZE_T _nCount)
    {
    SIZE_T i;

    if (_nCount > nCount)
    {
      if (SetSize(_nCount) == FALSE)
        return FALSE;
    }
    if (_nCount < nCount)
    {
      for (i = _nCount; i < nCount; i++)
        OnDeleteItem(lpItems[i]);
    }
    nCount = _nCount;
    return TRUE;
    };

  virtual SIZE_T GetSize() const
    {
    return nSize;
    };

  virtual BOOL SetSize(_In_ SIZE_T _nSize, _In_ BOOL bForceRealloc = FALSE)
    {
    TType *lpNewItems;
    SIZE_T k;

    if (_nSize != nSize)
    {
      if (_nSize > nSize || bForceRealloc != FALSE)
      {
        if (_nSize > 0)
        {
          if (nSize > 0 && bForceRealloc == FALSE)
          {
            //don't granulate on first allocation
            _nSize = (_nSize + nGranularity - 1) / nGranularity;
            _nSize *= nGranularity;
          }
          lpNewItems = (TType*)MX_MALLOC(_nSize * sizeof(TType));
          if (lpNewItems == NULL)
            return FALSE;
        }
        else
          lpNewItems = NULL;
        //free old pointers if needed
        if (_nSize < nCount)
        {
          //WARNING: Check "OnItemDelete" method to avoid errors
          for (k = _nSize; k < nCount; k++)
            OnDeleteItem(lpItems[k]);
          MxMemSet(&lpItems[_nSize], 0, (nCount - _nSize) * sizeof(TType));
        }
        //move old data to new
        if (_nSize > 0)
        {
          //copy old
          k = (nCount <= _nSize) ? nCount : _nSize;
          MxMemCopy(lpNewItems, lpItems, k * sizeof(TType));
          //initialize the rest to null
          if (_nSize > nCount)
            MxMemSet(&lpNewItems[nCount], 0, (_nSize - nCount) * sizeof(TType));
        }
        MX_FREE(lpItems);
        lpItems = lpNewItems;
        nSize = _nSize;
        if (nCount > nSize)
          nCount = nSize;
      }
    }
    return TRUE;
    };

  virtual BOOL IsEmpty() const
    {
    return (nCount == 0) ? TRUE : FALSE;
    };

  virtual BOOL RemoveElementAt(_In_ SIZE_T nIndex, _In_ SIZE_T nItemsCount = 1, _In_ BOOL bDoFinalize = TRUE)
    {
    SIZE_T i;

    if (nIndex >= nCount || nItemsCount == 0 || nIndex + nItemsCount < nIndex)
      return FALSE;
    if (nIndex + nItemsCount > nCount)
      nItemsCount = nCount - nIndex;
    nCount -= nItemsCount;
    if (bDoFinalize != FALSE)
    {
      for (i = 0; i < nItemsCount; i++)
        OnDeleteItem(lpItems[nIndex + i]);
    }
    MxMemMove(&lpItems[nIndex], &lpItems[nIndex + nItemsCount], (nCount - nIndex) * sizeof(TType));
    return TRUE;
    };

  virtual VOID RemoveAllElements()
    {
    SIZE_T i;

    for (i = 0; i < nCount; i++)
      OnDeleteItem(lpItems[i]);
    MX_FREE(lpItems);
    lpItems = NULL;
    nCount = nSize = 0;
    return;
    };

  virtual TType* ReserveBlock(_In_ SIZE_T nItemsCount, _In_ SIZE_T nIndex = (SIZE_T)-1)
    {
    if (SetSize(nCount + nItemsCount) == FALSE)
      return FALSE;
    if (nIndex > nCount)
      nIndex = nCount;
    MxMemMove(&lpItems[nIndex + nItemsCount], &lpItems[nIndex], (nCount - nIndex) * sizeof(TType));
    MxMemSet(&lpItems[nIndex], 0, nItemsCount * sizeof(TType));
    nCount += nItemsCount;
    return &lpItems[nIndex];
    };

  virtual TType* PopBlock(_In_ SIZE_T nItemsCount)
    {
    if (nItemsCount > nCount)
      nItemsCount = nCount;
    nCount -= nItemsCount;
    return &lpItems[nCount];
    };

protected:
  virtual VOID OnDeleteItem(_Inout_ TType& item)
    {
    return;
    };

private:
  TType *lpItems;
  SIZE_T nCount, nSize;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ARRAYLIST_H

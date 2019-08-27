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
#ifndef _MX_LINKEDLIST_H
#define _MX_LINKEDLIST_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

template <typename classOrStruct>
class TLnkLst;

template <typename classOrStruct>
class TLnkLstNode
{
public:
  typedef TLnkLst<classOrStruct> _LnkLstList;
  typedef TLnkLstNode<classOrStruct> _LnkLstNode;

  TLnkLstNode()
    {
    lpNext = lpPrev = NULL;
    lpList = NULL;
    return;
    };

  _inline _LnkLstNode* GetNextNode()
    {
    return lpNext;
    };

  _inline _LnkLstNode* GetPrevNode()
    {
    return lpPrev;
    };

  _inline classOrStruct* GetNextEntry()
    {
    return (lpNext != NULL) ? (lpNext->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetPrevEntry()
    {
    return (lpPrev != NULL) ? (lpPrev->GetEntry()) : NULL;
    };

  _inline _LnkLstList* GetLinkedList()
    {
    return lpList;
    };

  _inline classOrStruct* GetEntry()
    {
    return static_cast<classOrStruct*>(this);
    };

  _inline VOID RemoveNode()
    {
    if (lpList != NULL)
      lpList->Remove(this);
    return;
    };

private:
  template <typename classOrStruct>
  friend class TLnkLst;

  _LnkLstList *lpList;
  _LnkLstNode *lpNext, *lpPrev;
};

//-----------------------------------------------------------

template <typename classOrStruct>
class TLnkLst
{
public:
  typedef TLnkLst<classOrStruct> _LnkLstList;
  typedef TLnkLstNode<classOrStruct> _LnkLstNode;

  TLnkLst()
    {
    lpHead = lpTail = NULL;
#ifdef _DEBUG
    nCount = 0;
#endif //_DEBUG
    return;
    };

  _inline BOOL IsEmpty()
    {
    return (lpHead == NULL) ? TRUE : FALSE;
    };

  _inline VOID Remove(_Inout_ _LnkLstNode *lpNode)
    {
    MX_ASSERT(lpNode->lpList == this);
    lpNode->lpList = NULL;
    if (lpHead == lpNode)
      lpHead = lpNode->lpNext;
    if (lpTail == lpNode)
      lpTail = lpNode->lpPrev;
    if (lpNode->lpNext != NULL)
      lpNode->lpNext->lpPrev = lpNode->lpPrev;
    if (lpNode->lpPrev != NULL)
      lpNode->lpPrev->lpNext = lpNode->lpNext;
    lpNode->lpNext = lpNode->lpPrev = NULL;
#ifdef _DEBUG
    nCount--;
#endif //_DEBUG
    return;
    };

  _inline VOID PushHead(_Inout_ _LnkLstNode *lpNode)
    {
    MX_ASSERT(lpNode != NULL);
    MX_ASSERT(lpNode->lpList == NULL);
    MX_ASSERT(lpNode->lpPrev == NULL && lpNode->lpNext == NULL);
    lpNode->lpList = this;
    if (lpHead != NULL)
      lpHead->lpPrev = lpNode;
    lpNode->lpNext = lpHead;
    lpHead = lpNode;
    if (lpTail == NULL)
      lpTail = lpNode;
#ifdef _DEBUG
    nCount++;
#endif //_DEBUG
    return;
    };

  _inline VOID PushTail(_Inout_ _LnkLstNode *lpNode)
    {
    MX_ASSERT(lpNode != NULL);
    MX_ASSERT(lpNode->lpList == NULL);
    MX_ASSERT(lpNode->lpPrev == NULL && lpNode->lpNext == NULL);
    lpNode->lpList = this;
    if (lpTail != NULL)
      lpTail->lpNext = lpNode;
    lpNode->lpPrev = lpTail;
    lpTail = lpNode;
    if (lpHead == NULL)
      lpHead = lpNode;
#ifdef _DEBUG
    nCount++;
#endif //_DEBUG
    return;
    };

  _inline VOID PushAfter(_Inout_ _LnkLstNode *lpNode, _Inout_ _LnkLstNode *lpAfterNode)
    {
    if (lpAfterNode == lpTail)
    {
      PushTail(lpNode);
    }
    else if (lpAfterNode == NULL)
    {
      PushHead(lpNode);
    }
    else
    {
      MX_ASSERT(lpNode->lpList == NULL);
      MX_ASSERT(lpNode->lpNext==NULL && lpNode->lpPrev==NULL);
      lpNode->lpList = this;
      lpNode->lpNext = lpAfterNode->lpNext;
      lpNode->lpPrev = lpAfterNode;
      if (lpAfterNode->lpNext != NULL)
        lpAfterNode->lpNext->lpPrev = lpNode;
      lpAfterNode->lpNext = lpNode;
#ifdef _DEBUG
      nCount++;
#endif //_DEBUG
    }
    return;
    };

  _inline VOID PushBefore(_Inout_ _LnkLstNode *lpNode, _Inout_ _LnkLstNode *lpBeforeNode)
    {
    if (lpBeforeNode == lpHead)
    {
      PushHead(lpNode);
    }
    else if (lpBeforeNode == NULL)
    {
      PushTail(lpNode);
    }
    else
    {
      MX_ASSERT(lpNode->lpList == NULL);
      MX_ASSERT(lpNode->lpNext==NULL && lpNode->lpPrev==NULL);
      lpNode->lpList = this;
      lpNode->lpNext = lpBeforeNode;
      lpNode->lpPrev = lpBeforeNode->lpPrev;
      if (lpBeforeNode->lpPrev != NULL)
        lpBeforeNode->lpPrev->lpNext = lpNode;
      lpBeforeNode->lpPrev = lpNode;
#ifdef _DEBUG
      nCount++;
#endif //_DEBUG
    }
    return;
    };

  _inline classOrStruct* GetHead()
    {
    return (lpHead != NULL) ? (lpHead->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetTail()
    {
    return (lpTail != NULL) ? (lpTail->GetEntry()) : NULL;
    };

  _inline classOrStruct* PopHead()
    {
    _LnkLstNode *lpNode;

    if ((lpNode = lpHead) == NULL)
      return NULL;
    Remove(lpHead);
    return lpNode->GetEntry();
    };

  _inline classOrStruct* PopTail()
    {
    _LnkLstNode *lpNode;

    if ((lpNode = lpTail) == NULL)
      return NULL;
    Remove(lpTail);
    return lpNode->GetEntry();
    };

  _inline VOID RemoveAll()
    {
    while (lpHead != NULL)
      Remove(lpHead);
    return;
    };

#ifdef _DEBUG
  _inline ULONG GetItemsCount() const
    {
    return nCount;
    };
#endif //_DEBUG

  //---------------------------------------------------------

  class Iterator
  {
  public:
    classOrStruct* Begin(_In_ _LnkLstList &_list)
      {
      lpNextCursor = _list.lpHead;
      return Next();
      };

    classOrStruct* Begin(_In_ const _LnkLstList &_list)
      {
      return Begin(const_cast<_LnkLstList&>(_list));
      };

    classOrStruct* Next()
      {
      lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->lpNext;
      return lpCursor->GetEntry();
      };

  private:
    _LnkLstNode *lpCursor, *lpNextCursor;
  };

  //---------------------------------------------------------

  class IteratorRev
  {
  public:
    classOrStruct* Begin(_In_ _LnkLstList &_list)
      {
      lpNextCursor = _list.lpTail;
      return Next();
      };

    classOrStruct* Begin(_In_ const _LnkLstList &_list)
      {
      return Begin(const_cast<_LnkLstList&>(_list));
      };

    classOrStruct* Next()
      {
      lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->lpPrev;
      return lpCursor->GetEntry();
      };

  private:
    _LnkLstNode *lpCursor, *lpNextCursor;
  };

private:
  _LnkLstNode *lpHead, *lpTail;
#ifdef _DEBUG
  ULONG nCount;
#endif //_DEBUG
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_LINKEDLIST_H

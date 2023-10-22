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

class CLnkLst;

class CLnkLstNode
{
public:
  CLnkLstNode()
    {
    lpNext = lpPrev = NULL;
    lpList = NULL;
    return;
    };

  _inline CLnkLstNode* GetNext()
    {
    return lpNext;
    };

  _inline CLnkLstNode* GetPrev()
    {
    return lpPrev;
    };

  _inline CLnkLst* GetList()
    {
    return lpList;
    };

  _inline VOID Remove();

private:
  friend class CLnkLst;

  CLnkLst *lpList;
  CLnkLstNode *lpNext, *lpPrev;
};

//-----------------------------------------------------------

class CLnkLst : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CLnkLst() : CBaseMemObj(), CNonCopyableObj()
    {
    lpHead = lpTail = NULL;
    nCount = 0;
    return;
    };

  _inline BOOL IsEmpty()
    {
    return (lpHead == NULL) ? TRUE : FALSE;
    };

  _inline VOID Remove(_In_ CLnkLstNode *lpNode)
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
    nCount--;
    return;
    };

  _inline VOID PushHead(_In_ CLnkLstNode *lpNode)
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
    nCount++;
    return;
    };

  _inline VOID PushTail(_In_ CLnkLstNode *lpNode)
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
    nCount++;
    return;
    };

  _inline VOID PushAfter(_In_ CLnkLstNode *lpNode, _In_ CLnkLstNode *lpAfterNode)
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
      nCount++;
    }
    return;
    };

  _inline VOID PushBefore(_In_ CLnkLstNode *lpNode, _In_ CLnkLstNode *lpBeforeNode)
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
      nCount++;
    }
    return;
    };

  _inline CLnkLstNode* GetHead()
    {
    return lpHead;
    };

  _inline CLnkLstNode* GetTail()
    {
    return lpTail;
    };

  _inline CLnkLstNode* PopHead()
    {
    CLnkLstNode *lpNode;

    if ((lpNode = lpHead) == NULL)
      return NULL;
    Remove(lpHead);
    return lpNode;
    };

  _inline CLnkLstNode* PopTail()
    {
    CLnkLstNode *lpNode;

    if ((lpNode = lpTail) == NULL)
      return NULL;
    Remove(lpTail);
    return lpNode;
    };

  _inline VOID RemoveAll()
    {
    while (lpHead != NULL)
      Remove(lpHead);
    return;
    };

  _inline SIZE_T GetCount() const
    {
    return nCount;
    };

  //---------------------------------------------------------

public:
  class Iterator
  {
  public:
    CLnkLstNode* Begin(_In_ CLnkLst &cList)
      {
      lpNextCursor = cList.lpHead;
      return Next();
      };

    CLnkLstNode* Begin(_In_ const CLnkLst &cList)
      {
      return Begin(const_cast<CLnkLst&>(cList));
      };

    CLnkLstNode* Next()
      {
      lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->lpNext;
      return lpCursor;
      };

  private:
    CLnkLstNode *lpCursor{ NULL }, *lpNextCursor{ NULL };
  };

  //---------------------------------------------------------

public:
  class IteratorRev
  {
  public:
    CLnkLstNode* Begin(_In_ CLnkLst &cList)
      {
      lpNextCursor = cList.lpTail;
      return Next();
      };

    CLnkLstNode* Begin(_In_ const CLnkLst &cList)
      {
      return Begin(const_cast<CLnkLst&>(cList));
      };

    CLnkLstNode* Next()
      {
      lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->lpPrev;
      return lpCursor;
      };

  private:
    CLnkLstNode *lpCursor{ NULL }, *lpNextCursor{ NULL };
  };

private:
  CLnkLstNode *lpHead{ NULL }, *lpTail{ NULL };
  SIZE_T nCount{ 0 };
};

_inline VOID CLnkLstNode::Remove()
{
  if (lpList != NULL)
    lpList->Remove(this);
  return;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_LINKEDLIST_H

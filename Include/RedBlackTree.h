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
#ifndef _MX_REDBLACKTREE_H
#define _MX_REDBLACKTREE_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

class CRedBlackTree;

class CRedBlackTreeNode
{
public:
  CRedBlackTreeNode()
    {
    bRed = FALSE;
    lpTree = NULL;
    lpLeft = lpRight = lpParent = NULL;
    return;
    };

  CRedBlackTreeNode* GetNext()
    {
    CRedBlackTreeNode *lpSucc, *lpNode2;

    if (lpRight != NULL)
    {
      lpSucc = lpRight;
      while (lpSucc->lpLeft != NULL)
        lpSucc = lpSucc->lpLeft;
    }
    else
    {
      lpSucc = lpParent;
      lpNode2 = this;
      while (lpSucc != NULL && lpNode2 == lpSucc->lpRight)
      {
        lpNode2 = lpSucc;
        lpSucc = lpSucc->lpParent;
      }
    }
    return lpSucc;
    };

  CRedBlackTreeNode* GetPrev()
    {
    CRedBlackTreeNode *lpPred, *lpNode2;

    if (lpLeft != NULL)
    {
      lpPred = lpLeft;
      while (lpPred->lpRight != NULL)
        lpPred = lpPred->lpRight;
    }
    else
    {
      lpPred = lpParent;
      lpNode2 = this;
      while (lpPred != NULL && lpNode2 == lpPred->lpLeft)
      {
        lpNode2 = lpPred;
        lpPred = lpPred->lpParent;
      }
    }
    return lpPred;
    };

  _inline CRedBlackTreeNode* GetLeft()
    {
    MX_ASSERT(this != NULL);
    return (this) ? lpLeft : NULL;
    };

  _inline CRedBlackTreeNode* GetRight()
    {
    MX_ASSERT(this != NULL);
    return (this) ? lpRight : NULL;
    };

  _inline CRedBlackTreeNode* GetParent()
    {
    MX_ASSERT(this != NULL);
    return (this) ? lpParent : NULL;
    };

  _inline CRedBlackTree* GetTree()
    {
    return lpTree;
    };

  _inline VOID Remove();

private:
  _inline BOOL IsRed()
    {
    MX_ASSERT(this != NULL);
    return (this) ? bRed : FALSE;
    };

  _inline VOID SetBlack()
    {
    MX_ASSERT(this != NULL);
    if (this)
      bRed = FALSE;
    return;
    };

  _inline VOID SetRed()
    {
    MX_ASSERT(this != NULL);
    if (this)
      bRed = TRUE;
    return;
    };

private:
  friend class CRedBlackTree;

  BOOL bRed;
  CRedBlackTree *lpTree;
  CRedBlackTreeNode *lpLeft, *lpRight, *lpParent;
};

//-----------------------------------------------------------

class CRedBlackTree : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CRedBlackTree() : CBaseMemObj(), CNonCopyableObj()
    {
    lpRoot = NULL;
    nCount = 0;
    return;
    };

  _inline BOOL IsEmpty() const
    {
    return (lpRoot == NULL) ? TRUE : FALSE;
    };

  template<class _Comparator, class _KeyType>
  _inline CRedBlackTreeNode* Find(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc, _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode = lpRoot;

    while (lpNode != NULL)
    {
      int comp = lpSearchFunc(lpContext, _key, lpNode);
      if (comp == 0)
        return lpNode;
      lpNode = (comp < 0) ? lpNode->lpLeft : lpNode->lpRight;
    }
    return NULL;
    };

  _inline CRedBlackTreeNode* GetFirst()
    {
    CRedBlackTreeNode *lpNode;

    if (lpRoot == NULL)
      return NULL;
    lpNode = lpRoot;
    while (lpNode->lpLeft != NULL)
      lpNode = lpNode->lpLeft;
    return lpNode;
    };

  _inline CRedBlackTreeNode* GetLast()
    {
    CRedBlackTreeNode *lpNode;

    if (lpRoot == NULL)
      return NULL;
    lpNode = lpRoot;
    while (lpNode->lpRight != NULL)
      lpNode = lpNode->lpRight;
    return lpNode;
    };

  //Try to get entry with key greater or equal to the specified one. Else get nearest less.
  template<class _Comparator, class _KeyType>
  _inline CRedBlackTreeNode* GetCeiling(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc,
                                        _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode = lpRoot, *lpParent;
    int comp;

    while (lpNode != NULL)
    {
      int comp = lpSearchFunc(lpContext, _key, lpNode);
      if (comp == 0)
        return lpNode;
      if (comp < 0)
      {
        if (lpNode->lpLeft == NULL)
          return lpNode;
        lpNode = lpNode->lpLeft;
      }
      else
      {
        if (lpNode->lpRight != NULL)
        {
          lpNode = lpNode->lpRight;
        }
        else
        {
          lpParent = lpNode->lpParent;
          while (lpParent != NULL && lpNode == lpParent->lpRight)
          {
            lpNode = lpParent;
            lpParent = lpParent->lpParent;
          }
          return lpParent;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key less or equal to the specified one. Else get nearest greater.
  template<class _Comparator, class _KeyType>
  _inline CRedBlackTreeNode* GetFloor(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc,
                                      _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode = lpRoot, *lpParent;

    while (lpNode != NULL)
    {
      int comp = lpSearchFunc(lpContext, _key, lpNode);
      if (comp == 0)
        return lpNode;
      if (comp > 0)
      {
        if (lpNode->lpRight == NULL)
          return lpNode;
        lpNode = lpNode->lpRight;
      }
      else
      {
        if (lpNode->lpLeft != NULL)
        {
          lpNode = lpNode->lpLeft;
        }
        else
        {
          lpParent = lpNode->lpParent;
          while (lpParent != NULL && lpNode == lpParent->lpLeft) {
            lpNode = lpParent;
            lpParent = lpParent->lpParent;
          }
          return lpParent;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key greater or equal to the specified one. Else null
  template<class _Comparator, class _KeyType>
  _inline CRedBlackTreeNode* GetHigher(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc,
                                       _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode = lpRoot, *lpParent;

    while (lpNode != NULL)
    {
      int comp = lpSearchFunc(lpContext, _key, lpNode);
      if (comp < 0)
      {
        if (lpNode->lpLeft == NULL)
          return lpNode;
        lpNode = lpNode->lpLeft;
      }
      else
      {
        if (lpNode->lpRight != NULL)
        {
          lpNode = lpNode->lpRight;
        }
        else
        {
          lpParent = lpNode->lpParent;
          while (lpParent != NULL && lpNode == lpParent->lpRight)
          {
            lpNode = lpParent;
            lpParent = lpParent->lpParent;
          }
          return lpParent;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key less or equal to the specified one. Else null
  template<class _Comparator, class _KeyType>
  _inline CRedBlackTreeNode* GetLower(_In_ _KeyType _key, _In_ _Comparator lpSearchFunc,
                                      _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode = lpRoot, *lpParent;

    while (lpNode != NULL)
    {
      int comp = lpSearchFunc(lpContext, _key, lpNode);
      if (comp > 0)
      {
        if (lpNode->lpRight == NULL)
          return lpNode;
        lpNode = lpNode->lpRight;
      }
      else
      {
        if (lpNode->lpLeft != NULL)
        {
          lpNode = lpNode->lpLeft;
        }
        else
        {
          lpParent = lpNode->lpParent;
          while (lpParent != NULL && lpNode == lpParent->lpLeft)
          {
            lpNode = lpParent;
            lpParent = lpParent->lpParent;
          }
          return lpParent;
        }
      }
    }
    return NULL;
    };

  template<class _Comparator>
  BOOL Insert(_In_ CRedBlackTreeNode *lpNewNode, _In_ _Comparator lpCompareFunc, _In_opt_ BOOL bAllowDuplicates = FALSE,
              _Out_opt_ CRedBlackTreeNode **lplpMatchingNode = NULL, _In_opt_ LPVOID lpContext = NULL)
    {
    CRedBlackTreeNode *lpNode, *lpParent;
    int comp;

    MX_ASSERT(lpNewNode != NULL);
    MX_ASSERT(lpNewNode->lpParent == NULL);
    MX_ASSERT(lpNewNode->lpLeft == NULL && lpNewNode->lpRight == NULL);

    if (lplpMatchingNode != NULL)
      *lplpMatchingNode = NULL;
    if (lpRoot == NULL)
    {
      lpNewNode->lpParent = NULL;
      lpNewNode->lpLeft = lpNewNode->lpRight = NULL;
      lpNewNode->lpTree = this;
      lpNewNode->bRed = FALSE;
      lpRoot = lpNewNode;
      nCount++;
      return TRUE;
    }

#ifdef _DEBUG
    CheckTree();
#endif //_DEBUG

    lpNode = lpRoot;
    do
    {
      lpParent = lpNode;
      comp = lpCompareFunc(lpContext, lpNewNode, lpNode);
      if (comp == 0 && bAllowDuplicates == FALSE)
      {
        if (lplpMatchingNode != NULL)
          *lplpMatchingNode = lpNode;
        return FALSE;
      }
      lpNode = (comp < 0) ? lpNode->lpLeft : lpNode->lpRight;
    }
    while (lpNode != NULL);
    lpNewNode->lpLeft = lpNewNode->lpRight = NULL;
    lpNewNode->lpParent = lpParent;
    if (comp < 0)
      lpParent->lpLeft = lpNewNode;
    else
      lpParent->lpRight = lpNewNode;
    lpNewNode->lpTree = this;
    lpNewNode->bRed = TRUE;
    //If the parent node is black we are all set, if it is red we have
    //the following possible cases to deal with.  We iterate through
    // the rest of the tree to make sure none of the required properties
    //is violated.
    //
    //1) The uncle is red.  We repaint both the parent and uncle black
    //    and repaint the grandparent node red.
    //
    // 2) The uncle is black and the new node is the right child of its
    //    parent, and the parent in turn is the left child of its parent.
    //    We do a left rotation to switch the roles of the parent and
    //    child, relying on further iterations to fixup the old parent.
    //
    // 3) The uncle is black and the new node is the left child of its
    //    parent, and the parent in turn is the left child of its parent.
    //    We switch the colors of the parent and grandparent and perform
    //    a right rotation around the grandparent.  This makes the former
    //    parent the parent of the new node and the former grandparent.
    //
    //Note that because we use a sentinel for the root node we never
    //need to worry about replacing the root.
    lpNode = lpNewNode;
    while (lpNode->lpParent != NULL && lpNode->lpParent->bRed != FALSE)
    {
      CRedBlackTreeNode *lpUncle;

      if (lpNode->lpParent == lpNode->lpParent->lpParent->lpLeft)
      {
        lpUncle = lpNode->lpParent->lpParent->lpRight;
        if (lpUncle != NULL && lpUncle->bRed != FALSE)
        {
          lpNode->lpParent->bRed = FALSE;
          lpUncle->bRed = FALSE;
          lpNode->lpParent->lpParent->bRed = TRUE;
          lpNode = lpNode->lpParent->lpParent;
        }
        else
        {
          if (lpNode == lpNode->lpParent->lpRight)
          {
            lpNode = lpNode->lpParent;
            LeftRotate(lpNode);
          }
          lpNode->lpParent->bRed = FALSE;
          lpNode->lpParent->lpParent->bRed = TRUE;
          RightRotate(lpNode->lpParent->lpParent);
        }
      }
      else
      {
        lpUncle = lpNode->lpParent->lpParent->lpLeft;
        if (lpUncle != NULL && lpUncle->bRed != FALSE)
        {
          lpNode->lpParent->bRed = FALSE;
          lpUncle->bRed = FALSE;
          lpNode->lpParent->lpParent->bRed = TRUE;
          lpNode = lpNode->lpParent->lpParent;
        }
        else
        {
          if (lpNode == lpNode->lpParent->lpLeft)
          {
            lpNode = lpNode->lpParent;
            RightRotate(lpNode);
          }
          lpNode->lpParent->bRed = FALSE;
          lpNode->lpParent->lpParent->bRed = TRUE;
          LeftRotate(lpNode->lpParent->lpParent);
        }
      }
    }
    lpRoot->bRed = FALSE; //first node is always black
    nCount++;
#ifdef _DEBUG
    CheckTree();
#endif //_DEBUG
    return TRUE;
    };

  VOID Remove(_In_ CRedBlackTreeNode *lpDelNode)
    {
    CRedBlackTreeNode *lpNode, *lpParentNode;

    MX_ASSERT(lpDelNode != NULL);
    MX_ASSERT(lpDelNode->lpTree == this);
    MX_ASSERT(lpDelNode == lpRoot || lpDelNode->lpLeft != NULL || lpDelNode->lpRight != NULL ||
              lpDelNode->lpParent != NULL);

    lpNode = lpDelNode;
    if (lpDelNode->lpLeft != NULL && lpDelNode->lpRight != NULL)
    {
      //swap node to delete with successor
      CRedBlackTreeNode *lpSucc, *lpSuccRight, *lpSuccParent;
      BOOL bSuccIsRed;

#ifdef _DEBUG
      CheckTree();
#endif //_DEBUG

      lpSucc = lpDelNode->lpRight;
      if (lpSucc->lpLeft != NULL)
      {
        while (lpSucc->lpLeft != NULL)
          lpSucc = lpSucc->lpLeft;

        //swap "lpSucc" and "lpDelNode"
        lpSuccRight = lpSucc->lpRight;
        lpSuccParent = lpSucc->lpParent;
        bSuccIsRed = lpSucc->bRed;

        lpSucc->lpLeft = lpDelNode->lpLeft;
        lpSucc->lpRight = lpDelNode->lpRight;
        lpSucc->lpParent = lpDelNode->lpParent;
        lpSucc->bRed = lpDelNode->bRed;

        //put "lpSucc" in the place of "lpDelNode" and update neighbors
        if (lpSucc->lpLeft != NULL)
          lpSucc->lpLeft->lpParent = lpSucc;
        if (lpSucc->lpRight != NULL)
          lpSucc->lpRight->lpParent = lpSucc;
        if (lpSucc->lpParent == NULL)
          lpRoot = lpSucc;
        else if (lpSucc->lpParent->lpLeft == lpDelNode)
          lpSucc->lpParent->lpLeft = lpSucc;
        else
          lpSucc->lpParent->lpRight = lpSucc;

        //now put "lpDelNode" in the place of "lpSucc"
        lpDelNode->lpParent = lpSuccParent;
        lpDelNode->lpLeft = NULL;
        lpDelNode->lpRight = lpSuccRight;
        lpDelNode->bRed = bSuccIsRed;

        //and update neighbors
        if (lpDelNode->lpParent->lpLeft == lpSucc)
          lpDelNode->lpParent->lpLeft = lpDelNode;
        else
          lpDelNode->lpParent->lpRight = lpDelNode;
        if (lpDelNode->lpRight != NULL)
          lpDelNode->lpRight->lpParent = lpDelNode;

#ifdef _DEBUG
        CheckTree();
#endif //_DEBUG
      }
      else
      {
        //swap adyacent nodes
        lpSucc->lpParent = lpDelNode->lpParent;
        lpDelNode->lpParent = lpSucc;
        if (lpSucc->lpParent == NULL)
          lpRoot = lpSucc;
        else if (lpSucc->lpParent->lpLeft == lpDelNode)
          lpSucc->lpParent->lpLeft = lpSucc;
        else
          lpSucc->lpParent->lpRight = lpSucc;

        lpDelNode->lpRight = lpSucc->lpRight;
        if (lpSucc->lpRight != NULL)
          lpSucc->lpRight->lpParent = lpDelNode;
        lpSucc->lpRight = lpDelNode;

        lpSucc->lpLeft = lpDelNode->lpLeft;
        if (lpSucc->lpLeft != NULL)
          lpSucc->lpLeft->lpParent = lpSucc;
        lpDelNode->lpLeft = NULL;

        bSuccIsRed = lpSucc->bRed;
        lpSucc->bRed = lpDelNode->bRed;
        lpDelNode->bRed = bSuccIsRed;

#ifdef _DEBUG
        CheckTree();
#endif //_DEBUG
      }
    }

    lpNode = (lpDelNode->lpLeft != NULL) ? lpDelNode->lpLeft : lpDelNode->lpRight;
    lpParentNode = lpDelNode->lpParent;

    if (lpNode != NULL)
    {
      lpNode->lpParent = lpParentNode;
    }
    if (lpParentNode == NULL)
    {
      lpRoot = lpNode;
    }
    else if (lpDelNode == lpDelNode->lpParent->lpLeft)
    {
      lpDelNode->lpParent->lpLeft = lpNode;
    }
    else
    {
      lpDelNode->lpParent->lpRight = lpNode;
    }

    if (lpDelNode->bRed == FALSE)
    {
      CRedBlackTreeNode *w;

      while (lpNode != lpRoot && IsRed(lpNode) == FALSE)
      {
        if (lpNode != NULL)
        {
          lpParentNode = lpNode->lpParent;
        }
        if (lpNode == lpParentNode->lpLeft)
        {
          w = lpParentNode->lpRight;
          if (w->bRed != FALSE)
          {
            w->bRed = FALSE;
            lpParentNode->bRed = TRUE;
            LeftRotate(lpParentNode);
            w = lpParentNode->lpRight;
          }
          if (IsRed(w->lpLeft) == FALSE && IsRed(w->lpRight) == FALSE)
          {
            w->bRed = TRUE;
            lpNode = lpParentNode;
          }
          else
          {
            if (IsRed(w->lpRight) == FALSE)
            {
              if (w->lpLeft != NULL)
              {
                w->lpLeft->bRed = FALSE;
              }
              w->bRed = TRUE;
              RightRotate(w);
              w = lpParentNode->lpRight;
            }
            w->bRed = lpParentNode->bRed;
              lpParentNode->bRed = FALSE;
            if (w->lpRight != NULL)
            {
              w->lpRight->bRed = FALSE;
            }
            LeftRotate(lpParentNode);
            lpNode = lpRoot;
          }
        }
        else
        {
          w = lpParentNode->lpLeft;
          if (w->bRed != FALSE)
          {
            w->bRed = FALSE;
            lpParentNode->bRed = TRUE;
            RightRotate(lpParentNode);
            w = lpParentNode->lpLeft;
          }
          if (IsRed(w->lpLeft) == FALSE && IsRed(w->lpRight) == FALSE)
          {
            w->bRed = TRUE;
            lpNode = lpParentNode;
          }
          else
          {
            if (IsRed(w->lpLeft) == FALSE)
            {
              if (w->lpRight != NULL)
              {
                w->lpRight->bRed = FALSE;
              }
              w->bRed = TRUE;
              LeftRotate(w);
              w = lpParentNode->lpLeft;
            }
            w->bRed = lpParentNode->bRed;
            lpParentNode->bRed = FALSE;
            if (w->lpLeft != NULL)
            {
              w->lpLeft->bRed = FALSE;
            }
            RightRotate(lpParentNode);
            lpNode = lpRoot;
          }
        }
      }
      if (lpNode != NULL)
      {
        lpNode->bRed = FALSE;
      }
    }

#ifdef _DEBUG
    CheckTree();
#endif //_DEBUG

    lpDelNode->lpTree = NULL;
    lpDelNode->lpLeft = lpDelNode->lpRight = NULL;
    lpDelNode->lpParent = NULL;
    nCount--;
    return;
    };

  _inline VOID RemoveAll()
    {
    while (lpRoot != NULL)
      Remove(lpRoot);
    return;
    };

  _inline SIZE_T GetCount() const
    {
    return nCount;
    };

#ifdef _DEBUG
  VOID CheckTree()
  {
    if (lpRoot != NULL)
    {
      int nBlackPathCounter = -1;

      MX_ASSERT(lpRoot->bRed == FALSE);
      MX_ASSERT(lpRoot->lpParent == FALSE);
      CheckColors(lpRoot);
      CheckBlackCount(lpRoot, 0, &nBlackPathCounter);
    }
    return;
    };

  VOID CheckColors(_In_opt_ CRedBlackTreeNode *lpNode)
  {
    if (lpNode != NULL)
    {
      if (lpNode->bRed != FALSE)
      {
        MX_ASSERT(lpNode->lpLeft == NULL || lpNode->lpLeft->bRed == FALSE);
        MX_ASSERT(lpNode->lpRight == NULL || lpNode->lpRight->bRed == FALSE);
        MX_ASSERT(lpNode->lpParent == NULL || lpNode->lpParent->bRed == FALSE);
      }
      CheckColors(lpNode->lpLeft);
      CheckColors(lpNode->lpRight);
    }
    return;
    };

  VOID CheckBlackCount(_In_opt_ CRedBlackTreeNode *lpNode, _In_ int nBlackCount, _In_ int *lpnBlackPathCounter)
    {
    if (lpNode == NULL || lpNode->bRed == FALSE)
    {
      nBlackCount++;
    }
    if (lpNode == NULL)
    {
      if (*lpnBlackPathCounter == -1)
      {
        *lpnBlackPathCounter = nBlackCount;
      }
      else
      {
        MX_ASSERT(nBlackCount == *lpnBlackPathCounter);
      }
    }
    else
    {
      CheckBlackCount(lpNode->lpLeft, nBlackCount, lpnBlackPathCounter);
      CheckBlackCount(lpNode->lpRight, nBlackCount, lpnBlackPathCounter);
    }
    return;
    };
#endif //_DEBUG

private:
  friend class Iterator;
  friend class IteratorRev;

  VOID LeftRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    CRedBlackTreeNode *lpRight;

    lpRight = lpNode->lpRight;
    lpNode->lpRight = lpRight->lpLeft;
    if (lpRight->lpLeft != NULL)
      lpRight->lpLeft->lpParent = lpNode;

    lpRight->lpParent = lpNode->lpParent;

    if (lpNode->lpParent == NULL)
      lpRoot = lpRight;
    else if (lpNode == lpNode->lpParent->lpLeft)
      lpNode->lpParent->lpLeft = lpRight;
    else
      lpNode->lpParent->lpRight = lpRight;

    lpRight->lpLeft = lpNode;
    lpNode->lpParent = lpRight;
    return;
    };

  VOID RightRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    CRedBlackTreeNode *lpLeft;

    lpLeft = lpNode->lpLeft;
    lpNode->lpLeft = lpLeft->lpRight;
    if (lpLeft->lpRight != NULL)
      lpLeft->lpRight->lpParent = lpNode;

    lpLeft->lpParent = lpNode->lpParent;

    if (lpNode->lpParent == NULL)
      lpRoot = lpLeft;
    else if (lpNode == lpNode->lpParent->lpRight)
      lpNode->lpParent->lpRight = lpLeft;
    else
      lpNode->lpParent->lpLeft = lpLeft;

    lpLeft->lpRight = lpNode;
    lpNode->lpParent = lpLeft;
    return;
    };

  BOOL IsRed(_In_ CRedBlackTreeNode *lpNode)
    {
    return (lpNode != NULL) ? lpNode->bRed : FALSE;
    };

  //---------------------------------------------------------

public:
  class Iterator
  {
  public:
    CRedBlackTreeNode* Begin(_In_ CRedBlackTree &cTree)
      {
      lpNextCursor = cTree.GetFirst();
      return Next();
      };

    CRedBlackTreeNode* Begin(_In_ const CRedBlackTree &cTree)
      {
      return Begin(const_cast<CRedBlackTree&>(cTree));
      };

    CRedBlackTreeNode* Next()
      {
      CRedBlackTreeNode *lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->GetNext();
      return lpCursor;
      };

  private:
    CRedBlackTreeNode *lpNextCursor;
  };

  //---------------------------------------------------------

public:
  class IteratorRev
  {
  public:
    CRedBlackTreeNode* Begin(_In_ CRedBlackTree &cTree)
      {
      lpNextCursor = cTree.GetLast();
      return Next();
      };

    CRedBlackTreeNode* Begin(_In_ const CRedBlackTree &cTree)
      {
      return Begin(const_cast<CRedBlackTree&>(cTree));
      };

    CRedBlackTreeNode* Next()
      {
      CRedBlackTreeNode *lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->GetPrev();
      return lpCursor;
      };

  private:
    CRedBlackTreeNode *lpNextCursor;
  };

private:
  CRedBlackTreeNode *lpRoot;
  SIZE_T nCount;
};

_inline VOID CRedBlackTreeNode::Remove()
{
  if (lpTree != NULL)
    lpTree->Remove(this);
  return;
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_REDBLACKTREE_H

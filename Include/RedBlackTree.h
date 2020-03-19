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
    return (this) ? lpLeft : NULL;
    };

  _inline CRedBlackTreeNode* GetRight()
    {
    return (this) ? lpRight : NULL;
    };

  _inline CRedBlackTreeNode* GetParent()
    {
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
    return (this) ? bRed : FALSE;
    };

  _inline VOID SetBlack()
    {
    if (this)
      bRed = FALSE;
    return;
    };

  _inline VOID SetRed()
    {
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
    CRedBlackTreeNode *lpNode, *lpParent, *lpUncle;
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
    Check(lpRoot);
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
    while (lpNode != NULL && lpNode != lpRoot && lpNode->lpParent->bRed != FALSE)
    {
      if (lpNode->GetParent() == lpNode->GetParent()->GetParent()->GetLeft())
      {
        lpUncle = lpNode->GetParent()->GetParent()->GetRight();
        if (lpUncle->IsRed() != FALSE)
        {
          lpNode->GetParent()->SetBlack();
          lpUncle->SetBlack();
          lpNode->GetParent()->GetParent()->SetRed();
          lpNode = lpNode->GetParent()->GetParent();
        }
        else
        {
          if (lpNode == lpNode->GetParent()->GetRight())
          {
            lpNode = lpNode->GetParent();
            LeftRotate(lpNode);
          }
          lpNode->GetParent()->SetBlack();
          lpNode->GetParent()->GetParent()->SetRed();
          RightRotate(lpNode->GetParent()->GetParent());
        }
      }
      else
      {
        lpUncle = lpNode->GetParent()->GetParent()->GetLeft();
        if (lpUncle->IsRed() != FALSE)
        {
          lpNode->GetParent()->SetBlack();
          lpUncle->SetBlack();
          lpNode->GetParent()->GetParent()->SetRed();
          lpNode = lpNode->GetParent()->GetParent();
        }
        else
        {
          if (lpNode == lpNode->GetParent()->GetLeft())
          {
            lpNode = lpNode->GetParent();
            RightRotate(lpNode);
          }
          lpNode->GetParent()->SetBlack();
          lpNode->GetParent()->GetParent()->SetRed();
          LeftRotate(lpNode->GetParent()->GetParent());
        }
      }
    }
    lpRoot->bRed = FALSE; //first node is always black
    nCount++;
#ifdef _DEBUG
    Check(lpRoot);
#endif //_DEBUG
    return TRUE;
    };

  VOID Remove(_In_ CRedBlackTreeNode *lpDelNode)
    {
    CRedBlackTreeNode *n;

    MX_ASSERT(lpDelNode != NULL);
    MX_ASSERT(lpDelNode->lpTree == this);
    MX_ASSERT(lpDelNode == lpRoot || lpDelNode->lpLeft != NULL || lpDelNode->lpRight != NULL ||
              lpDelNode->lpParent != NULL);

    if (lpDelNode->lpLeft != NULL && lpDelNode->lpRight != NULL)
    {
      CRedBlackTreeNode *lpSucc, *lpSuccRight, *lpSuccParent;
      BOOL bSuccIsRed;

#ifdef _DEBUG
      Check(lpRoot);
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

        //put "lpSucc" in the place of "lpDelNode" and update neighbours
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
        //and update neighbours
        if (lpDelNode->lpParent->lpLeft == lpSucc)
          lpDelNode->lpParent->lpLeft = lpDelNode;
        else
          lpDelNode->lpParent->lpRight = lpDelNode;
        if (lpDelNode->lpRight != NULL)
          lpDelNode->lpRight->lpParent = lpDelNode;

#ifdef _DEBUG
        Check(lpRoot);
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

#ifdef _DEBUG
        Check(lpRoot);
#endif //_DEBUG

        bSuccIsRed = lpSucc->bRed;
        lpSucc->bRed = lpDelNode->bRed;
        lpDelNode->bRed = bSuccIsRed;
      }
    }
    //remove the node
    MX_ASSERT(lpDelNode->lpRight == NULL || lpDelNode->lpLeft == NULL);
    n = (lpDelNode->lpLeft == NULL) ? lpDelNode->lpRight : lpDelNode->lpLeft;
    if (n != NULL)
    {
      n->lpParent = lpDelNode->lpParent;
      if (lpDelNode->lpParent == NULL)
        lpRoot = n;
      else if (lpDelNode == lpDelNode->lpParent->lpLeft)
        lpDelNode->lpParent->lpLeft = n;
      else
        lpDelNode->lpParent->lpRight = n;

      lpDelNode->lpLeft = lpDelNode->lpRight = (CRedBlackTreeNode*)1;
      lpDelNode->lpParent = (CRedBlackTreeNode*)1;

#ifdef _DEBUG
      Check(lpRoot);
#endif //_DEBUG

      if (lpDelNode->bRed == FALSE)
        DeleteFixup(n);

#ifdef _DEBUG
      Check(lpRoot);
#endif //_DEBUG
    }
    else if (lpDelNode->lpParent == NULL)
    {
      MX_ASSERT(nCount == 1);
      lpRoot = NULL; //we are the only node
    }
    else
    {
      if (lpDelNode->bRed == FALSE)
        DeleteFixup(lpDelNode);

#ifdef _DEBUG
      Check(lpRoot);
#endif //_DEBUG

      MX_ASSERT(lpDelNode->lpParent != NULL);
      if (lpDelNode->lpParent != NULL)
      {
        if (lpDelNode->lpParent->lpLeft == lpDelNode)
          lpDelNode->lpParent->lpLeft = NULL;
        else if (lpDelNode->lpParent->lpRight == lpDelNode)
          lpDelNode->lpParent->lpRight = NULL;
      }
#ifdef _DEBUG
      Check(lpRoot);
#endif //_DEBUG
    }

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
  VOID Check(_In_ CRedBlackTreeNode *lpNode)
    {
    if (lpNode == NULL)
      return;
    MX_ASSERT(lpNode->lpLeft == NULL || lpNode->lpLeft->lpParent == lpNode);
    Check(lpNode->lpLeft);
    MX_ASSERT(lpNode->lpRight == NULL || lpNode->lpRight->lpParent == lpNode);
    Check(lpNode->lpRight);
    if (lpNode->lpParent == NULL)
    {
      MX_ASSERT(lpRoot == lpNode);
    }
    else
    {
      MX_ASSERT(lpNode == lpNode->lpParent->lpLeft || lpNode == lpNode->lpParent->lpRight);
    }
    };
#endif //_DEBUG

private:
  friend class Iterator;
  friend class IteratorRev;

  VOID LeftRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    CRedBlackTreeNode *lpChild;

    if (lpNode != NULL)
    {
      lpChild = lpNode->lpRight;
      lpNode->lpRight = lpChild->lpLeft;
      if (lpChild->lpLeft != NULL)
        lpChild->lpLeft->lpParent = lpNode;
      lpChild->lpParent = lpNode->lpParent;
      //instead of checking if left->lpParent is the root as in the book, we
      //count on the root sentinel to implicitly take care of this case
      if (lpNode->lpParent == NULL)
        lpRoot = lpChild;
      else if (lpNode == lpNode->lpParent->lpLeft)
        lpNode->lpParent->lpLeft = lpChild;
      else
        lpNode->lpParent->lpRight = lpChild;
      lpChild->lpLeft = lpNode;
      lpNode->lpParent = lpChild;
    }
    return;
    };

  VOID RightRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    CRedBlackTreeNode *lpChild;

    if (lpNode != NULL)
    {
      lpChild = lpNode->lpLeft;
      lpNode->lpLeft = lpChild->lpRight;
      if (lpChild->lpRight != NULL)
        lpChild->lpRight->lpParent = lpNode;
      //instead of checking if left->lpParent is the root as in the book, we
      //count on the root sentinel to implicitly take care of this case
      lpChild->lpParent = lpNode->lpParent;
      if (lpNode->lpParent == NULL)
        lpRoot = lpChild;
      else if (lpNode == lpNode->lpParent->lpRight)
        lpNode->lpParent->lpRight = lpChild;
      else
        lpNode->lpParent->lpLeft = lpChild;
      lpChild->lpRight = lpNode;
      lpNode->lpParent = lpChild;
    }
    return;
    };

  VOID DeleteFixup(_In_ CRedBlackTreeNode *lpNode)
    {
    CRedBlackTreeNode *lpSibling;

    while (lpNode != lpRoot && lpNode->IsRed() == FALSE)
    {
      if (lpNode == lpNode->GetParent()->GetLeft())
      {
        lpSibling = lpNode->GetParent()->GetRight();
        if (lpSibling->IsRed() != FALSE)
        {
          lpSibling->SetBlack();
          lpNode->GetParent()->SetRed();
          LeftRotate(lpNode->GetParent());
          lpSibling = lpNode->GetParent()->GetRight();
        }
        if (lpSibling->GetRight()->IsRed() == FALSE && lpSibling->GetLeft()->IsRed() == FALSE)
        {
          lpSibling->SetRed();
          lpNode = lpNode->GetParent();
        }
        else
        {
          if (lpSibling->GetRight()->IsRed() == FALSE)
          {
            lpSibling->GetLeft()->SetBlack();
            lpSibling->SetRed();
            RightRotate(lpSibling);
            lpSibling = lpNode->GetParent()->GetRight();
          }
          if (lpNode->GetParent()->IsRed() != FALSE)
            lpSibling->SetRed();
          else
            lpSibling->SetBlack();
          lpNode->GetParent()->SetBlack();
          lpSibling->GetRight()->SetBlack();
          LeftRotate(lpNode->GetParent());
          lpNode = lpRoot; //exit
        }
      }
      else
      {
        lpSibling = lpNode->GetParent()->GetLeft();
        if (lpSibling->IsRed() != FALSE)
        {
          lpSibling->SetBlack();
          lpNode->GetParent()->SetRed();
          RightRotate(lpNode->GetParent());
          lpSibling = lpNode->GetParent()->GetLeft();
        }
        if (lpSibling->GetRight()->IsRed() == FALSE && lpSibling->GetLeft()->IsRed() == FALSE)
        {
          lpSibling->SetRed();
          lpNode = lpNode->GetParent();
        }
        else
        {
          if (lpSibling->GetLeft()->IsRed() == FALSE)
          {
            lpSibling->GetRight()->SetBlack();
            lpSibling->SetRed();
            LeftRotate(lpSibling);
            lpSibling = lpNode->GetParent()->GetLeft();
          }
          if (lpNode->GetParent()->IsRed() != FALSE)
            lpSibling->SetRed();
          else
            lpSibling->SetBlack();
          lpNode->GetParent()->SetBlack();
          lpSibling->GetLeft()->SetBlack();
          RightRotate(lpNode->GetParent());
          lpNode = lpRoot; //exit
        }
      }
    }
    lpNode->SetBlack();
    return;
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

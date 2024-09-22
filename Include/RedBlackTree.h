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

  _inline BOOL IsRed()
    {
    return (this) ? bRed : FALSE;
    };

  _inline VOID Remove();

private:
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
    CheckTree();
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
      CRedBlackTreeNode *lpSucc;
      BOOL bIsRed;

      //get the successor
      lpSucc = lpDelNode->lpRight;
      while (lpSucc->lpLeft != NULL)
        lpSucc = lpSucc->lpLeft;

      //swap colors
      bIsRed = lpSucc->bRed;
      lpSucc->bRed = lpDelNode->bRed;
      lpDelNode->bRed = bIsRed;

      //swap left child (we know the successor has no left node)
      lpSucc->lpLeft = lpDelNode->lpLeft;
      lpDelNode->lpLeft = NULL;

      //relocate successor left child's parent to correct node
      if (lpSucc->lpLeft != NULL)
      {
        MX_ASSERT(lpSucc->lpLeft->lpParent == lpDelNode);
        lpSucc->lpLeft->lpParent = lpSucc;
      }

      //the parent of del-node must now point to successor
      if (lpDelNode->lpParent != NULL)
      {
        if (lpDelNode->lpParent->lpLeft == lpDelNode)
        {
          lpDelNode->lpParent->lpLeft = lpSucc;
        }
        else
        {
          MX_ASSERT(lpDelNode->lpParent->lpRight == lpDelNode);
          lpDelNode->lpParent->lpRight = lpSucc;
        }
      }
      else
      {
        //else the new root is successor
        MX_ASSERT(lpDelNode == lpRoot);
        lpRoot = lpSucc;
      }

      //if the successor is the direct child of del-node...
      if (lpSucc == lpDelNode->lpRight)
      {
        //swap right child
        lpDelNode->lpRight = lpSucc->lpRight;
        lpSucc->lpRight = lpDelNode;

        //swap parents
        lpSucc->lpParent = lpDelNode->lpParent;
        lpDelNode->lpParent = lpSucc;

        //relocate successor right child's parent to correct node
        if (lpDelNode->lpRight != NULL)
        {
          MX_ASSERT(lpDelNode->lpRight->lpParent == lpSucc);
          lpDelNode->lpRight->lpParent = lpDelNode;
        }
      }
      else
      {
        CRedBlackTreeNode *lpTemp;

        //swap right child
        lpTemp = lpSucc->lpRight;
        lpSucc->lpRight = lpDelNode->lpRight;
        lpDelNode->lpRight = lpTemp;

        //swap parents
        lpTemp = lpSucc->lpParent;
        lpSucc->lpParent = lpDelNode->lpParent;
        lpDelNode->lpParent = lpTemp;

        //relocate del-node & successor right child's parent to correct nodes
        if (lpDelNode->lpRight != NULL)
        {
          MX_ASSERT(lpDelNode->lpRight->lpParent == lpSucc);
          lpDelNode->lpRight->lpParent = lpDelNode;
        }
        if (lpSucc->lpRight != NULL)
        {
          MX_ASSERT(lpSucc->lpRight->lpParent == lpDelNode);
          lpSucc->lpRight->lpParent = lpSucc;
        }

        //relocate del-node parent childs to correct nodes
        if (lpDelNode->lpParent != NULL)
        {
          if (lpDelNode->lpParent->lpLeft == lpSucc)
          {
            lpDelNode->lpParent->lpLeft = lpDelNode;
          }
          else
          {
            MX_ASSERT(lpDelNode->lpParent->lpRight == lpSucc);
            lpDelNode->lpParent->lpRight = lpDelNode;
          }
        }
      }

#ifdef _DEBUG
      CheckTree();
#endif //_DEBUG
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

      if (lpDelNode->bRed == FALSE)
      {
        DeleteFixup(n);
      }

#ifdef _DEBUG
      CheckTree();
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
      {
        DeleteFixup(lpDelNode);
      }

      if (lpDelNode->lpParent != NULL)
      {
        if (lpDelNode->lpParent->lpLeft == lpDelNode)
          lpDelNode->lpParent->lpLeft = NULL;
        else if (lpDelNode->lpParent->lpRight == lpDelNode)
          lpDelNode->lpParent->lpRight = NULL;
      }

#ifdef _DEBUG
      CheckTree();
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
  VOID CheckTree()
  {
    if (lpRoot != NULL)
    {
      int nBlackPathCounter = -1;

      MX_ASSERT(lpRoot->bRed == FALSE);
      MX_ASSERT(lpRoot->lpParent == NULL);
      CheckRecursive(lpRoot);
      CheckBlacksRecursive(lpRoot, 1, &nBlackPathCounter);
    }
    return;
    };

  VOID CheckRecursive(_In_opt_ CRedBlackTreeNode *lpNode)
    {
    if (lpNode != NULL)
    {
      MX_ASSERT(lpNode == lpRoot || lpNode->lpParent != NULL);

      if (lpNode->bRed != FALSE)
      {
        MX_ASSERT(lpNode->lpLeft == NULL || lpNode->lpLeft->bRed == FALSE);
        MX_ASSERT(lpNode->lpRight == NULL || lpNode->lpRight->bRed == FALSE);
        MX_ASSERT(lpNode->lpParent->bRed == FALSE);
      }

      MX_ASSERT(lpNode->lpLeft == NULL || (lpNode->lpLeft != lpNode->lpParent && lpNode->lpLeft->lpParent == lpNode));
      MX_ASSERT(lpNode->lpRight == NULL || (lpNode->lpRight != lpNode->lpParent && lpNode->lpRight->lpParent == lpNode
                                            && lpNode->lpRight != lpNode->lpLeft));

      CheckRecursive(lpNode->lpLeft);
      CheckRecursive(lpNode->lpRight);
    }
    return;
    };

  VOID CheckBlacksRecursive(_In_opt_ CRedBlackTreeNode *lpNode, _In_ int nCounter, _In_ int *lpnBlackPathCounter)
    {
    if (lpNode != NULL)
    {
      if (lpNode->bRed == FALSE)
        nCounter++;
      CheckBlacksRecursive(lpNode->lpLeft, nCounter, lpnBlackPathCounter);
      CheckBlacksRecursive(lpNode->lpRight, nCounter, lpnBlackPathCounter);
    }
    else
    {
      if (*lpnBlackPathCounter == -1)
      {
        *lpnBlackPathCounter = nCounter;
      }
      else
      {
        MX_ASSERT(nCounter == *lpnBlackPathCounter);
      }
    }
    return;
    };
#endif //_DEBUG

private:
  friend class Iterator;
  friend class IteratorRev;

  VOID LeftRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    if (lpNode != NULL)
    {
      CRedBlackTreeNode *lpRight;

      lpRight = lpNode->lpRight;
      lpNode->lpRight = lpRight->lpLeft;
      if (lpRight->lpLeft != NULL)
        lpRight->lpLeft->lpParent = lpNode;
      lpRight->lpParent = lpNode->lpParent;
      //instead of checking if left->lpParent is the root as in the book, we
      //count on the root sentinel to implicitly take care of this case
      if (lpNode->lpParent == NULL)
        lpRoot = lpRight;
      else if (lpNode == lpNode->lpParent->lpLeft)
        lpNode->lpParent->lpLeft = lpRight;
      else
        lpNode->lpParent->lpRight = lpRight;
      lpRight->lpLeft = lpNode;
      lpNode->lpParent = lpRight;
    }
    return;
    };

  VOID RightRotate(_In_ CRedBlackTreeNode *lpNode)
    {
    if (lpNode != NULL)
    {
      CRedBlackTreeNode *lpLeft;

      lpLeft = lpNode->lpLeft;
      lpNode->lpLeft = lpLeft->lpRight;
      if (lpLeft->lpRight != NULL)
        lpLeft->lpRight->lpParent = lpNode;
      //instead of checking if left->lpParent is the root as in the book, we
      //count on the root sentinel to implicitly take care of this case
      lpLeft->lpParent = lpNode->lpParent;
      if (lpNode->lpParent == NULL)
        lpRoot = lpLeft;
      else if (lpNode == lpNode->lpParent->lpRight)
        lpNode->lpParent->lpRight = lpLeft;
      else
        lpNode->lpParent->lpLeft = lpLeft;
      lpLeft->lpRight = lpNode;
      lpNode->lpParent = lpLeft;
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

          break;
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

          break;
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

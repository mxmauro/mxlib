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
#include <map>
//-----------------------------------------------------------

namespace MX {

template <class classOrStruct, typename KeyType>
class TRedBlackTree;

template <class classOrStruct, typename KeyType>
class MX_NOVTABLE TRedBlackTreeNode : public virtual CBaseMemObj
{
public:
  typedef TRedBlackTree<classOrStruct, KeyType> _RbTree;
  typedef TRedBlackTreeNode<classOrStruct, KeyType> _RbTreeNode;

  template <class classOrStruct, typename KeyType>
  friend class TRedBlackTree;

  friend class TRedBlackTree<classOrStruct, KeyType>;

public:
  TRedBlackTreeNode() : CBaseMemObj()
    {
    bRed = FALSE;
    lpTree = NULL;
    lpLeft = lpRight = lpParent = NULL;
    return;
    };

  virtual KeyType GetNodeKey() const = 0;

  //Returns -1 if key is less than "this" node's key, 1 if greater or 0 if equal
  virtual int CompareKeys(_In_ KeyType key) const
    {
    KeyType this_key = GetNodeKey();

    if (key < this_key)
      return -1;
    if (key > this_key)
      return 1;
    return 0;
    };

  _RbTreeNode* GetNextNode()
    {
    _RbTreeNode *lpSucc, *lpNode2;

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

  _RbTreeNode* GetPrevNode()
    {
    _RbTreeNode *lpPred, *lpNode2;

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

  _inline _RbTreeNode* GetLeftNode()
    {
    return (this) ? lpLeft : NULL;
    };

  _inline _RbTreeNode* GetRightNode()
    {
    return (this) ? lpRight : NULL;
    };

  _inline _RbTreeNode* GetParentNode()
    {
    return (this) ? lpParent : NULL;
    };

  _inline classOrStruct* GetNextEntry()
    {
    _RbTreeNode *lpNode = GetNextNode();
    return (lpNode != NULL) ? (lpNode->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetPrevEntry()
    {
    _RbTreeNode *lpNode = GetPrevNode();
    return (lpNode != NULL) ? (lpNode->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetParentEntry()
    {
    return (lpParent != NULL) ? (lpParent->GetEntry()) : NULL;
    };

  _inline _RbTree* GetTree()
    {
    return lpTree;
    };

  _inline classOrStruct* GetEntry()
    {
    return static_cast<classOrStruct*>(this);
    };

  _inline VOID RemoveNode()
    {
    if (lpTree != NULL)
      lpTree->Remove(this);
    return;
    };

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
  BOOL bRed;
  _RbTree *lpTree;
  _RbTreeNode *lpLeft, *lpRight, *lpParent;
};

//-----------------------------------------------------------

template <class classOrStruct, typename KeyType>
class TRedBlackTree
{
public:
  typedef TRedBlackTree<classOrStruct, KeyType> _RbTree;
  typedef TRedBlackTreeNode<classOrStruct, KeyType> _RbTreeNode;

  MX_DISABLE_COPY_CONSTRUCTOR(_RbTree);

public:
  TRedBlackTree()
    {
    lpRoot = NULL;
    nItemsCount = 0;
    return;
    };

  _inline BOOL IsEmpty() const
    {
    return (lpRoot == NULL) ? TRUE : FALSE;
    };

  _inline SIZE_T GetCount() const
    {
    return nItemsCount;
    };

  _inline classOrStruct* Find(_In_ KeyType _key)
    {
    _RbTreeNode *lpNode = lpRoot;
    int comp;

    while (lpNode != NULL)
    {
      comp = lpNode->CompareKeys(_key);
      if (comp == 0)
        return lpNode->GetEntry();
      lpNode = (comp < 0) ? lpNode->lpLeft : lpNode->lpRight;
    }
    return NULL;
    };

  _inline classOrStruct* GetFirst()
    {
    _RbTreeNode *lpNode;

    if (lpRoot == NULL)
      return NULL;
    lpNode = lpRoot;
    while (lpNode->lpLeft != NULL)
      lpNode = lpNode->lpLeft;
    return lpNode->GetEntry();
    };

  _inline classOrStruct* GetLast()
    {
    _RbTreeNode *lpNode;

    if (lpRoot == NULL)
      return NULL;
    lpNode = lpRoot;
    while (lpNode->lpRight != NULL)
      lpNode = lpNode->lpRight;
    return lpNode->GetEntry();
    };

  //Try to get entry with key greater or equal to the specified one. Else get nearest less.
  _inline classOrStruct* GetCeiling(_In_ KeyType _key)
    {
    _RbTreeNode *lpNode = lpRoot, *lpParent;
    int comp;

    while (lpNode != NULL)
    {
      comp = lpNode->CompareKeys(_key);
      if (comp == 0)
        return lpNode->GetEntry();
      if (comp < 0)
      {
        if (lpNode->lpLeft == NULL)
          return lpNode->GetEntry();
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
          return (lpParent != NULL) ? lpParent->GetEntry() : NULL;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key less or equal to the specified one. Else get nearest greater.
  _inline classOrStruct* GetFloor(_In_ KeyType _key)
    {
    _RbTreeNode *lpNode = lpRoot, *lpParent;
    int comp;

    while (lpNode != NULL)
    {
      comp = lpNode->CompareKeys(_key);
      if (comp == 0)
        return lpNode->GetEntry();
      if (comp > 0)
      {
        if (lpNode->lpRight == NULL)
          return lpNode->GetEntry();
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
          return (lpParent != NULL) ? lpParent->GetEntry() : NULL;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key greater or equal to the specified one. Else null
  _inline classOrStruct* GetHigher(_In_ KeyType _key)
    {
    _RbTreeNode *lpNode = lpRoot, *lpParent;
    int comp;

    while (lpNode != NULL)
    {
      comp = lpNode->CompareKeys(_key);
      if (comp < 0)
      {
        if (lpNode->lpLeft == NULL)
          return lpNode->GetEntry();
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
          return (lpParent != NULL) ? lpParent->GetEntry() : NULL;
        }
      }
    }
    return NULL;
    };

  //Try to get entry with key less or equal to the specified one. Else null
  _inline classOrStruct* GetLower(_In_ KeyType _key)
    {
    _RbTreeNode *lpNode = lpRoot, *lpParent;
    int comp;

    while (lpNode != NULL)
    {
      comp = lpNode->CompareKeys(_key);
      if (comp > 0)
      {
        if (lpNode->lpRight == NULL)
          return lpNode->GetEntry();
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
          return (lpParent != NULL) ? lpParent->GetEntry() : NULL;
        }
      }
    }
    return NULL;
    };

  BOOL Insert(_In_ _RbTreeNode *lpNewNode, _In_opt_ BOOL bAllowDuplicates = FALSE,
              _In_opt_ _RbTreeNode **lplpMatchingNode = NULL)
    {
    _RbTreeNode *lpNode, *lpParent, *lpUncle;
    int res;

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
      nItemsCount++;
      return TRUE;
    }

#ifdef _DEBUG
    Check(lpRoot);
#endif //_DEBUG

    lpNode = lpRoot;
    do
    {
      lpParent = lpNode;
      res = lpNode->CompareKeys(lpNewNode->GetNodeKey());
      if (res == 0 && bAllowDuplicates == FALSE)
      {
        if (lplpMatchingNode != NULL)
          *lplpMatchingNode = lpNode;
        return FALSE;
      }
      lpNode = (res < 0) ? lpNode->lpLeft : lpNode->lpRight;
    }
    while (lpNode != NULL);
    lpNewNode->lpLeft = lpNewNode->lpRight = NULL;
    lpNewNode->lpParent = lpParent;
    if (res < 0)
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
      if (lpNode->GetParentNode() == lpNode->GetParentNode()->GetParentNode()->GetLeftNode())
      {
        lpUncle = lpNode->GetParentNode()->GetParentNode()->GetRightNode();
        if (lpUncle->IsRed() != FALSE)
        {
          lpNode->GetParentNode()->SetBlack();
          lpUncle->SetBlack();
          lpNode->GetParentNode()->GetParentNode()->SetRed();
          lpNode = lpNode->GetParentNode()->GetParentNode();
        }
        else
        {
          if (lpNode == lpNode->GetParentNode()->GetRightNode())
          {
            lpNode = lpNode->GetParentNode();
            LeftRotate(lpNode);
          }
          lpNode->GetParentNode()->SetBlack();
          lpNode->GetParentNode()->GetParentNode()->SetRed();
          RightRotate(lpNode->GetParentNode()->GetParentNode());
        }
      }
      else
      {
        lpUncle = lpNode->GetParentNode()->GetParentNode()->GetLeftNode();
        if (lpUncle->IsRed() != FALSE)
        {
          lpNode->GetParentNode()->SetBlack();
          lpUncle->SetBlack();
          lpNode->GetParentNode()->GetParentNode()->SetRed();
          lpNode = lpNode->GetParentNode()->GetParentNode();
        }
        else
        {
          if (lpNode == lpNode->GetParentNode()->GetLeftNode())
          {
            lpNode = lpNode->GetParentNode();
            RightRotate(lpNode);
          }
          lpNode->GetParentNode()->SetBlack();
          lpNode->GetParentNode()->GetParentNode()->SetRed();
          LeftRotate(lpNode->GetParentNode()->GetParentNode());
        }
      }
    }
    lpRoot->bRed = FALSE; //first node is always black
    nItemsCount++;
#ifdef _DEBUG
    Check(lpRoot);
#endif //_DEBUG
    return TRUE;
    };

  VOID Remove(_In_ _RbTreeNode *lpDelNode)
    {
    _RbTreeNode *n;

    MX_ASSERT(lpDelNode != NULL);
    MX_ASSERT(lpDelNode->lpTree == this);
    MX_ASSERT(lpDelNode == lpRoot || lpDelNode->lpLeft != NULL || lpDelNode->lpRight != NULL || lpDelNode->lpParent != NULL);

    if (lpDelNode->lpLeft != NULL && lpDelNode->lpRight != NULL)
    {
      _RbTreeNode *lpSucc, *lpSuccRight, *lpSuccParent;
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

      lpDelNode->lpLeft = lpDelNode->lpRight = (_RbTreeNode *)1;
      lpDelNode->lpParent = (_RbTreeNode *)1;

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
      MX_ASSERT(nItemsCount == 1);
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
    nItemsCount--;
    return;
    };

#ifdef _DEBUG
  VOID Check(_In_ _RbTreeNode *lpNode)
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

  VOID LeftRotate(_Inout_ _RbTreeNode *lpNode)
    {
    _RbTreeNode *lpChild;

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

  VOID RightRotate(_Inout_ _RbTreeNode *lpNode)
    {
    _RbTreeNode *lpChild;

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

  VOID DeleteFixup(_Inout_ _RbTreeNode *lpNode)
    {
    _RbTreeNode *lpSibling;

    while (lpNode != lpRoot && lpNode->IsRed() == FALSE)
    {
      if (lpNode == lpNode->GetParentNode()->GetLeftNode())
      {
        lpSibling = lpNode->GetParentNode()->GetRightNode();
        if (lpSibling->IsRed() != FALSE)
        {
          lpSibling->SetBlack();
          lpNode->GetParentNode()->SetRed();
          LeftRotate(lpNode->GetParentNode());
          lpSibling = lpNode->GetParentNode()->GetRightNode();
        }
        if (lpSibling->GetRightNode()->IsRed() == FALSE && lpSibling->GetLeftNode()->IsRed() == FALSE)
        {
          lpSibling->SetRed();
          lpNode = lpNode->GetParentNode();
        }
        else
        {
          if (lpSibling->GetRightNode()->IsRed() == FALSE)
          {
            lpSibling->GetLeftNode()->SetBlack();
            lpSibling->SetRed();
            RightRotate(lpSibling);
            lpSibling = lpNode->GetParentNode()->GetRightNode();
          }
          if (lpNode->GetParentNode()->IsRed() != FALSE)
            lpSibling->SetRed();
          else
            lpSibling->SetBlack();
          lpNode->GetParentNode()->SetBlack();
          lpSibling->GetRightNode()->SetBlack();
          LeftRotate(lpNode->GetParentNode());
          lpNode = lpRoot; //exit
        }
      }
      else
      {
        lpSibling = lpNode->GetParentNode()->GetLeftNode();
        if (lpSibling->IsRed() != FALSE)
        {
          lpSibling->SetBlack();
          lpNode->GetParentNode()->SetRed();
          RightRotate(lpNode->GetParentNode());
          lpSibling = lpNode->GetParentNode()->GetLeftNode();
        }
        if (lpSibling->GetRightNode()->IsRed() == FALSE && lpSibling->GetLeftNode()->IsRed() == FALSE)
        {
          lpSibling->SetRed();
          lpNode = lpNode->GetParentNode();
        }
        else
        {
          if (lpSibling->GetLeftNode()->IsRed() == FALSE)
          {
            lpSibling->GetRightNode()->SetBlack();
            lpSibling->SetRed();
            LeftRotate(lpSibling);
            lpSibling = lpNode->GetParentNode()->GetLeftNode();
          }
          if (lpNode->GetParentNode()->IsRed() != FALSE)
            lpSibling->SetRed();
          else
            lpSibling->SetBlack();
          lpNode->GetParentNode()->SetBlack();
          lpSibling->GetLeftNode()->SetBlack();
          RightRotate(lpNode->GetParentNode());
          lpNode = lpRoot; //exit
        }
      }
    }
    lpNode->SetBlack();
    return;
    };

  _inline _RbTreeNode* GetFirstNode()
    {
    _RbTreeNode *lpNode = lpRoot;
    if (lpNode != NULL)
    {
      while (lpNode->lpLeft != NULL)
        lpNode = lpNode->lpLeft;
    }
    return lpNode;
    };

  _inline _RbTreeNode* GetLastNode()
    {
    _RbTreeNode *lpNode = lpRoot;
    if (lpNode != NULL)
    {
      while (lpNode->lpRight != NULL)
        lpNode = lpNode->lpRight;
    }
    return lpNode;
    };

  //---------------------------------------------------------

public:
  class Iterator
  {
  public:
    classOrStruct* Begin(_In_ _RbTree &_tree)
      {
      lpNextCursor = _tree.GetFirstNode();
      return Next();
      };

    classOrStruct* Begin(_In_ const _RbTree &_tree)
      {
      return Begin(const_cast<_RbTree&>(_tree));
      };

    classOrStruct* Next()
      {
      _RbTreeNode *lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->GetNextNode();
      return lpCursor->GetEntry();
      };

  private:
    _RbTreeNode *lpNextCursor;
  };

  //---------------------------------------------------------

  class IteratorRev
  {
  public:
    classOrStruct* Begin(_In_ _RbTree &_tree)
      {
      lpNextCursor = _tree.GetLast();
      return Next();
      };

    classOrStruct* Begin(_In_ const _RbTree &_tree)
      {
      return Begin(const_cast<_RbTree&>(_tree));
      };

    classOrStruct* Next()
      {
      _RbTreeNode *lpCursor = lpNextCursor;
      if (lpCursor == NULL)
        return NULL;
      lpNextCursor = lpCursor->GetPrevNode();
      return lpCursor->GetEntry();
      };

  private:
    _RbTreeNode *lpNextCursor;
  };

private:
  template <class classOrStruct, typename KeyType>
  friend class TRedBlackTreeNode;

  _RbTreeNode *lpRoot;
  SIZE_T nItemsCount;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_REDBLACKTREE_H

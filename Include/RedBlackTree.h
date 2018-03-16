/*
 * Original code by Emin Martinian (http://web.mit.edu/~emin/www.old/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that neither the name of Emin
 * Martinian nor the names of any contributors are be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Modified by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 **/
#ifndef _MX_REDBLACKTREE_H
#define _MX_REDBLACKTREE_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

template <class classOrStruct>
class TRedBlackTree;

template <class classOrStruct>
class TRedBlackTreeNode : public virtual CBaseMemObj
{
public:
  typedef TRedBlackTree<classOrStruct> _RbTree;
  typedef TRedBlackTreeNode<classOrStruct> _RbTreeNode;

  template <class classOrStruct>
  friend class TRedBlackTree;

  friend class TRedBlackTree<classOrStruct>;

public:
  TRedBlackTreeNode() : CBaseMemObj()
    {
    bRed = FALSE;
    lpTree = NULL;
    lpLeft = lpRight = lpParent = NULL;
    return;
    };

  virtual BOOL IsGreaterThan(_In_ classOrStruct *lpOtherNode)
    {
    return FALSE;
    };

  _RbTreeNode* GetNextNode()
    {
    _RbTreeNode *lpNode, *lpNode2;

    if ((lpNode = lpRight) == NULL)
      return NULL;
    if (lpNode != &(lpTree->cNil))
    {
      while (lpNode->lpLeft != &(lpTree->cNil))
        lpNode = lpNode->lpLeft;
      return static_cast<_RbTreeNode*>(lpNode);
    }
    lpNode = lpParent;
    lpNode2 = this;
    while (lpNode2 == lpNode->lpRight)
    {
      lpNode2 = lpNode;
      lpNode = lpNode->lpParent;
    }
    return (lpNode != &(lpTree->cRoot)) ? static_cast<_RbTreeNode*>(lpNode) : NULL;
    };

  _RbTreeNode* GetPrevNode()
    {
    _RbTreeNode *lpNode, *lpNode2;

    if ((lpNode = lpLeft) == NULL)
      return NULL;
    if (lpNode != &(lpTree->cNil))
    {
      while (lpNode->lpRight != &(lpTree->cNil))
        lpNode = lpNode->lpRight;
      return lpNode;
    }
    lpNode = lpParent;
    lpNode2 = this;
    while (lpNode2 == lpNode->lpLeft)
    {
      if (lpNode == &(lpTree->cRoot))
        return NULL;
      lpNode2 = lpNode;
      lpNode = lpNode->lpParent;
    }
    return static_cast<_RbTreeNode*>(lpNode);
    };

  _inline _RbTreeNode* GetParentNode()
    {
    return lpParent;
    };

  _inline classOrStruct* GetNextEntry()
    {
    RedBlackTreeNode *lpNode = GetNextNode();
    return (lpNode != NULL) ? (lpNode->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetPrevEntry()
    {
    RedBlackTreeNode *lpNode = GetPrevNode();
    return (lpNode != NULL) ? (lpNode->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetParentEntry()
    {
    return (lpParent != NULL && lpParent != &cRoot) ? (lpParent->GetEntry()) : NULL;
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

  _inline VOID RemoveAll()
    {
    while (cRoot.lpLeft != &cNil)
      Remove(cRoot.lpLeft);
    while (cRoot.lpRight != &cNil)
      Remove(cRoot.lpRight);
    return;
    };

private:
  BOOL bRed;
  _RbTree *lpTree;
  _RbTreeNode *lpLeft, *lpRight, *lpParent;
};

//-----------------------------------------------------------

template <class classOrStruct>
class TRedBlackTree
{
  MX_DISABLE_COPY_CONSTRUCTOR(TRedBlackTree<classOrStruct>);
public:
  typedef TRedBlackTree<classOrStruct> _RbTree;
  typedef TRedBlackTreeNode<classOrStruct> _RbTreeNode;

  TRedBlackTree()
    {
    cRoot.lpLeft = cRoot.lpRight = cRoot.lpParent = &cNil;
    cNil.lpLeft = cNil.lpRight = cNil.lpParent = &cNil;
    cRoot.bRed = cNil.bRed = FALSE;
    return;
    };

  _inline BOOL IsEmpty()
    {
    return (cRoot.lpLeft == &cNil && cRoot.lpRight == &cNil) ? TRUE : FALSE;
    };

  _inline classOrStruct* GetFirst()
    {
    _RbTreeNode *lpNode;

    lpNode = cRoot.lpLeft;
    while (lpNode->lpLeft != &cNil)
      lpNode = lpNode->lpLeft;
    return (lpNode != &cNil) ? (static_cast<_RbTreeNode*>(lpNode)->GetEntry()) : NULL;
    };

  _inline classOrStruct* GetLast()
    {
    _RbTreeNode *lpNode;

    lpNode = (cRoot.lpRight != &cNil) ? cRoot.lpRight : cRoot.lpLeft;
    do
    {
      if (lpNode->lpRight != &cNil)
        lpNode = lpNode->lpRight;
      else if (lpNode->lpLeft != &cNil)
        lpNode = lpNode->lpLeft;
      else
        break;
    }
    while (lpNode != &cNil);
    return (lpNode != &cNil) ? (static_cast<_RbTreeNode*>(lpNode)->GetEntry()) : NULL;
    };

  VOID Insert(_In_ _RbTreeNode *lpNewNode)
    {
    _RbTreeNode *lpNode, *lpNode2;

    MX_ASSERT(lpNewNode != NULL);
    lpNode = static_cast<_RbTreeNode*>(lpNewNode);
    MX_ASSERT(lpNode->lpParent == NULL);
    MX_ASSERT(lpNode->lpLeft == NULL && lpNode->lpRight == NULL);
    InsertFixup(lpNode);
    __analysis_assume(lpNode->lpParent != NULL);
    lpNode->lpTree = this;
    lpNode->bRed = TRUE;
    while (lpNode->lpParent->bRed != FALSE)
    {
      if (lpNode->lpParent == lpNode->lpParent->lpParent->lpLeft)
      {
        lpNode2 = lpNode->lpParent->lpParent->lpRight;
        if (lpNode2->bRed != FALSE)
        {
          lpNode->lpParent->bRed = lpNode2->bRed = FALSE;
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
        lpNode2 = lpNode->lpParent->lpParent->lpLeft;
        if (lpNode2->bRed != FALSE)
        {
          lpNode->lpParent->bRed = lpNode2->bRed = FALSE;
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
    cRoot.lpLeft->bRed = FALSE;
    return;
    };

  VOID Remove(_In_ _RbTreeNode *lpDelNode)
    {
    _RbTreeNode *lpNode, *lpNode2, *lpNode3;

    MX_ASSERT(lpDelNode != NULL);
    lpNode = static_cast<_RbTreeNode*>(lpDelNode);
    MX_ASSERT(lpNode->lpTree == this);
    if (lpNode->lpLeft == &cNil || lpNode->lpRight == &cNil)
    {
      lpNode2 = lpNode;
    }
    else
    {
      _RbTreeNode *lpNext = lpDelNode->GetNextNode();
      lpNode2 = (lpNext != NULL) ? static_cast<_RbTreeNode*>(lpNext) : NULL;
    }
    if (lpNode2 == NULL)
      lpNode2 = &cNil;
    lpNode3 = (lpNode2->lpLeft == &cNil) ? lpNode2->lpRight : lpNode2->lpLeft;
    lpNode3->lpParent = lpNode2->lpParent;
    if (&cRoot == lpNode3->lpParent)
    {
      cRoot.lpLeft = lpNode3;
    }
    else
    {
      if (lpNode2 == lpNode2->lpParent->lpLeft)
        lpNode2->lpParent->lpLeft = lpNode3;
      else
        lpNode2->lpParent->lpRight = lpNode3;
    }
    if (lpNode2 != lpNode)
    {
      lpNode2->lpLeft = lpNode->lpLeft;
      lpNode2->lpRight = lpNode->lpRight;
      lpNode2->lpParent = lpNode->lpParent;
      lpNode->lpLeft->lpParent = lpNode->lpRight->lpParent = lpNode2;
      if (lpNode == lpNode->lpParent->lpLeft)
        lpNode->lpParent->lpLeft = lpNode2;
      else
        lpNode->lpParent->lpRight = lpNode2;
      if (lpNode2->bRed == FALSE)
      {
        lpNode2->bRed = lpNode->bRed;
        DeleteFixup(lpNode3);
      }
      else
      {
        lpNode2->bRed = lpNode->bRed; 
      }
    }
    else
    {
      if (lpNode2->bRed == FALSE)
        DeleteFixup(lpNode3);
    }
    lpNode->lpTree = NULL;
    lpNode->lpParent = NULL;
    lpNode->lpLeft = lpNode->lpRight = NULL;
    return;
    };

private:
  friend class Iterator;
  friend class IteratorRev;

  VOID LeftRotate(_Inout_ _RbTreeNode *left)
    {
    _RbTreeNode *right;

    right = left->lpRight;
    left->lpRight = right->lpLeft;
    if (right->lpLeft != &cNil)
      right->lpLeft->lpParent = left;
    right->lpParent = left->lpParent;
    //instead of checking if left->lpParent is the root as in the book, we
    //count on the root sentinel to implicitly take care of this case
    if (left == left->lpParent->lpLeft)
      left->lpParent->lpLeft = right;
    else
      left->lpParent->lpRight = right;
    right->lpLeft = left;
    left->lpParent = right;
    return;
    };

  VOID RightRotate(_Inout_ _RbTreeNode *right)
    {
    _RbTreeNode *left;

    left = right->lpLeft;
    right->lpLeft = left->lpRight;
    if (left->lpRight != &cNil)
      left->lpRight->lpParent = right;
    //instead of checking if left->lpParent is the root as in the book, we
    //count on the root sentinel to implicitly take care of this case
    left->lpParent = right->lpParent;
    if (right == right->lpParent->lpLeft)
      right->lpParent->lpLeft = left;
    else
      right->lpParent->lpRight = left;
    left->lpRight = right;
    right->lpParent = left;
    return;
    };

  VOID InsertFixup(_Inout_ _RbTreeNode *lpNodeToAdd)
    {
    _RbTreeNode *left, *right;

    lpNodeToAdd->lpLeft = lpNodeToAdd->lpRight = &cNil;
    right = &cRoot;
    left = cRoot.lpLeft;
    while (left != &cNil)
    {
      right = left;
      if (left->IsGreaterThan(static_cast<classOrStruct*>(lpNodeToAdd)) != FALSE)
        left = left->lpLeft;
      else
        left = left->lpRight;
    }
    lpNodeToAdd->lpParent = right;
    if (right == &cRoot || right->IsGreaterThan(static_cast<classOrStruct*>(lpNodeToAdd)) != FALSE)
      right->lpLeft = lpNodeToAdd;
    else
      right->lpRight = lpNodeToAdd;
    return;
    };

  VOID DeleteFixup(_Inout_ _RbTreeNode *lpNode)
    {
    _RbTreeNode *lpNode2, *rootLeft = cRoot.lpLeft;

    while (lpNode->bRed == FALSE && rootLeft != lpNode)
    {
      if (lpNode == lpNode->lpParent->lpLeft)
      {
        lpNode2 = lpNode->lpParent->lpRight;
        if (lpNode2->bRed != FALSE)
        {
          lpNode2->bRed = FALSE;
          lpNode->lpParent->bRed = TRUE;
          LeftRotate(lpNode->lpParent);
          lpNode2 = lpNode->lpParent->lpRight;
        }
        if (lpNode2->lpRight->bRed == FALSE && lpNode2->lpLeft->bRed == FALSE)
        {
          lpNode2->bRed = TRUE;
          lpNode = lpNode->lpParent;
        }
        else
        {
          if (lpNode2->lpRight->bRed == FALSE)
          {
            lpNode2->lpLeft->bRed = FALSE;
            lpNode2->bRed = TRUE;
            RightRotate(lpNode2);
            lpNode2 = lpNode->lpParent->lpRight;
          }
          lpNode2->bRed = lpNode->lpParent->bRed;
          lpNode->lpParent->bRed = lpNode2->lpRight->bRed = FALSE;
          LeftRotate(lpNode->lpParent);
          break;;
        }
      }
      else
      {
        lpNode2 = lpNode->lpParent->lpLeft;
        if (lpNode2->bRed != FALSE)
        {
          lpNode2->bRed = FALSE;
          lpNode->lpParent->bRed = TRUE;
          RightRotate(lpNode->lpParent);
          lpNode2 = lpNode->lpParent->lpLeft;
        }
        if (lpNode2->lpRight->bRed == FALSE && lpNode2->lpLeft->bRed == FALSE)
        {
          lpNode2->bRed = TRUE;
          lpNode = lpNode->lpParent;
        }
        else
        {
          if (lpNode2->lpLeft->bRed == FALSE)
          {
            lpNode2->lpRight->bRed = FALSE;
            lpNode2->bRed = TRUE;
            LeftRotate(lpNode2);
            lpNode2 = lpNode->lpParent->lpLeft;
          }
          lpNode2->bRed = lpNode->lpParent->bRed;
          lpNode->lpParent->bRed = lpNode2->lpLeft->bRed = FALSE;
          RightRotate(lpNode->lpParent);
          break;
        }
      }
    }
    lpNode->bRed = FALSE;
    return;
    };

  _inline _RbTreeNode* GetFirstNode()
    {
    _RbTreeNode *lpNode;

    lpNode = cRoot.lpLeft;
    while (lpNode->lpLeft != &cNil)
      lpNode = lpNode->lpLeft;
    return (lpNode != &cNil) ? lpNode : NULL;
    };

  _inline _RbTreeNode* GetLastNode()
    {
    _RbTreeNode *lpNode;

    lpNode = (cRoot.lpRight != &cNil) ? cRoot.lpRight : cRoot.lpLeft;
    do
    {
      if (lpNode->lpRight != &cNil)
        lpNode = lpNode->lpRight;
      else if (lpNode->lpLeft != &cNil)
        lpNode = lpNode->lpLeft;
      else
        break;
    }
    while (lpNode != &cNil);
    return (lpNode != &cNil) ? lpNode : NULL;
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
  template <class classOrStruct>
  friend class TRedBlackTreeNode;

  _RbTreeNode cRoot, cNil;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_REDBLACKTREE_H

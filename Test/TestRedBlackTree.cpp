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
#include "TestRedBlackTree.h"
#include <RedBlackTree.h>

//-----------------------------------------------------------

class CNode : public virtual MX::CBaseMemObj
{
public:
  CNode() : MX::CBaseMemObj()
    {
    nValue = 0;
    return;
    };

  ~CNode()
    {
    MX_ASSERT(cTreeNode.GetTree() == NULL);
    return;
    };

public:
  MX::CRedBlackTreeNode cTreeNode;
  ULONG nValue;
};

//-----------------------------------------------------------

static BOOL CreateTree(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes, ...);
static CNode* SearchTree(_In_ MX::CRedBlackTree &cTree, _In_ ULONG nValue);
static BOOL CheckTreeNode(_In_ MX::CRedBlackTree &cTree, _In_ CNode *lpNode, _In_opt_ CNode *lpParent,
                          _In_opt_ CNode *lpLeft, _In_opt_ CNode *lpRight, _In_ BOOL bIsRed);

static BOOL CreateTree_Example133CormenPage314(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes);
static BOOL CreateTree_Example133CormenPage317(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes);
static BOOL CreateTree_Example133CormenPage317Inverted(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes);
static HRESULT TestDeletion();
static HRESULT TestExample133CormenPage314MinimumMaximum();
static HRESULT TestThreeNodesDeleteRight();
static HRESULT TestThreeNodesDeleteLeft();
static HRESULT TestThreeNodesDeleteRoot();
static HRESULT TestExample133CormenPage314Delete3();
static HRESULT TestExample133CormenPage314Delete6();
static HRESULT TestExample133CormenPage314Delete9();
static HRESULT TestExample133CormenPage314Delete18();
static HRESULT TestExample133CormenPage314Delete18e19e20();
static HRESULT TestExample133CormenPage317CaseA();
static HRESULT TestExample133CormenPage317CaseAInverted();
static HRESULT TestExample137CormenPage329CaseA();

static HRESULT TestDuplicates(_In_ SIZE_T nCount);
static HRESULT TestNonDuplicates(_In_ SIZE_T nCount);

static int InsertNode(_In_opt_ LPVOID lpContext, _In_ MX::CRedBlackTreeNode *lpNode1,
                      _In_ MX::CRedBlackTreeNode *lpNode2);
static int SearchNode(_In_opt_ LPVOID lpContext, _In_ PULONG lpnValue, _In_ MX::CRedBlackTreeNode *lpNode);

//-----------------------------------------------------------

int TestRedBlackTree()
{
  HRESULT hRes;

#pragma warning(suppress : 28159)
  srand(::GetTickCount());

  wprintf_s(L"Running Deletion test... ");
  hRes = TestDeletion();
  if (FAILED(hRes))
  {
on_error:
    if (hRes == E_OUTOFMEMORY)
      wprintf_s(L"\nError: Not enough memory.\n");
    else
      wprintf_s(L"\nError: Failed.\n");
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Minimum Maximum... ");
  hRes = TestExample133CormenPage314MinimumMaximum();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  wprintf_s(L"Running Delete Right... ");
  hRes = TestThreeNodesDeleteRight();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Delete Left... ");
  hRes = TestThreeNodesDeleteLeft();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Delete Root... ");
  hRes = TestThreeNodesDeleteRoot();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Delete 3... ");
  hRes = TestExample133CormenPage314Delete3();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Delete 6... ");
  hRes = TestExample133CormenPage314Delete6();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Delete 9... ");
  hRes = TestExample133CormenPage314Delete9();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Delete 18... ");
  hRes = TestExample133CormenPage314Delete18();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 314 - Delete 18e19e20... ");
  hRes = TestExample133CormenPage314Delete18e19e20();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 317 - Case A... ");
  hRes = TestExample133CormenPage317CaseA();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 317 - Case A Inverted... ");
  hRes = TestExample133CormenPage317CaseAInverted();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  wprintf_s(L"Running Example 133 - Cormen Page 329 - Case A... ");
  hRes = TestExample137CormenPage329CaseA();
  if (FAILED(hRes))
    goto on_error;
  wprintf_s(L"OK\n");

  //----

  for (SIZE_T nPass = 1; nPass <= 3; nPass++)
  {
    SIZE_T nCount = nPass * 10000;

    wprintf_s(L"Running RB-Tree test with duplicates (%Iu elements)... ", nCount);
    hRes = TestDuplicates(nCount);
    if (FAILED(hRes))
      goto on_error;
    wprintf_s(L"OK\n");

    //----

    wprintf_s(L"Running RB-Tree test with no duplicates (%Iu elements)... ", nCount);
    hRes = TestNonDuplicates(nCount);
    if (FAILED(hRes))
      goto on_error;
    wprintf_s(L"OK\n");
  }

  //done
  return (int)S_OK;
}

//-----------------------------------------------------------

static BOOL CreateTree_Example133CormenPage314(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes)
{
  return CreateTree(cTree, lplpNodes, 7, 4, 11, 3, 6, 9, 18, 2, 14, 19, 12, 17, 22, 20, 0);
}

static BOOL CreateTree_Example133CormenPage317(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes)
{
  return CreateTree(cTree, lplpNodes, 11, 2, 14, 1, 7, 15, 5, 8, 0);
}

static BOOL CreateTree_Example133CormenPage317Inverted(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes)
{
  return CreateTree(cTree, lplpNodes, 11, 2, 20, 1, 15, 25, 14, 16, 0);
}

static HRESULT TestDeletion()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;

  //        4
  //       / \
  //     2     6
  //    / \   / \
  //   1   3 5   7
  if (CreateTree(cTree, &lpNodes, 4, 2, 6, 1, 3, 5, 7, 0) == FALSE)
    return E_OUTOFMEMORY;
  if (SearchTree(cTree, 4)->cTreeNode.GetParent() != NULL)
  {
on_failure:
    cTree.RemoveAll();
    delete[] lpNodes;
    return E_FAIL;
  }
  cTree.Remove(&(SearchTree(cTree, 4)->cTreeNode));

  cTree.RemoveAll();
  delete[] lpNodes;

  //---------------------------------------------------------------------------

  //       3
  //      / \
  //     2   5
  //    /     \
  //   1       6
  if (CreateTree(cTree, &lpNodes, 3, 2, 5, 1, 6) == FALSE)
    return E_OUTOFMEMORY;
  if (SearchTree(cTree, 3)->cTreeNode.GetParent() != NULL)
    goto on_failure;

  cTree.Remove(&(SearchTree(cTree, 3)->cTreeNode));

  cTree.RemoveAll();
  delete[] lpNodes;

  //---------------------------------------------------------------------------

  //       3
  //      / \
  //     2   6
  //    /   / \
  //   1   4   7
  //        \
  //         5
  if (CreateTree(cTree, &lpNodes, 3, 2, 6, 1, 4, 7, 5, 0) == FALSE)
    return E_OUTOFMEMORY;
  if (SearchTree(cTree, 3)->cTreeNode.GetParent() != NULL)
    goto on_failure;
  cTree.Remove(&(SearchTree(cTree, 3)->cTreeNode));

  cTree.RemoveAll();
  delete[] lpNodes;

  //---------------------------------------------------------------------------

  //       3
  //      / \
  //     2   5
  //    /
  //   1
  if (CreateTree(cTree, &lpNodes, 3, 2, 5, 1, 0) == FALSE)
    return E_OUTOFMEMORY;
  if (SearchTree(cTree, 3)->cTreeNode.GetParent() != NULL)
    goto on_failure;
  cTree.Remove(&(SearchTree(cTree, 3)->cTreeNode));

  cTree.RemoveAll();
  delete[] lpNodes;

  //---------------------------------------------------------------------------

  //       3
  //      / \
  //     2   6
  //    /   / \
  //   1   5   8
  //      /   / \
  //     4   7   9
  if (CreateTree(cTree, &lpNodes, 3, 2, 6, 1, 5, 8, 4, 7, 9, 0) == FALSE)
    return E_OUTOFMEMORY;
  if (SearchTree(cTree, 3)->cTreeNode.GetParent() != NULL)
    goto on_failure;
  cTree.Remove(&(SearchTree(cTree, 6)->cTreeNode));

  cTree.RemoveAll();
  delete[] lpNodes;

  //---------------------------------------------------------------------------


  //done
  return S_OK;
}

static HRESULT TestExample133CormenPage314MinimumMaximum()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *lpFirst = CONTAINING_RECORD(cTree.GetFirst(), CNode, cTreeNode);
  CNode *lpLast = CONTAINING_RECORD(cTree.GetLast(), CNode, cTreeNode);

  if (CheckTreeNode(cTree, lpFirst, SearchTree(cTree, 3), NULL, NULL, TRUE) == FALSE ||
      CheckTreeNode(cTree, lpLast, SearchTree(cTree, 20), NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestThreeNodesDeleteRight()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree(cTree, &lpNodes, 7, 4, 11, 0) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node7 = SearchTree(cTree, 7);
  CNode *node4 = SearchTree(cTree, 4);
  CNode *node11 = SearchTree(cTree, 11);

  cTree.Remove(&(node11->cTreeNode));

  if (CheckTreeNode(cTree, node7, NULL, node4, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node4, node7, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestThreeNodesDeleteLeft()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree(cTree, &lpNodes, 7, 4, 11, 0) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node7 = SearchTree(cTree, 7);
  CNode *node4 = SearchTree(cTree, 4);
  CNode *node11 = SearchTree(cTree, 11);

  cTree.Remove(&(node4->cTreeNode));

  if (CheckTreeNode(cTree, node7, NULL, NULL, node11, FALSE) == FALSE ||
      CheckTreeNode(cTree, node11, node7, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestThreeNodesDeleteRoot()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree(cTree, &lpNodes, 7, 4, 11, 0) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node7 = SearchTree(cTree, 7);
  CNode *node4 = SearchTree(cTree, 4);
  CNode *node11 = SearchTree(cTree, 11);

  cTree.Remove(&(node7->cTreeNode));

  if (CheckTreeNode(cTree, node11, NULL, node4, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node4, node11, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage314Delete3()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node6 = SearchTree(cTree, 6);
  CNode *node2 = SearchTree(cTree, 2);
  CNode *node3 = SearchTree(cTree, 3);
  CNode *node4 = SearchTree(cTree, 4);
  CNode *node7 = SearchTree(cTree, 7);

  cTree.Remove(&(node3->cTreeNode));

  if (CheckTreeNode(cTree, node4, node7, node2, node6, FALSE) == FALSE ||
      CheckTreeNode(cTree, node2, node4, NULL, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node6, node4, NULL, NULL, FALSE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage314Delete6()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node6 = SearchTree(cTree, 6);
  CNode *node2 = SearchTree(cTree, 2);
  CNode *node3 = SearchTree(cTree, 3);
  CNode *node4 = SearchTree(cTree, 4);
  CNode *node7 = SearchTree(cTree, 7);
  CNode *node11 = SearchTree(cTree, 11);

  cTree.Remove(&(node6->cTreeNode));

  if (CheckTreeNode(cTree, node7, NULL, node3, node11, FALSE) == FALSE ||
      CheckTreeNode(cTree, node3, node7, node2, node4, FALSE) == FALSE ||
      CheckTreeNode(cTree, node2, node3, NULL, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node4, node3, NULL, NULL, FALSE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage314Delete9()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node7 = SearchTree(cTree, 7);
  CNode *node11 = SearchTree(cTree, 11);
  CNode *node9 = SearchTree(cTree, 9);
  CNode *node18 = SearchTree(cTree, 18);
  CNode *node14 = SearchTree(cTree, 14);
  CNode *node20 = SearchTree(cTree, 20);
  CNode *node17 = SearchTree(cTree, 17);
  CNode *node19 = SearchTree(cTree, 19);
  CNode *node22 = SearchTree(cTree, 22);

  cTree.Remove(&(node9->cTreeNode));

  if (CheckTreeNode(cTree, node18, node7, node14, node20, FALSE) == FALSE ||
      CheckTreeNode(cTree, node14, node18, node11, node17, TRUE) == FALSE ||
      CheckTreeNode(cTree, node20, node18, node19, node22, FALSE) == FALSE ||
      CheckTreeNode(cTree, node19, node20, NULL, NULL, TRUE) == FALSE ||
      CheckTreeNode(cTree, node22, node20, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage314Delete18()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node17 = SearchTree(cTree, 17);
  CNode *node19 = SearchTree(cTree, 19);
  CNode *node22 = SearchTree(cTree, 22);
  CNode *node12 = SearchTree(cTree, 12);
  CNode *node7 = SearchTree(cTree, 7);
  CNode *node11 = SearchTree(cTree, 11);
  CNode *node9 = SearchTree(cTree, 9);
  CNode *node18 = SearchTree(cTree, 18);
  CNode *node14 = SearchTree(cTree, 14);
  CNode *node20 = SearchTree(cTree, 20);

  cTree.Remove(&(node18->cTreeNode));

  if (CheckTreeNode(cTree, node11, node7, node9, node19, FALSE) == FALSE ||
      CheckTreeNode(cTree, node9, node11, NULL, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node19, node11, node14, node20, TRUE) == FALSE ||
      CheckTreeNode(cTree, node14, node19, node12, node17, FALSE) == FALSE ||
      CheckTreeNode(cTree, node20, node19, NULL, node22, FALSE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage314Delete18e19e20()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  CNode *node17 = SearchTree(cTree, 17);
  CNode *node22 = SearchTree(cTree, 22);
  CNode *node12 = SearchTree(cTree, 12);
  CNode *node7 = SearchTree(cTree, 7);
  CNode *node11 = SearchTree(cTree, 11);
  CNode *node9 = SearchTree(cTree, 9);
  CNode *node14 = SearchTree(cTree, 14);

  cTree.Remove(&(SearchTree(cTree, 18)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 19)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 20)->cTreeNode));

  if (CheckTreeNode(cTree, node11, node7, node9, node14, FALSE) == FALSE ||
      CheckTreeNode(cTree, node9, node11, NULL, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node14, node11, node12, node22, TRUE) == FALSE ||
      CheckTreeNode(cTree, node22, node14, node17, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node17, node22, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage317CaseA()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage317(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  SIZE_T nIdx = cTree.GetCount();
  lpNodes[nIdx].nValue = 4;
  cTree.Insert(&(lpNodes[nIdx].cTreeNode), &InsertNode);
  CNode *node4 = &lpNodes[nIdx];

  CNode *node5 = SearchTree(cTree, 5);
  CNode *node2 = SearchTree(cTree, 2);
  CNode *node7 = SearchTree(cTree, 7);
  CNode *node11 = SearchTree(cTree, 11);
  CNode *node1 = SearchTree(cTree, 1);
  CNode *node8 = SearchTree(cTree, 8);
  CNode *node14 = SearchTree(cTree, 14);

  if (CheckTreeNode(cTree, node7, NULL, node2, node11, FALSE) == FALSE ||
      CheckTreeNode(cTree, node2, node7, node1, node5, TRUE) == FALSE ||
      CheckTreeNode(cTree, node11, node7, node8, node14, TRUE) == FALSE ||
      CheckTreeNode(cTree, node5, node2, node4, NULL, FALSE) == FALSE ||
      CheckTreeNode(cTree, node4, node5, NULL, NULL, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample133CormenPage317CaseAInverted()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  HRESULT hRes;

  if (CreateTree_Example133CormenPage317Inverted(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  SIZE_T nIdx = cTree.GetCount();
  lpNodes[nIdx].nValue = 17;
  cTree.Insert(&(lpNodes[nIdx].cTreeNode), &InsertNode);
  CNode *node17 = &lpNodes[nIdx];

  CNode *node15 = SearchTree(cTree, 15);
  CNode *node2 = SearchTree(cTree, 2);
  CNode *node20 = SearchTree(cTree, 20);
  CNode *node11 = SearchTree(cTree, 11);
  CNode *node16 = SearchTree(cTree, 16);
  CNode *node25 = SearchTree(cTree, 25);
  CNode *node14 = SearchTree(cTree, 14);

  if (CheckTreeNode(cTree, node15, NULL, node11, node20, FALSE) == FALSE ||
      CheckTreeNode(cTree, node20, node15, node16, node25, TRUE) == FALSE ||
      CheckTreeNode(cTree, node16, node20, NULL, node17, FALSE) == FALSE ||
      CheckTreeNode(cTree, node17, node16, NULL, NULL, TRUE) == FALSE ||
      CheckTreeNode(cTree, node11, node15, node2, node14, TRUE) == FALSE)
  {
    hRes = E_FAIL;
    goto done;
  }

  hRes = S_OK;

done:
  cTree.RemoveAll();
  delete[] lpNodes;
  return hRes;
}

static HRESULT TestExample137CormenPage329CaseA()
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;

  if (CreateTree_Example133CormenPage314(cTree, &lpNodes) == FALSE)
    return E_OUTOFMEMORY;

  cTree.Remove(&(SearchTree(cTree, 12)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 17)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 19)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 22)->cTreeNode));
  cTree.Remove(&(SearchTree(cTree, 2)->cTreeNode));

  SIZE_T nIdx = cTree.GetCount();
  lpNodes[nIdx].nValue = 8;
  cTree.Insert(&(lpNodes[nIdx].cTreeNode), &InsertNode);

  cTree.Remove(&(SearchTree(cTree, 8)->cTreeNode));

  //CheckTreeNode(cTree, tree, null, 11, 20, null, FALSE);
  cTree.RemoveAll();
  delete[] lpNodes;

  return S_OK;
}

static HRESULT TestDuplicates(_In_ SIZE_T nCount)
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  MX::CRedBlackTreeNode *lpTreeNode;
  SIZE_T i;
  ULONG nCurrValue;

  lpNodes = MX_DEBUG_NEW CNode[nCount];
  if (lpNodes == NULL)
    return E_OUTOFMEMORY;

  for (i = 0; i < nCount; i++)
  {
    lpNodes[i].nValue = 1 + (ULONG)(rand() % 1000);
    cTree.Insert(&(lpNodes[i].cTreeNode), &InsertNode, TRUE);
  }

  lpTreeNode = cTree.GetFirst();
  nCurrValue = 0;
  for (i = 0; i < nCount; i++)
  {
    CNode *lpNode = CONTAINING_RECORD(lpTreeNode, CNode, cTreeNode);

    if (lpNode->nValue < nCurrValue)
    {
      cTree.RemoveAll();
      delete [] lpNodes;
      return E_FAIL;
    }

    lpTreeNode = lpTreeNode->GetNext();
    nCurrValue = lpNode->nValue;
  }

  if ((nCount & 1) != 0)
  {
    lpNodes[nCount - 1].cTreeNode.Remove();
  }
  for (i = 0; i < nCount / 2; i++)
  {
    lpNodes[i].cTreeNode.Remove();
    lpNodes[(nCount / 2) + i].cTreeNode.Remove();
  }
  delete[] lpNodes;

  //done
  return S_OK;
}

static HRESULT TestNonDuplicates(_In_ SIZE_T nCount)
{
  MX::CRedBlackTree cTree;
  CNode *lpNodes;
  MX::CRedBlackTreeNode *lpTreeNode;
  SIZE_T i;
  ULONG nCurrValue;

  lpNodes = MX_DEBUG_NEW CNode[nCount];
  if (lpNodes == NULL)
    return E_OUTOFMEMORY;
  for (i = 0; i < nCount; i++)
  {
    lpNodes[i].nValue = 1 + (ULONG)i;
  }
  for (i = 0; i < 30000; i++)
  {
    SIZE_T nIdx1 = (SIZE_T)(rand() % nCount);
    SIZE_T nIdx2 = (SIZE_T)(rand() % nCount);

    ULONG nTemp = lpNodes[nIdx1].nValue;
    lpNodes[nIdx1].nValue = lpNodes[nIdx2].nValue;
    lpNodes[nIdx2].nValue = nTemp;
  }
  for (i = 0; i < nCount; i++)
  {
    cTree.Insert(&(lpNodes[i].cTreeNode), &InsertNode, FALSE);
  }

  lpTreeNode = cTree.GetFirst();
  nCurrValue = 0;
  for (i = 0; i < nCount; i++)
  {
    CNode *lpNode = CONTAINING_RECORD(lpTreeNode, CNode, cTreeNode);

    if (lpNode->nValue <= nCurrValue)
    {
      cTree.RemoveAll();
      delete [] lpNodes;
      return E_FAIL;
    }

    lpTreeNode = lpTreeNode->GetNext();
    nCurrValue = lpNode->nValue;
  }

  if ((nCount & 1) != 0)
  {
    lpNodes[nCount - 1].cTreeNode.Remove();
  }
  for (i = 0; i < nCount / 2; i++)
  {
    lpNodes[i].cTreeNode.Remove();
    lpNodes[(nCount / 2) + i].cTreeNode.Remove();
  }
  delete[] lpNodes;

  //done
  return S_OK;
}

//-----------------------------------------------------------

static BOOL CreateTree(_In_ MX::CRedBlackTree &cTree, _Out_ CNode **lplpNodes, ...)
{
  va_list args;
  SIZE_T i, nCount;

  va_start(args, lplpNodes);
  for (nCount = 0; ; nCount++)
  {
    ULONG val = va_arg(args, ULONG);
    if (val == 0)
      break;
  }
  va_end(args);

  *lplpNodes = MX_DEBUG_NEW CNode[nCount + 16]; //extra nodes
  if ((*lplpNodes) == NULL)
    return FALSE;

  va_start(args, lplpNodes);
  for (i = 0; i < nCount ; i++)
  {
    (*lplpNodes)[i].nValue = va_arg(args, ULONG);

    cTree.Insert(&(((*lplpNodes)[i]).cTreeNode), &InsertNode);
  }
  va_end(args);

  return TRUE;
}

static CNode* SearchTree(_In_ MX::CRedBlackTree &cTree, _In_ ULONG nValue)
{
  MX::CRedBlackTreeNode *lpNode;

  lpNode = cTree.Find(&nValue, &SearchNode);
  return (lpNode != NULL) ? CONTAINING_RECORD(lpNode, CNode, cTreeNode) : NULL;
}

static BOOL CheckTreeNode(_In_ MX::CRedBlackTree &cTree, _In_ CNode *lpNode, _In_opt_ CNode *lpParent,
                          _In_opt_ CNode *lpLeft, _In_opt_ CNode *lpRight, _In_ BOOL bIsRed)
{
  if (lpNode->cTreeNode.GetParent() != ((lpParent != NULL) ? &(lpParent->cTreeNode) : NULL))
  {
    MX_ASSERT(FALSE);
    return FALSE;
  }
  if (lpNode->cTreeNode.GetLeft() != ((lpLeft != NULL) ? &(lpLeft->cTreeNode) : NULL))
  {
    MX_ASSERT(FALSE);
    return FALSE;
  }
  if (lpNode->cTreeNode.GetRight() != ((lpRight != NULL) ? &(lpRight->cTreeNode) : NULL))
  {
    MX_ASSERT(FALSE);
    return FALSE;
  }
  if (lpNode->cTreeNode.IsRed() != bIsRed)
  {
    MX_ASSERT(FALSE);
    return FALSE;
  }
  return TRUE;
}

//-----------------------------------------------------------

static int InsertNode(_In_opt_ LPVOID lpContext, _In_ MX::CRedBlackTreeNode *lpNode1,
                      _In_ MX::CRedBlackTreeNode *lpNode2)
{
  CNode *lpElem1 = CONTAINING_RECORD(lpNode1, CNode, cTreeNode);
  CNode *lpElem2 = CONTAINING_RECORD(lpNode2, CNode, cTreeNode);

  if (lpElem1->nValue < lpElem2->nValue)
    return -1;
  if (lpElem1->nValue > lpElem2->nValue)
    return 1;
  return 0;
}

static int SearchNode(_In_opt_ LPVOID lpContext, _In_ PULONG lpnValue, _In_ MX::CRedBlackTreeNode *lpNode)
{
  CNode *lpElem = CONTAINING_RECORD(lpNode, CNode, cTreeNode);

  if ((*lpnValue) < lpElem->nValue)
    return -1;
  if ((*lpnValue) > lpElem->nValue)
    return 1;
  return 0;
}

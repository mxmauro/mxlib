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

static int InsertNode(_In_opt_ LPVOID lpContext, _In_ MX::CRedBlackTreeNode *lpNode1,
                      _In_ MX::CRedBlackTreeNode *lpNode2);
static int SearchNode(_In_opt_ LPVOID lpContext, _In_ PULONG lpnValue, _In_ MX::CRedBlackTreeNode *lpNode);

//-----------------------------------------------------------

int TestRedBlackTree()
{
  CNode *lpNodes, *lpNode;
  MX::CRedBlackTree cTree;
  MX::CRedBlackTreeNode *lpTreeNode;
  SIZE_T i, nCount, nPass;
  ULONG nCurrValue;

  srand(::GetTickCount());

  for (nPass = 1; nPass <= 3; nPass++)
  {
    nCount = nPass * 10000;

    wprintf_s(L"Running RB-Tree test with duplicates (%lu elements)... ", nCount);

    lpNodes = MX_DEBUG_NEW CNode[nCount];
    if (lpNodes == NULL)
    {
err_nomem:
      wprintf_s(L"\nError: Not enough memory.\n");
      return (int)E_OUTOFMEMORY;
    }
    for (i = 0; i < nCount; i++)
    {
      lpNodes[i].nValue = 1 + (ULONG)(rand() % 1000);
      cTree.Insert(&(lpNodes[i].cTreeNode), &InsertNode, TRUE);
    }

    lpTreeNode = cTree.GetFirst();
    nCurrValue = 0;
    for (i = 0; i < nCount; i++)
    {
      lpNode = CONTAINING_RECORD(lpTreeNode, CNode, cTreeNode);

      if (lpNode->nValue < nCurrValue)
      {
        wprintf_s(L"Wrong sequence\n");
        break;
      }

      lpTreeNode = lpTreeNode->GetNext();
      nCurrValue = lpNode->nValue;
    }

    for (i = 0; i < nCount; i++)
    {
      lpNodes[i].cTreeNode.Remove();
    }
    delete[] lpNodes;

    wprintf_s(L"OK\n");

    //----

    wprintf_s(L"Running RB-Tree test with no duplicates (%lu elements)... ", nCount);

    lpNodes = MX_DEBUG_NEW CNode[nCount];
    if (lpNodes == NULL)
      goto err_nomem;
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
      lpNode = CONTAINING_RECORD(lpTreeNode, CNode, cTreeNode);

      if (lpNode->nValue <= nCurrValue)
      {
        wprintf_s(L"Wrong sequence\n");
        break;
      }

      lpTreeNode = lpTreeNode->GetNext();
      nCurrValue = lpNode->nValue;
    }

    for (i = 0; i < nCount; i++)
    {
      lpNodes[i].cTreeNode.Remove();
    }
    delete[] lpNodes;

    wprintf_s(L"OK\n");
  }

  //done
  return (int)S_OK;
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

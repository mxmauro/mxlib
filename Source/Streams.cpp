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
#include "..\Include\Streams.h"
#include "..\Include\Strings\Strings.h"
#include "..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

HRESULT CStream::WriteString(_In_ LPCSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr)
{
  CStringA cStrTempA;
  SIZE_T nWritten;
  HRESULT hRes;

  if (cStrTempA.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Write((LPSTR)cStrTempA, cStrTempA.GetLength(), nWritten);
  if (SUCCEEDED(hRes) && nWritten != cStrTempA.GetLength())
    hRes = MX_E_WriteFault;
  return hRes;
}

HRESULT CStream::WriteString(_In_ LPCWSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(_In_ LPCWSTR szFormatA, _In_ va_list argptr)
{
  CStringW cStrTempW;
  SIZE_T nWritten;
  HRESULT hRes;

  if (cStrTempW.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Write((LPWSTR)cStrTempW, cStrTempW.GetLength(), nWritten);
  if (SUCCEEDED(hRes) && nWritten != cStrTempW.GetLength())
    hRes = MX_E_WriteFault;
  return hRes;
}

HRESULT CStream::Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod)
{
  return E_NOTIMPL;
}

HRESULT CStream::CopyTo(_In_ CStream *lpTarget, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                        _In_opt_ ULONGLONG nStartOffset)
{
  MX::TAutoFreePtr<BYTE> aBuffer;
  BYTE aMemSafeBuf[1024], *lpTempBuf;
  SIZE_T nBufferSize, nThisBytesRead, nThisBytesWritten;
  HRESULT hRes;

  nBytesWritten = 0;
  if (lpTarget == NULL)
    return E_POINTER;
  nBufferSize = 65536;
  aBuffer.Attach((LPBYTE)MX_MALLOC(nBufferSize));
  if (aBuffer)
  {
    lpTempBuf = aBuffer.Get();
  }
  else
  {
    nBufferSize = sizeof(aMemSafeBuf);
    lpTempBuf = aMemSafeBuf;
  }
  hRes = S_OK;
  while (nBytes > 0)
  {
    hRes = Read(lpTempBuf, (nBytes > nBufferSize) ? nBufferSize : nBytes, nThisBytesRead, nStartOffset);
    if (FAILED(hRes))
      break;
    if (nThisBytesRead == 0)
      break;
    nBytes -= nThisBytesRead;
    if (nStartOffset != ULONGLONG_MAX)
      nStartOffset += (ULONGLONG)nThisBytesRead;

    hRes = lpTarget->Write(lpTempBuf, nThisBytesRead, nThisBytesWritten);
    if (FAILED(hRes))
      break;
    nBytesWritten += nThisBytesWritten;
    if (nThisBytesWritten != nThisBytesRead)
    {
      hRes = MX_E_WriteFault;
      break;
    }
  }
  //done
  return (hRes == MX_E_EndOfFileReached) ? S_OK : hRes;
}

VOID CStream::SetChainedStream(_In_opt_ CStream *lpStream)
{
  if (lpNextStream != NULL)
    lpNextStream->Release();
  lpNextStream = lpStream;
  if (lpNextStream != NULL)
    lpNextStream->AddRef();
  return;
}

CStream* CStream::GetChainedStream() const
{
  if (lpNextStream != NULL)
    lpNextStream->AddRef();
  return lpNextStream;
}


} //namespace MX

/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#include "..\Include\FileStream.h"

//-----------------------------------------------------------

namespace MX {

CFileStream::CFileStream() : CStream()
{
  return;
}

CFileStream::~CFileStream()
{
  return;
}

HRESULT CFileStream::Create(__in LPCWSTR szFileNameW, __in_opt DWORD dwDesiredAccess, __in_opt DWORD dwShareMode,
                            __in_opt DWORD dwCreationDisposition, __in_opt DWORD dwFlagsAndAttributes,
                            __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  HANDLE h;
  NTSTATUS nNtStatus;

  Close();
  if ((dwDesiredAccess & (GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE|GENERIC_ALL)) == 0)
    dwDesiredAccess |= SYNCHRONIZE;
  nNtStatus = ::MxCreateFile(&h, szFileNameW, dwDesiredAccess, dwShareMode, dwCreationDisposition,
                             dwFlagsAndAttributes, lpSecurityAttributes);
  if (!NT_SUCCESS(nNtStatus))
    return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  //done
  cFileH.Attach(h);
  return S_OK;
}

VOID CFileStream::Close()
{
  cFileH.Close();
  return;
}

HRESULT CFileStream::Read(__out LPVOID lpDest, __in SIZE_T nBytes, __out SIZE_T &nReaded)
{
  MX_IO_STATUS_BLOCK sIoStatus;
  NTSTATUS nNtStatus;

  nReaded = 0;
  if (!cFileH)
    return MX_E_NotReady;
  if (lpDest == NULL)
    return E_POINTER;
  if (nBytes > (SIZE_T)0xFFFFFFFFUL)
    nBytes = (SIZE_T)0xFFFFFFFFUL;
  nNtStatus = ::MxNtReadFile(cFileH, NULL, NULL, NULL, &sIoStatus, lpDest, (ULONG)nBytes, NULL, NULL);
  if (nNtStatus == STATUS_PENDING)
  {
    nNtStatus = ::MxNtWaitForSingleObject(cFileH, FALSE, NULL);
    if (NT_SUCCESS(nNtStatus))
      nNtStatus = sIoStatus.Status;
  }
  if (nNtStatus == STATUS_END_OF_FILE)
    return MX_E_EndOfFileReached;
  if (!NT_SUCCESS(nNtStatus))
    return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  //done
  nReaded = (SIZE_T)(sIoStatus.Information);
  return S_OK;
}

HRESULT CFileStream::Write(__in LPCVOID lpSrc, __in SIZE_T nBytes, __out SIZE_T &nWritten)
{
  MX_IO_STATUS_BLOCK sIoStatus;
  ULONG nToWrite;
  NTSTATUS nNtStatus;

  nWritten = 0;
  if (!cFileH)
    return MX_E_NotReady;
  if (nBytes == 0)
    return S_OK;
  if (lpSrc == NULL)
    return E_POINTER;
  nNtStatus = STATUS_SUCCESS;
  while (nBytes > 0)
  {
    MemSet(&sIoStatus, 0, sizeof(sIoStatus));
    nToWrite = (nBytes < (SIZE_T)0xFFFFFFFFUL) ? (ULONG)nBytes : 0xFFFFFFFFUL;
    nNtStatus = ::MxNtWriteFile(cFileH, NULL, NULL, NULL, &sIoStatus, (PVOID)lpSrc, nToWrite, NULL, NULL);
    if (nNtStatus == STATUS_PENDING)
    {
      nNtStatus = ::MxNtWaitForSingleObject(cFileH, FALSE, NULL);
      if (NT_SUCCESS(nNtStatus))
        nNtStatus = sIoStatus.Status;
    }
    if (!NT_SUCCESS(nNtStatus) || nToWrite != sIoStatus.Information)
      break;
    lpSrc = (LPBYTE)lpSrc + nToWrite;
    nBytes -= (SIZE_T)nToWrite;
  }
  if (!NT_SUCCESS(nNtStatus))
    return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  //done
  return S_OK;
}

HRESULT CFileStream::Seek(__in ULONGLONG nPosition, __in_opt eSeekMethod nMethod)
{
  MX_FILE_POSITION_INFORMATION sFilePosInfo;
  MX_FILE_STANDARD_INFORMATION sFileStdInfo;
  MX_IO_STATUS_BLOCK sIoStatus;
  NTSTATUS nNtStatus;

  if (!cFileH)
    return MX_E_NotReady;
  switch (nMethod)
  {
    case SeekStart:
      sFilePosInfo.CurrentByteOffset.QuadPart = (LONGLONG)nPosition;
      break;

    case SeekCurrent:
      nNtStatus = ::MxNtQueryInformationFile(cFileH, &sIoStatus, &sFilePosInfo, (ULONG)sizeof(sFilePosInfo),
                                             MxFilePositionInformation);
      if (!NT_SUCCESS(nNtStatus))
        return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
      sFilePosInfo.CurrentByteOffset.QuadPart += (LONGLONG)nPosition;
      if (sFilePosInfo.CurrentByteOffset.QuadPart < 0i64)
        return E_FAIL;
      break;

    case SeekEnd:
      nNtStatus = ::MxNtQueryInformationFile(cFileH, &sIoStatus, &sFileStdInfo, (ULONG)sizeof(sFileStdInfo),
                                             MxFileStandardInformation);
      if (!NT_SUCCESS(nNtStatus))
        return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
      if (nPosition > (ULONGLONG)sFileStdInfo.EndOfFile.QuadPart)
        return E_FAIL;
      sFilePosInfo.CurrentByteOffset.QuadPart = sFileStdInfo.EndOfFile.QuadPart - (LONGLONG)nPosition;
      break;

    default:
      return E_INVALIDARG;
  }
  nNtStatus = ::MxNtSetInformationFile(cFileH, &sIoStatus, &sFilePosInfo, (ULONG)sizeof(sFilePosInfo),
                                        MxFilePositionInformation);
  if (!NT_SUCCESS(nNtStatus))
    return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  //done
  return S_OK;
}

ULONGLONG CFileStream::GetLength() const
{
  MX_FILE_STANDARD_INFORMATION sFileStdInfo;
  MX_IO_STATUS_BLOCK sIoStatus;
  NTSTATUS nNtStatus;

  if (!cFileH)
    return 0ui64;
  nNtStatus = ::MxNtQueryInformationFile(cFileH, &sIoStatus, &sFileStdInfo, (ULONG)sizeof(sFileStdInfo),
                                         MxFileStandardInformation);
  if (!NT_SUCCESS(nNtStatus))
    return 0ui64;
  return (ULONGLONG)(sFileStdInfo.EndOfFile.QuadPart);
}

CFileStream* CFileStream::Clone()
{
  return NULL;
}

} //namespace MX

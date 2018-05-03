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
#ifdef _DEBUG
  nCurrentOffset = 0ui64;
#endif //_DEBUG
  return;
}

CFileStream::~CFileStream()
{
  return;
}

HRESULT CFileStream::Create(_In_ LPCWSTR szFileNameW, _In_opt_ DWORD dwDesiredAccess, _In_opt_ DWORD dwShareMode,
                            _In_opt_ DWORD dwCreationDisposition, _In_opt_ DWORD dwFlagsAndAttributes,
                            _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  HANDLE h;
  NTSTATUS nNtStatus;

  Close();
  if ((dwDesiredAccess & (GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL)) == 0)
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
#ifdef _DEBUG
  nCurrentOffset = 0ui64;
#endif //_DEBUG
  return;
}

HRESULT CFileStream::Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesRead,
                          _In_opt_ ULONGLONG nStartOffset)
{
  MX_IO_STATUS_BLOCK sIoStatus;
  ULARGE_INTEGER liOffset;
  NTSTATUS nNtStatus;

  nBytesRead = 0;
  if (!cFileH)
    return MX_E_NotReady;
  if (lpDest == NULL)
    return E_POINTER;
  if (nBytes > (SIZE_T)0xFFFFFFFFUL)
    nBytes = (SIZE_T)0xFFFFFFFFUL;
  liOffset.QuadPart = nStartOffset;
  nNtStatus = ::MxNtReadFile(cFileH, NULL, NULL, NULL, &sIoStatus, lpDest, (ULONG)nBytes,
                             (nStartOffset != ULONGLONG_MAX) ? (PLARGE_INTEGER)&liOffset : NULL, NULL);
  if (nNtStatus == STATUS_PENDING)
  {
    nNtStatus = ::MxNtWaitForSingleObject(cFileH, FALSE, NULL);
    if (NT_SUCCESS(nNtStatus))
      nNtStatus = sIoStatus.Status;
  }
  if (nNtStatus == STATUS_END_OF_FILE)
    return MX_E_EndOfFileReached;
  //advance pointer and read (latter can be less than 'nBytes'
  nBytesRead = (SIZE_T)(sIoStatus.Information);
#ifdef _DEBUG
  if (nStartOffset == ULONGLONG_MAX)
    nCurrentOffset += (ULONGLONG)(sIoStatus.Information);
#endif //_DEBUG
  //done
  if (!NT_SUCCESS(nNtStatus))
    return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  return S_OK;
}

HRESULT CFileStream::Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                           _In_opt_ ULONGLONG nStartOffset)
{
  MX_IO_STATUS_BLOCK sIoStatus;
  ULARGE_INTEGER liOffset;
  ULONG nToWrite;
  NTSTATUS nNtStatus;

  nBytesWritten = 0;
  if (!cFileH)
    return MX_E_NotReady;
  if (nBytes == 0)
    return S_OK;
  if (lpSrc == NULL)
    return E_POINTER;
  nNtStatus = STATUS_SUCCESS;
  liOffset.QuadPart = nStartOffset;
  while (nBytes > 0)
  {
    MemSet(&sIoStatus, 0, sizeof(sIoStatus));
    nToWrite = (nBytes < 65536) ? (ULONG)nBytes : 65536;
    nNtStatus = ::MxNtWriteFile(cFileH, NULL, NULL, NULL, &sIoStatus, (PVOID)lpSrc, nToWrite,
                                (nStartOffset != ULONGLONG_MAX) ? (PLARGE_INTEGER)&liOffset : NULL, NULL);
    if (nNtStatus == STATUS_PENDING)
    {
      nNtStatus = ::MxNtWaitForSingleObject(cFileH, FALSE, NULL);
      if (NT_SUCCESS(nNtStatus))
        nNtStatus = sIoStatus.Status;
    }
    nBytesWritten += (ULONGLONG)(sIoStatus.Information);
    if (nStartOffset != ULONGLONG_MAX)
      liOffset.QuadPart += (ULONGLONG)(sIoStatus.Information);
#ifdef _DEBUG
    if (nStartOffset == ULONGLONG_MAX)
      nCurrentOffset += (ULONGLONG)(sIoStatus.Information);
#endif //_DEBUG
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
    if ((ULONG_PTR)nToWrite != sIoStatus.Information)
      break; //stop if less bytes than requested were written
    lpSrc = (LPBYTE)lpSrc + nToWrite;
    nBytes -= (SIZE_T)nToWrite;
  }
  //done
  return S_OK;
}

HRESULT CFileStream::Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod)
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
#ifdef _DEBUG
  nCurrentOffset = (ULONGLONG)(sFilePosInfo.CurrentByteOffset.QuadPart);
#endif //_DEBUG
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

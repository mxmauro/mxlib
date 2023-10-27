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
#include "..\..\Include\ZipLib\ZipFile.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\FileStream.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\DateTime\DateTime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
  #include "MiniZipSource\mz.h"
  #include "MiniZipSource\mz_os.h"
  #include "MiniZipSource\mz_zip.h"
  #include "MiniZipSource\mz_strm_os.h"
}; //extern "C"

//-----------------------------------------------------------

typedef struct _ZIPCREATOR_DATA {
  void *zip_stream;
  void *zip_handle;
  BOOL bFileIsOpen;
  BYTE aTempBuffer[32768];
} ZIPCREATOR_DATA, *LPZIPCREATOR_DATA;

#define zc_data ((LPZIPCREATOR_DATA)lpData)

//-----------------------------------------------------------

static HRESULT mzError_2_HRESULT(_In_ int32_t err);

//-----------------------------------------------------------

namespace MX {

CZipFile::CZipFile() : CBaseMemObj(), CNonCopyableObj()
{
  lpData = MX_MALLOC(sizeof(ZIPCREATOR_DATA));
  if (lpData != NULL)
  {
    ::MxMemSet(lpData, 0, sizeof(ZIPCREATOR_DATA));
  }
  return;
}

CZipFile::~CZipFile()
{
  CloseArchive();
  MX_FREE(lpData);
  return;
}

HRESULT CZipFile::CreateArchive(_In_z_ LPCWSTR szFileNameW)
{
  return CreateOrOpenArchive(szFileNameW, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);
}

HRESULT CZipFile::OpenArchive(_In_z_ LPCWSTR szFileNameW)
{
  return CreateOrOpenArchive(szFileNameW, MZ_OPEN_MODE_EXISTING | MZ_OPEN_MODE_READ);
}

VOID CZipFile::CloseArchive()
{
  if (lpData != NULL)
  {
    CloseFile();

    if (zc_data->zip_handle != NULL)
    {
      mz_zip_close(zc_data->zip_handle);
      mz_zip_delete(&(zc_data->zip_handle));
      //zc_data->zip_handle = NULL;
    }
    if (zc_data->zip_stream != NULL)
    {
      mz_stream_os_close(zc_data->zip_stream);
      mz_stream_os_delete(&(zc_data->zip_stream));
      //zc_data->zip_stream = NULL;
    }
  }
  return;
}

HRESULT CZipFile::AddFile(_In_z_ LPCWSTR szFileNameInZipW, _In_z_ LPCWSTR szSrcFileNameW,
                          _In_opt_z_ LPCWSTR szPasswordW)
{
  static const BYTE aSharingAccess[4] = {
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE,
    FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SHARE_READ
  };
  TAutoRefCounted<CFileStream> cStream;
  HRESULT hRes;

  cStream.Attach(MX_DEBUG_NEW CFileStream());
  if (!cStream)
    return E_OUTOFMEMORY;
  for (SIZE_T i = 0; i < 4; i++)
  {
    hRes = cStream->Create(szSrcFileNameW, GENERIC_READ, (DWORD)aSharingAccess[i]);
    if (hRes != HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) && hRes != HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION))
      break;
  }
  if (SUCCEEDED(hRes))
  {
    hRes = AddStream(szFileNameInZipW, cStream, szPasswordW);
  }
  return hRes;
}

HRESULT CZipFile::AddStream(_In_z_ LPCWSTR szFileNameInZipW, _In_ MX::CStream *lpStream, _In_opt_z_ LPCWSTR szPasswordW)
{
  mz_zip_file file_info;
  CStringA cStrFileNameA_Utf8;
  CSecureStringA cStrPasswordA_Utf8;
  CDateTime cDt;
  LONGLONG t;
  HRESULT hRes;

  if (szFileNameInZipW == NULL || lpStream == NULL)
    return E_POINTER;
  if (*szFileNameInZipW == 0)
    return E_INVALIDARG;

  if (zc_data->zip_handle == NULL)
    return MX_E_NotReady;
  if (zc_data->bFileIsOpen != FALSE)
    return MX_E_NotReady;

  hRes = MX::Utf8_Encode(cStrFileNameA_Utf8, szFileNameInZipW);
  if (FAILED(hRes))
    return hRes;

  if (szPasswordW != NULL && *szPasswordW != 0)
  {
    hRes = MX::Utf8_Encode(cStrPasswordA_Utf8, szPasswordW);
    if (FAILED(hRes))
      return hRes;
  }

  hRes = lpStream->Seek(0, MX::CStream::eSeekMethod::Start);
  if (FAILED(hRes))
    return hRes;

  ::MxMemSet(&file_info, 0, sizeof(file_info));
  file_info.version_madeby = MZ_VERSION_MADEBY;
  file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
  file_info.filename = (LPCSTR)cStrFileNameA_Utf8;
  file_info.uncompressed_size = lpStream->GetLength();
  file_info.external_fa = (uint32_t)FILE_ATTRIBUTE_NORMAL;

  if (lpStream->GetHandle() != NULL)
  {
    MX_FILE_BASIC_INFORMATION sBasicInfo;
    MX_IO_STATUS_BLOCK sIoStatus;
    FILETIME sFt;
    LONG nNtStatus;

    ::MxMemSet(&sIoStatus, 0, sizeof(sIoStatus));
    ::MxMemSet(&sBasicInfo, 0, sizeof(sBasicInfo));
    nNtStatus = ::MxNtQueryInformationFile(lpStream->GetHandle(), &sIoStatus, &sBasicInfo,
                                           (ULONG)sizeof(sBasicInfo), MxFileBasicInformation);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));

    file_info.external_fa = (uint32_t)(sBasicInfo.FileAttributes);

    sFt.dwLowDateTime = sBasicInfo.CreationTime.LowPart;
    sFt.dwHighDateTime = sBasicInfo.CreationTime.HighPart;
    hRes = cDt.SetFromFileTime(sFt);
    if (SUCCEEDED(hRes))
    {
      hRes = cDt.GetUnixTime(&t);
      if (SUCCEEDED(hRes))
        file_info.creation_date = (time_t)t;
    }
    if (FAILED(hRes))
      return hRes;

    sFt.dwLowDateTime = sBasicInfo.LastWriteTime.LowPart;
    sFt.dwHighDateTime = sBasicInfo.LastWriteTime.HighPart;
    hRes = cDt.SetFromFileTime(sFt);
    if (SUCCEEDED(hRes))
    {
      hRes = cDt.GetUnixTime(&t);
      if (SUCCEEDED(hRes))
        file_info.modified_date = (time_t)t;
    }
    if (FAILED(hRes))
      return hRes;

    sFt.dwLowDateTime = sBasicInfo.LastAccessTime.LowPart;
    sFt.dwHighDateTime = sBasicInfo.LastAccessTime.HighPart;
    hRes = cDt.SetFromFileTime(sFt);
    if (SUCCEEDED(hRes))
    {
      hRes = cDt.GetUnixTime(&t);
      if (SUCCEEDED(hRes))
        file_info.accessed_date = (time_t)t;
    }
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    hRes = cDt.SetFromNow(FALSE);
    if (SUCCEEDED(hRes))
    {
      hRes = cDt.GetUnixTime(&t);
      if (SUCCEEDED(hRes))
        file_info.creation_date = file_info.modified_date = file_info.accessed_date = (time_t)t;
    }
    if (FAILED(hRes))
      return hRes;
  }

  hRes = mzError_2_HRESULT(mz_zip_entry_write_open(zc_data->zip_handle, &file_info, MZ_COMPRESS_LEVEL_BEST, 0,
                                                   (cStrPasswordA_Utf8.IsEmpty() != FALSE)
                                                   ? NULL : (LPCSTR)cStrPasswordA_Utf8));
  if (SUCCEEDED(hRes))
  {
    ULONGLONG ullToRead;

    ullToRead = file_info.uncompressed_size;
    while (SUCCEEDED(hRes) && ullToRead > 0ui64)
    {
      SIZE_T nToRead, nBytesRead;
      int32_t written;

      nToRead = (ullToRead > (ULONGLONG)sizeof(zc_data->aTempBuffer)) ? sizeof(zc_data->aTempBuffer)
                                                                      : (SIZE_T)ullToRead;
      hRes = lpStream->Read(zc_data->aTempBuffer, nToRead, nBytesRead);
      if (FAILED(hRes))
        break;
      if (nBytesRead != nToRead)
      {
        hRes = MX_E_ReadFault;
        break;
      }

      written = mz_zip_entry_write(zc_data->zip_handle, zc_data->aTempBuffer, (int32_t)nBytesRead);
      if (written < MZ_OK)
      {
        hRes = mzError_2_HRESULT(written);
        break;
      }

      ullToRead -= (ULONGLONG)(uint32_t)written;
    }

    if (SUCCEEDED(hRes))
      hRes = mzError_2_HRESULT(mz_zip_entry_close(zc_data->zip_handle));
    else
      mz_zip_entry_close(zc_data->zip_handle);
  }

  //done
  return hRes;
}

HRESULT CZipFile::OpenFile(_In_z_ LPCWSTR szFileNameInZipW, _In_opt_z_ LPCWSTR szPasswordW)
{
  CStringA cStrFileNameA_Utf8;
  CSecureStringA cStrPasswordA_Utf8;
  HRESULT hRes;

  if (szFileNameInZipW == NULL)
    return E_POINTER;
  if (*szFileNameInZipW == 0)
    return E_INVALIDARG;

  if (zc_data->zip_handle == NULL)
    return MX_E_NotReady;

  CloseFile();

  hRes = MX::Utf8_Encode(cStrFileNameA_Utf8, szFileNameInZipW);
  if (FAILED(hRes))
    return hRes;

  if (szPasswordW != NULL && *szPasswordW != 0)
  {
    hRes = MX::Utf8_Encode(cStrPasswordA_Utf8, szPasswordW);
    if (FAILED(hRes))
      return hRes;
  }

  if (mz_zip_locate_entry(zc_data->zip_handle, (LPCSTR)cStrFileNameA_Utf8, 1) != MZ_OK)
    return MX_E_FileNotFound;

  hRes = mzError_2_HRESULT(mz_zip_entry_read_open(zc_data->zip_handle, 0,
                                                  ((cStrPasswordA_Utf8.IsEmpty() != FALSE)
                                                   ? NULL : (LPCSTR)cStrPasswordA_Utf8)));
  if (FAILED(hRes))
    return hRes;

  //done
  zc_data->bFileIsOpen = TRUE;
  return S_OK;
}

VOID CZipFile::CloseFile()
{
  if (lpData != NULL)
  {
    if (zc_data->bFileIsOpen != FALSE)
    {
      mz_zip_entry_close(zc_data->zip_handle);
      zc_data->bFileIsOpen = FALSE;
    }
  }
  return;
}

HRESULT CZipFile::GetFileInfo(_Out_opt_ PULONGLONG lpnFileSize, _Out_opt_ LPDWORD lpdwFileAttributes,
                              _Out_opt_ LPSYSTEMTIME lpFileTime)
{
  mz_zip_file *file_info;
  HRESULT hRes;

  if (lpnFileSize != NULL)
    *lpnFileSize = 0ui64;
  if (lpdwFileAttributes != NULL)
    *lpdwFileAttributes = 0;
  if (lpFileTime != NULL)
    ::MxMemSet(lpFileTime, 0, sizeof(SYSTEMTIME));

  if (lpData == NULL || zc_data->bFileIsOpen == FALSE)
    return MX_E_NotReady;

  hRes = mzError_2_HRESULT(mz_zip_entry_get_info(zc_data->zip_handle, &file_info));
  if (FAILED(hRes))
    return hRes;

  if (lpnFileSize != NULL)
    *lpnFileSize = (ULONGLONG)(file_info->uncompressed_size);

  if (lpdwFileAttributes != NULL)
    *lpdwFileAttributes = (DWORD)(file_info->external_fa);

  if (lpFileTime != NULL)
  {
    CDateTime cDt;

    if (file_info->modified_date != 0)
    {
      hRes = cDt.SetFromUnixTime((LONGLONG)(file_info->modified_date));
    }
    else if (file_info->creation_date != 0)
    {
      hRes = cDt.SetFromUnixTime((LONGLONG)(file_info->modified_date));
    }
    else
    {
      hRes = cDt.SetFromNow(FALSE);
    }
    if (SUCCEEDED(hRes))
      cDt.GetSystemTime(*lpFileTime);
    else
      ::GetSystemTime(lpFileTime);
  }

  //done
  return S_OK;
}

HRESULT CZipFile::Read(_Out_writes_bytes_to_opt_(nToRead, *lpnRead) LPVOID lpDest, _In_ SIZE_T nToRead,
                       _Out_opt_ SIZE_T *lpnRead)
{
  BOOL bFirstRead;

  if (lpnRead != NULL)
    *lpnRead = 0;
  if (lpDest == NULL && nToRead > 0)
    return E_POINTER;

  if (lpData == NULL || zc_data->bFileIsOpen == FALSE)
    return MX_E_NotReady;

  bFirstRead = TRUE;
  while (nToRead > 0)
  {
    int32_t read, nToReadThisRound;

    nToReadThisRound = (nToRead > 65536) ? 65536 : (unsigned int)nToRead;

    read = mz_zip_entry_read(zc_data->zip_handle, lpDest, nToReadThisRound);
    if (read < MZ_OK)
      return mzError_2_HRESULT(read);
    if (read == 0 && bFirstRead != FALSE)
      return MX_E_EndOfFileReached;

    bFirstRead = FALSE;
    if (lpnRead != NULL)
      *lpnRead += (SIZE_T)read;

    if (read < nToReadThisRound)
      break;

    lpDest = (LPBYTE)lpDest + (SIZE_T)read;
    nToRead -= (SIZE_T)read;
  }

  //done
  return S_OK;
}

HRESULT CZipFile::Read(_In_ CStream *lpStream, _In_ SIZE_T nToRead, _Out_opt_ SIZE_T *lpnRead)
{
  BOOL bFirstRead;

  if (lpnRead != NULL)
    *lpnRead = 0;
  if (lpStream == NULL && nToRead > 0)
    return E_POINTER;

  if (lpData == NULL || zc_data->bFileIsOpen == FALSE)
    return MX_E_NotReady;

  bFirstRead = TRUE;
  while (nToRead > 0)
  {
    int32_t read, nToReadThisRound;
    SIZE_T nBytesWritten;
    HRESULT hRes;

    nToReadThisRound = (nToRead > sizeof(zc_data->aTempBuffer)) ? (unsigned int)sizeof(zc_data->aTempBuffer)
                                                                : (unsigned int)nToRead;

    read = mz_zip_entry_read(zc_data->zip_handle, zc_data->aTempBuffer, nToReadThisRound);
    if (read < MZ_OK)
      return mzError_2_HRESULT(read);
    if (read == 0 && bFirstRead != FALSE)
      return MX_E_EndOfFileReached;

    hRes = lpStream->Write(zc_data->aTempBuffer, nToReadThisRound, nBytesWritten);
    if (FAILED(hRes))
      return hRes;

    bFirstRead = FALSE;
    if (lpnRead != NULL)
      *lpnRead += (SIZE_T)read;

    if (read < nToReadThisRound)
      break;

    nToRead -= (SIZE_T)read;
  }

  //done
  return S_OK;
}

HRESULT CZipFile::CreateOrOpenArchive(_In_z_ LPCWSTR szFileNameW, _In_ int mode)
{
  CStringA cStrUtf8FileNameA;
  HRESULT hRes;

  if (szFileNameW == NULL)
    return E_POINTER;
  if (*szFileNameW == 0)
    return E_INVALIDARG;

  if (lpData == NULL)
    return E_OUTOFMEMORY;

  CloseArchive();

  hRes = Utf8_Encode(cStrUtf8FileNameA, szFileNameW);
  if (FAILED(hRes))
    return hRes;

  zc_data->zip_stream = mz_stream_os_create();
  if (mz_stream_os_open(zc_data->zip_stream, (LPCSTR)cStrUtf8FileNameA, (int32_t)mode) != MZ_OK)
  {
    hRes = MX_HRESULT_FROM_WIN32(mz_stream_os_error(zc_data->zip_stream));
    CloseArchive();
    return hRes;
  }

  zc_data->zip_handle = mz_zip_create();
  if (zc_data->zip_handle == NULL)
  {
    CloseArchive();
    return E_OUTOFMEMORY;
  }

  hRes = mzError_2_HRESULT(mz_zip_open(zc_data->zip_handle, zc_data->zip_stream,
                           (int32_t)(mode & (MZ_OPEN_MODE_READ | MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_APPEND))));
  if (FAILED(hRes))
  {
    CloseArchive();
    return hRes;
  }

  //done
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT mzError_2_HRESULT(_In_ int32_t err)
{
  switch (err)
  {
    case MZ_OK:
      return S_OK;

    case MZ_STREAM_ERROR:
    case MZ_DATA_ERROR:
    case MZ_VERSION_ERROR:
    case MZ_CRC_ERROR:
      return MX_E_InvalidData;

    case MZ_MEM_ERROR:
    case MZ_BUF_ERROR:
      return E_OUTOFMEMORY;

    case MZ_END_OF_LIST:
    case MZ_END_OF_STREAM:
      return MX_E_EndOfFileReached;

    case MZ_PARAM_ERROR:
      return E_INVALIDARG;

    case MZ_EXIST_ERROR:
      return MX_E_AlreadyExists;

      /*
    case MZ_FORMAT_ERROR:
    case MZ_CRYPT_ERROR:
    case MZ_PASSWORD_ERROR:
    case MZ_SUPPORT_ERROR:
    case MZ_HASH_ERROR:

    case MZ_OPEN_ERROR:
    case MZ_CLOSE_ERROR:

    case MZ_SIGN_ERROR:
    case MZ_SYMLINK_ERROR:
      */

    case MZ_SEEK_ERROR:
    case MZ_TELL_ERROR:
    case MZ_READ_ERROR:
      return MX_E_ReadFault;
    case MZ_WRITE_ERROR:
      return MX_E_WriteFault;
  }
  return E_FAIL;
}

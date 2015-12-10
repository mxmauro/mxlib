/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include "..\..\Include\DateTime\DateTime.h"
#include <float.h>
#include <limits.h>

//-----------------------------------------------------------

//NOTE: In PPC is compiled with /Gy to prevent LNK1166

//-----------------------------------------------------------

static LONGLONG CalculateTicks(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds,
                               __in int nMilliSeconds, __out_opt LPBOOL lpbOverflow=NULL);
static BOOL myStrTol(__out int &nValue, __inout_z LPCSTR &sA, __inout SIZE_T &nLen);
static BOOL myWcsTol(__out int &nValue, __inout_z LPCWSTR &sW, __inout SIZE_T &nLen);

//-----------------------------------------------------------

namespace MX {


CTimeSpan::CTimeSpan(__in const CTimeSpan& cSrc) : CBaseMemObj()
{
  nTicks = cSrc.nTicks;
  return;
}

CTimeSpan::CTimeSpan(__in_opt LONGLONG nValue) : CBaseMemObj()
{
  nTicks = nValue;
  return;
}

CTimeSpan::CTimeSpan(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds,
                     __in_opt int nMilliSeconds) : CBaseMemObj()
{
  nTicks = CalculateTicks(nDays, nHours, nMinutes, nSeconds, nMilliSeconds);
  return;
}

CTimeSpan::CTimeSpan(__in int nHours, __in int nMinutes, __in int nSeconds) : CBaseMemObj()
{
  nTicks = CalculateTicks(0, nHours, nMinutes, nSeconds, 0);
  return;
}

int CTimeSpan::GetDays() const
{
  return (int)(nTicks / MX_DATETIME_TICKS_PER_DAY);
}

int CTimeSpan::GetHours() const
{
  return (int)((nTicks % MX_DATETIME_TICKS_PER_DAY) / MX_DATETIME_TICKS_PER_HOUR);
}

int CTimeSpan::GetMinutes() const
{
  return (int)((nTicks % MX_DATETIME_TICKS_PER_HOUR) / MX_DATETIME_TICKS_PER_MINUTE);
}

int CTimeSpan::GetSeconds() const
{
  return (int)((nTicks % MX_DATETIME_TICKS_PER_MINUTE) / MX_DATETIME_TICKS_PER_SECOND);
}

int CTimeSpan::GetMilliSeconds() const
{
  return (int)((nTicks % MX_DATETIME_TICKS_PER_SECOND) / MX_DATETIME_TICKS_PER_MILLISECOND);
}

LONGLONG CTimeSpan::GetDuration() const
{
  if (nTicks == (-0x7FFFFFFFFFFFFFFFi64 - 1i64))
    return 0;
  return (nTicks >= 0) ? nTicks : (-nTicks);
}

double CTimeSpan::GetTotalDays() const
{
  return (double)nTicks / (double)MX_DATETIME_TICKS_PER_DAY;
}

double CTimeSpan::GetTotalHours() const
{
  return (double)nTicks / (double)MX_DATETIME_TICKS_PER_HOUR;
}

double CTimeSpan::GetTotalMinutes() const
{
  return (double)nTicks / (double)MX_DATETIME_TICKS_PER_MINUTE;
}

double CTimeSpan::GetTotalSeconds() const
{
  return (double)nTicks / (double)MX_DATETIME_TICKS_PER_SECOND;
}

double CTimeSpan::GetTotalMilliSeconds() const
{
  return (double)nTicks / (double)MX_DATETIME_TICKS_PER_MILLISECOND;
}

HRESULT CTimeSpan::Add(__in LONGLONG nValue)
{
  if (nTicks >= 0)
  {
    if (nValue >= 0 && (nTicks+nValue) < nTicks)
      return MX_E_ArithmeticOverflow;
  }
  else
  {
    if (nValue < 0 && (nTicks+nValue) > nTicks)
      return MX_E_ArithmeticOverflow;
  }
  nTicks += nValue;
  return S_OK;
}

HRESULT CTimeSpan::Add(__in CTimeSpan &cTs)
{
  return Add(cTs.nTicks);
}

HRESULT CTimeSpan::Sub(__in LONGLONG nValue)
{
  return Add(-nValue);
}

HRESULT CTimeSpan::Sub(__in CTimeSpan &cTs)
{
  return Sub(cTs.nTicks);
}

HRESULT CTimeSpan::SetFrom(__in double nValue, __in LONGLONG nTickMultiplicator)
{
  LONGLONG nNewTick;
  int nClass;

  nClass = _fpclass((double)nValue);
  if (nClass==_FPCLASS_SNAN || nClass==_FPCLASS_QNAN ||
      nClass==_FPCLASS_NINF || nClass==_FPCLASS_PINF)
    return E_INVALIDARG;
  nValue = nValue * (double)(nTickMultiplicator / MX_DATETIME_TICKS_PER_MILLISECOND);
  nNewTick = (LONGLONG)nValue;
  nNewTick *= MX_DATETIME_TICKS_PER_MILLISECOND;
  nTicks = nNewTick;
  return S_OK;
}

HRESULT CTimeSpan::SetFromDays(__in double nValue)
{
  return SetFrom(nValue, MX_DATETIME_TICKS_PER_DAY);
}

HRESULT CTimeSpan::SetFromHours(__in double nValue)
{
  return SetFrom(nValue, MX_DATETIME_TICKS_PER_HOUR);
}

HRESULT CTimeSpan::SetFromMinutes(__in double nValue)
{
  return SetFrom(nValue, MX_DATETIME_TICKS_PER_MINUTE);
}

HRESULT CTimeSpan::SetFromSeconds(__in double nValue)
{
  return SetFrom(nValue, MX_DATETIME_TICKS_PER_SECOND);
}

HRESULT CTimeSpan::SetFromMilliSeconds(__in double nValue)
{
  return SetFrom(nValue, MX_DATETIME_TICKS_PER_MILLISECOND);
}

HRESULT CTimeSpan::SetFromTicks(__in LONGLONG nValue)
{
  nTicks = nValue;
  return S_OK;
}

HRESULT CTimeSpan::SetFrom(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds,
                           __in_opt int nMilliSeconds)
{
  BOOL bOverflow;
  LONGLONG nValue;

  nValue = CalculateTicks(nDays, nHours, nMinutes, nSeconds, nMilliSeconds, &bOverflow);
  if (bOverflow != FALSE)
    return E_INVALIDARG;
  nTicks = nValue;
  return S_OK;
}

HRESULT CTimeSpan::SetFrom(__in int nHours, __in int nMinutes, __in int nSeconds)
{
  BOOL bOverflow;
  LONGLONG nValue;

  nValue = CalculateTicks(0, nHours, nMinutes, nSeconds, 0, &bOverflow);
  if (bOverflow != FALSE)
    return E_INVALIDARG;
  nTicks = nValue;
  return S_OK;
}

HRESULT CTimeSpan::Negate()
{
  if (nTicks == (-0x7FFFFFFFFFFFFFFFi64 - 1i64))
    return MX_E_ArithmeticOverflow;
  nTicks = -nTicks;
  return S_OK;
}

HRESULT CTimeSpan::Parse(__in_z LPCSTR szSrcA, __in SIZE_T nLength, __out_opt LPSTR *lpszEndStringA)
{
  // Parse [ws][-][dd.]hh:mm:ss[.ff][ws]
  int nDays, nHours, nMinutes, nSeconds, nTicksX;
  LONGLONG nResult;
  LPCSTR szBackupA;
  BOOL bSign, bOverflow;
  HRESULT hRes;

  if (lpszEndStringA != NULL)
    *lpszEndStringA = (LPSTR)szSrcA;
  while (nLength>0 && (*szSrcA==' ' || *szSrcA=='\t'))
  {
    szSrcA++;
    nLength--;
  }
  //get sign
  bSign = FALSE;
  if (nLength > 0 && *szSrcA == '-')
  {
    bSign = TRUE;
    szSrcA++;
    nLength--;
  }
  //get days count
  szBackupA = szSrcA;
  if (myStrTol(nDays, szSrcA, nLength) == FALSE ||
      nDays < 0)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for day separator
  if (nLength > 0 && *szSrcA == '.')
  {
    //if found, skip
    szSrcA++;
    nLength--;
    //and parse hours
    if (myStrTol(nHours, szSrcA, nLength) == FALSE ||
        nHours < 0 || nHours > 23)
    {
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
  }
  else
  {
    nHours = nDays;
    if (nHours < 0 || nHours > 23)
    {
      szSrcA = szBackupA;
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
    nDays = 0;
  }
  //check for hours separator and skip it
  if (nLength == 0 || *szSrcA != ':')
  {
    hRes = E_FAIL;
    goto parse_err;
  }
  szSrcA++;
  nLength--;
  //get minutes count
  if (myStrTol(nMinutes, szSrcA, nLength) == FALSE ||
      nMinutes < 0 || nMinutes > 59)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for minutes separator and skip it
  if (nLength == 0 || *szSrcA != ':')
  {
    hRes = E_FAIL;
    goto parse_err;
  }
  szSrcA++;
  nLength--;
  //get seconds count
  if (myStrTol(nSeconds, szSrcA, nLength) == FALSE ||
      nSeconds < 0 || nSeconds > 59)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for ticks separator
  nTicksX = 0;
  if (nLength > 0 && *szSrcA == L'.')
  {
    //if found, skip
    szSrcA++;
    nLength--;
    //get ticks count
    if (myStrTol(nTicksX, szSrcA, nLength) == FALSE ||
        nTicksX < 0 || nTicksX > 9999999)
    {
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
  }
  nResult = CalculateTicks(nDays, nHours, nMinutes, nSeconds, 0, &bOverflow);
  if (bOverflow != FALSE || (nResult+(LONGLONG)nTicksX) < nResult)
  {
    hRes = MX_E_ArithmeticOverflow;
    goto parse_err;
  }
  nResult += (LONGLONG)nTicksX;
  nTicks = (bSign == FALSE) ? nResult : (-nResult);
  hRes = S_OK;
  //done
parse_err:
  if (lpszEndStringA != NULL)
    *lpszEndStringA = (LPSTR)szSrcA;
  return hRes;
}

HRESULT CTimeSpan::Parse(__in_z LPCWSTR szSrcW, __in SIZE_T nLength, __out_opt LPWSTR *lpszEndStringW)
{
  // Parse [ws][-][dd.]hh:mm:ss[.ff][ws]
  int nDays, nHours, nMinutes, nSeconds, nTicksX;
  LONGLONG nResult;
  LPCWSTR szBackupW;
  BOOL bSign, bOverflow;
  HRESULT hRes;

  if (lpszEndStringW != NULL)
    *lpszEndStringW = (LPWSTR)szSrcW;
  while (nLength>0 && (*szSrcW==L' ' || *szSrcW==L'\t'))
  {
    szSrcW++;
    nLength--;
  }
  //get sign
  bSign = FALSE;
  if (nLength > 0 && *szSrcW == L'-')
  {
    bSign = TRUE;
    szSrcW++;
    nLength--;
  }
  //get days count
  szBackupW = szSrcW;
  if (myWcsTol(nDays, szSrcW, nLength) == FALSE ||
      nDays < 0)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for day separator
  if (nLength > 0 && *szSrcW == L'.')
  {
    //if found, skip
    szSrcW++;
    nLength--;
    //and parse hours
    if (myWcsTol(nHours, szSrcW, nLength) == FALSE ||
        nHours < 0 || nHours > 23)
    {
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
  }
  else
  {
    nHours = nDays;
    if (nHours < 0 || nHours > 23)
    {
      szSrcW = szBackupW;
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
    nDays = 0;
  }
  //check for hours separator and skip it
  if (nLength == 0 || *szSrcW != L':')
  {
    hRes = E_INVALIDARG;
    goto parse_err;
  }
  szSrcW++;
  nLength--;
  //get minutes count
  if (myWcsTol(nMinutes, szSrcW, nLength) == FALSE ||
      nMinutes < 0 || nMinutes > 59)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for minutes separator and skip it
  if (nLength == 0 || *szSrcW != L':')
  {
    hRes = E_INVALIDARG;
    goto parse_err;
  }
  szSrcW++;
  nLength--;
  //get seconds count
  if (myWcsTol(nSeconds, szSrcW, nLength) == FALSE ||
      nSeconds < 0 || nSeconds > 59)
  {
    hRes = MX_E_InvalidData;
    goto parse_err;
  }
  //check for ticks separator
  nTicksX = 0;
  if (nLength > 0 && *szSrcW == L'.')
  {
    //if found, skip
    szSrcW++;
    nLength--;
    //get ticks count
    if (myWcsTol(nTicksX, szSrcW, nLength) == FALSE ||
        nTicksX < 0 || nTicksX > 9999999)
    {
      hRes = MX_E_InvalidData;
      goto parse_err;
    }
  }
  nResult = CalculateTicks(nDays, nHours, nMinutes, nSeconds, 0, &bOverflow);
  if (bOverflow != FALSE || (nResult+(LONGLONG)nTicksX) < nResult)
  {
    hRes = MX_E_ArithmeticOverflow;
    goto parse_err;
  }
  nResult += (LONGLONG)nTicksX;
  nTicks = (bSign == FALSE) ? nResult : (-nResult);
  hRes = S_OK;
  //done
parse_err:
  if (lpszEndStringW != NULL)
    *lpszEndStringW = (LPWSTR)szSrcW;
  return hRes;
}

HRESULT CTimeSpan::Format(__inout CStringW& cStrDestW)
{
  cStrDestW.Empty();
  return AppendFormat(cStrDestW);
}

HRESULT CTimeSpan::AppendFormat(__inout CStringW& cStrDestW)
{
  LONGLONG nTemp;

  if (nTicks < 0)
  {
    if (cStrDestW.Concat(L"-") == FALSE)
      return E_OUTOFMEMORY;
  }
  nTemp = GetDays();
  if (nTemp != 0)
  {
    if (nTemp < 0)
      nTemp = -nTemp;
    if (cStrDestW.AppendFormat(L"%I64u.", nTemp) == FALSE)
      return E_OUTOFMEMORY;
  }
  nTemp = GetHours();
  if (nTemp < 0)
    nTemp = -nTemp;
  if (cStrDestW.AppendFormat(L"%I64u:", nTemp) == FALSE)
    return E_OUTOFMEMORY;
  nTemp = GetMinutes();
  if (nTemp < 0)
    nTemp = -nTemp;
  if (cStrDestW.AppendFormat(L"%I64u:", nTemp) == FALSE)
    return E_OUTOFMEMORY;
  nTemp = GetSeconds();
  if (nTemp < 0)
    nTemp = -nTemp;
  if (cStrDestW.AppendFormat(L"%I64u", nTemp) == FALSE)
    return E_OUTOFMEMORY;
  nTemp = nTicks % MX_DATETIME_TICKS_PER_SECOND;
  if (nTemp < 0)
    nTemp = -nTemp;
  if (nTemp != 0)
  {
    if (cStrDestW.AppendFormat(L".%lu", (unsigned int)nTemp / 10000) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

CTimeSpan& CTimeSpan::operator=(__in const CTimeSpan& cSrc)
{
  if (&cSrc != this)
    nTicks = cSrc.nTicks;
  return *this;
}

bool CTimeSpan::operator==(__in const CTimeSpan& cTs) const
{
  return (nTicks == cTs.nTicks) ? true : false;
}

bool CTimeSpan::operator!=(__in const CTimeSpan& cTs) const
{
  return (nTicks != cTs.nTicks) ? true : false;
}

bool CTimeSpan::operator<(__in const CTimeSpan& cTs) const
{
  return (nTicks < cTs.nTicks) ? true : false;
}

bool CTimeSpan::operator>(__in const CTimeSpan& cTs) const
{
  return (nTicks > cTs.nTicks) ? true : false;
}

bool CTimeSpan::operator<=(__in const CTimeSpan& cTs) const
{
  return (nTicks <= cTs.nTicks) ? true : false;
}

bool CTimeSpan::operator>=(__in const CTimeSpan& cTs) const
{
  return (nTicks >= cTs.nTicks) ? true : false;
}

} //namespace MX

//-----------------------------------------------------------

static LONGLONG CalculateTicks(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds,
                               __in int nMilliSeconds, __out_opt LPBOOL lpbOverflow)
{
  LONGLONG t, nTicksDays, nTemp;
  BOOL bOverflow;

  if (nHours<0 || nHours>23 || nMinutes<0 || nMinutes>59 ||
      nSeconds<0 || nSeconds>59 || nMilliSeconds<0 || nMilliSeconds>999)
  {
    if (lpbOverflow != NULL)
      *lpbOverflow = TRUE;
    return 0; //error
  }
  t = (LONGLONG)(((nHours * 3600) + (nMinutes * 60) + nSeconds) * 1000 + nMilliSeconds);
  t *= 10000i64;
  bOverflow = FALSE;
  // nDays is problematic because it can bOverflow but that bOverflow can be 
  // "legal" (i.e. temporary) (e.g. if other parameters are negative) or 
  // illegal (e.g. sign change).
  if (nDays > 0)
  {
    nTicksDays = MX_DATETIME_TICKS_PER_DAY * nDays;
    if (t < 0)
    {
      nTemp = t;
      t += nTicksDays;
      // positive nDays -> total nTicks should be lower
      bOverflow = (nTemp > t) ? TRUE : FALSE;
    }
    else
    {
      t += nTicksDays;
      // positive + positive != negative result
      bOverflow = (t < 0) ? TRUE : FALSE;
    }
  }
  else if (nDays < 0)
  {
    nTicksDays = MX_DATETIME_TICKS_PER_DAY * nDays;
    if (t <= 0)
    {
      t += nTicksDays;
      // negative + negative != positive result
      bOverflow = (t > 0) ? TRUE : FALSE;
    }
    else
    {
      nTemp = t;
      t += nTicksDays;
      // negative nDays -> total nTicks should be lower
      bOverflow = (t > nTemp) ? TRUE : FALSE;
    }
  }
  if (lpbOverflow != NULL)
    *lpbOverflow = bOverflow;
  return (bOverflow == FALSE) ? t : 0;
}

static BOOL myStrTol(__out int &nValue, __inout_z LPCSTR &sA, __inout SIZE_T &nLen)
{
  BOOL bNeg = FALSE;
  ULONG nVal, nDigit;

  nValue = 0;
  nVal = 0;
  while (nLen > 0 && (sA[0] == ' ' || sA[0] == '\t'))
  {
    sA++;
    nLen--;
  }
  if (nLen > 0 && sA[0] == '-')
  {
    bNeg = TRUE;
    sA++;
    nLen--;
  }
  while (nLen > 0 && sA[0] >= '0' && sA[0] <= '9')
  {
    if (nVal > ULONG_MAX/10)
      return FALSE;
    nDigit = (ULONG)(sA[0] - '0');
    nVal = nVal * 10;
    if (nVal > ULONG_MAX-nDigit)
      return FALSE;
    nVal += nDigit;
    sA++;
    nLen--;
  }
  if (bNeg == FALSE)
  {
    if (nVal > LONG_MAX)
      return FALSE;
    nValue = (int)nVal;
  }
  else
  {
    if (nVal > -LONG_MIN)
      return FALSE;
    nValue = -((int)nVal);
  }
  return TRUE;
}

static BOOL myWcsTol(__out int &nValue, __inout_z LPCWSTR &sW, __inout SIZE_T &nLen)
{
  BOOL bNeg = FALSE;
  ULONG nVal, nDigit;

  nValue = 0;
  nVal = 0;
  while (nLen > 0 && (sW[0] == L' ' || sW[0] == L'\t'))
  {
    sW++;
    nLen--;
  }
  if (nLen > 0 && sW[0] == L'-')
  {
    bNeg = TRUE;
    sW++;
    nLen--;
  }
  while (nLen > 0 && sW[0] >= L'0' && sW[0] <= L'9')
  {
    if (nVal > ULONG_MAX/10)
      return FALSE;
    nDigit = (ULONG)(sW[0] - L'0');
    nVal = nVal * 10;
    if (nVal > ULONG_MAX-nDigit)
      return FALSE;
    nVal += nDigit;
    sW++;
    nLen--;
  }
  if (bNeg == FALSE)
  {
    if (nVal > LONG_MAX)
      return FALSE;
    nValue = (int)nVal;
  }
  else
  {
    if (nVal > -LONG_MIN)
      return FALSE;
    nValue = -((int)nVal);
  }
  return TRUE;
}

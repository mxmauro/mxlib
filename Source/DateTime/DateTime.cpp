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
#include "..\..\Include\DateTime\DateTime.h"
#include <math.h>
#include <intsafe.h>

//-----------------------------------------------------------

//NOTE: In PPC is compiled with /Gy to prevent LNK1166

//-----------------------------------------------------------

#define X_MAX_VALUE_TICKS             3155378975999999999i64

//-----------------------------------------------------------

static const int aDateTimeMonthDays[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

//-----------------------------------------------------------

static HRESULT Format_AddNumber(_Inout_ MX::CStringW &cStrDestW, _In_ SIZE_T nNumber, _In_ SIZE_T nMinDigits);
static HRESULT DoTwoDigitsYearAdjustment(_Inout_ int &nYear, _In_ int nTwoDigitsYearRule);
static VOID TicksToYearAndDayOfYear(_In_ const MX::CTimeSpan &cTicks, _Out_opt_ int *lpnYear,
                                    _Out_opt_ int *lpnDayOfYear);
static BOOL ConvertCustomSettingsA2W(_Out_ MX::CDateTime::LPCUSTOMSETTINGSW lpCustomW,
                                     _In_ MX::CDateTime::LPCUSTOMSETTINGSA lpCustomA);
static VOID FreeCustomSettings(_In_ MX::CDateTime::LPCUSTOMSETTINGSW lpCustomW);
static VOID FixCustomSettings(_Out_ MX::CDateTime::LPCUSTOMSETTINGSW lpOut, _In_ MX::CDateTime::LPCUSTOMSETTINGSW lpIn);
static HRESULT GetRoundedMillisecondsForOA(_In_ double nMs, _Out_ LONGLONG *lpnRes);
static LPWSTR Ansi2Wide(__in_nz_opt LPCSTR szSrcA);
static SIZE_T CompareStrW(_In_z_ LPCWSTR szStrW, _In_z_ LPCWSTR szCompareToW);
static int FixGmtOffset(_In_ int nGmtOffset);

//-----------------------------------------------------------

namespace MX {

CDateTime::CDateTime(_In_ const CDateTime& cSrc) : CBaseMemObj()
{
  cTicks = cSrc.cTicks;
  nGmtOffset = cSrc.nGmtOffset;
  return;
}

CDateTime::CDateTime(_In_opt_ LONGLONG nTicks, _In_opt_ int _nGmtOffset) : CBaseMemObj()
{
  cTicks.SetFromTicks((nTicks>=0 && nTicks<=X_MAX_VALUE_TICKS) ? nTicks : 0);
  nGmtOffset = FixGmtOffset(_nGmtOffset);
  return;
}

CDateTime::CDateTime(_In_ CTimeSpan &cTs, _In_opt_ int _nGmtOffset) : CBaseMemObj()
{
  LONGLONG nTicks;

  nTicks = cTs.GetTicks();
  cTicks.SetFromTicks((nTicks>=0 && nTicks<=X_MAX_VALUE_TICKS) ? nTicks : 0);
  nGmtOffset = FixGmtOffset(_nGmtOffset);
  return;
}

CDateTime::CDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_opt_ int _nGmtOffset) : CBaseMemObj()
{
  int nAbsDay;

  nAbsDay = GetAbsoluteDay(nYear, nMonth, nDay);
  if (nAbsDay < 0 || cTicks.SetFrom(nAbsDay, 0, 0, 0) == FALSE)
    cTicks.SetFromTicks(0);
  nGmtOffset = FixGmtOffset(_nGmtOffset);
  return;
}

CDateTime::CDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_ int nHours, _In_ int nMinutes,
                     _In_ int nSeconds, _In_opt_ int nMilliSeconds, _In_opt_ int _nGmtOffset) : CBaseMemObj()
{
  int nAbsDay;

  nAbsDay = GetAbsoluteDay(nYear, nMonth, nDay);
  if (nAbsDay < 0 || cTicks.SetFrom(nAbsDay, nHours, nMinutes, nSeconds) == FALSE)
    cTicks.SetFromTicks(0);
  nGmtOffset = FixGmtOffset(_nGmtOffset);
  return;
}

CDateTime::~CDateTime()
{
  return;
}

VOID CDateTime::Clear()
{
  cTicks.SetFromTicks(0);
  nGmtOffset = 0;
  return;
}

HRESULT CDateTime::SetDate(_In_ int nYear, _In_ int nMonth, _In_ int nDay)
{
  CTimeSpan cTempTicks;
  int nAbsDay;
  HRESULT hRes;

  nAbsDay = GetAbsoluteDay(nYear, nMonth, nDay);
  if (nAbsDay < 0)
    return E_INVALIDARG;
  hRes = cTempTicks.SetFrom(nAbsDay, GetHours(), GetMinutes(), GetSeconds());
  if (SUCCEEDED(hRes))
    cTicks = cTempTicks;
  return hRes;
}

HRESULT CDateTime::SetTime(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds, _In_opt_ int nMilliSeconds)
{
  return cTicks.SetFrom((int)cTicks.GetDays(), nHours, nMinutes, nSeconds, nMilliSeconds);
}

HRESULT CDateTime::SetDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_ int nHours, _In_ int nMinutes,
                               _In_ int nSeconds, _In_opt_ int nMilliSeconds)
{
  CTimeSpan cOldTicks;
  HRESULT hRes;

  cOldTicks = cTicks;
  hRes = SetDate(nYear, nMonth, nDay);
  if (SUCCEEDED(hRes))
    hRes = SetTime(nHours, nMinutes, nSeconds, nMilliSeconds);
  if (FAILED(hRes))
    cTicks = cOldTicks;
  return hRes;
}

HRESULT CDateTime::SetFromTicks(_In_ LONGLONG nTicks)
{
  if (nTicks < 0 || nTicks > X_MAX_VALUE_TICKS)
    return E_INVALIDARG;
  cTicks.SetFromTicks(nTicks);
  return S_OK;
}

HRESULT CDateTime::SetFromSystemTime(_In_ const SYSTEMTIME& sSrcSysTime)
{
  HRESULT hRes;

  hRes = SetDateTime((int)sSrcSysTime.wYear, (int)sSrcSysTime.wMonth, (int)sSrcSysTime.wDay,
                     (int)sSrcSysTime.wHour, (int)sSrcSysTime.wMinute, (int)sSrcSysTime.wSecond,
                      (int)sSrcSysTime.wMilliseconds);
  if (SUCCEEDED(hRes))
    nGmtOffset = 0;
  return hRes;
}

HRESULT CDateTime::SetFromFileTime(_In_ const FILETIME& sSrcFileTime)
{
  SYSTEMTIME sSysTime;

  if (::FileTimeToSystemTime(&sSrcFileTime, &sSysTime) == FALSE)
    return E_INVALIDARG;
  return SetFromSystemTime(sSysTime);
}

HRESULT CDateTime::SetFromString(_In_z_ LPCSTR szDateA, _In_z_ LPCSTR szFormatA, _In_opt_ LPCUSTOMSETTINGSA lpCustomA,
                                 _In_opt_ int nTwoDigitsYearRule)
{
  CStringW cStrTempDateW, cStrTempFormatW;
  CUSTOMSETTINGSW sCustomW;
  HRESULT hRes;

  if (szDateA == NULL || szFormatA == NULL)
    return E_POINTER;
  if (cStrTempDateW.Copy(szDateA) == FALSE || cStrTempFormatW.Copy(szFormatA) == FALSE)
    return E_OUTOFMEMORY;
  if (lpCustomA != NULL)
  {
    if (ConvertCustomSettingsA2W(&sCustomW, lpCustomA) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = SetFromString((LPWSTR)cStrTempDateW, (LPWSTR)cStrTempFormatW, (lpCustomA != NULL) ? &sCustomW : NULL,
                       nTwoDigitsYearRule);
  if (lpCustomA != NULL)
    FreeCustomSettings(&sCustomW);
  //done
  return hRes;
}

HRESULT CDateTime::SetFromString(_In_z_ LPCWSTR szDateW, _In_z_ LPCWSTR szFormatW, _In_opt_ LPCUSTOMSETTINGSW lpCustomW,
                                 _In_opt_ int nTwoDigitsYearRule)
{
  int nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds;
  int nHourFlag, nYDay, nWeekDay, nWeekNumber, nSign;
  CUSTOMSETTINGSW sCustomW;
  SIZE_T i, nTemp, nTemp2, nMaxDigits;
  LPWSTR szOldFormatW;
  CStringW cStrTempFormatW;
  BOOL bAlternateFlag, bAlternateFlag2;
  struct tagTimeZone {
    int nOffset;
    BOOL bOffsetSpecified;
  } sTimeZone;
  HRESULT hRes;

  if (szDateW == NULL || szFormatW == NULL)
    return E_POINTER;
  if (*szDateW == 0)
    return E_INVALIDARG;
  if (nTwoDigitsYearRule > 0 && nTwoDigitsYearRule <= 100)
    return E_INVALIDARG;

  FixCustomSettings(&sCustomW, lpCustomW);
  nYear = nMonth = nDay = -1;
  nHours = nMinutes = nSeconds = nMilliSeconds = 0;
  nWeekDay = nWeekNumber = -1;
  nHourFlag = nYDay = 0;
  sTimeZone.bOffsetSpecified = FALSE;

sfs_restart:
  szOldFormatW = NULL;
  while (*szFormatW != 0 && *szDateW != 0)
  {
    if (*szFormatW == L'%')
    {
      szFormatW++;
      bAlternateFlag = FALSE;
      if (*szFormatW == L'#')
      {
        szFormatW++;
        bAlternateFlag = TRUE;
      }
      if (szOldFormatW != NULL && *szFormatW == L'!')
      {
        szFormatW++;
        bAlternateFlag2 = TRUE;
      }
      if (*szFormatW == 0)
        continue;

      switch (*szFormatW)
      {
        case L'a': // abbreviated weekday name
        case L'A': // full weekday name
          //try full first
          for (i = 0; i < 7; i++)
          {
            nTemp = CompareStrW(szDateW, sCustomW.szLongDayNamesW[i]);
            if (nTemp != 0)
              break;
          }
          if (i >= 7)
          {
            //no match... try short
            for (i = 0; i < 7; i++)
            {
              nTemp = CompareStrW(szDateW, sCustomW.szShortDayNamesW[i]);
              if (nTemp != 0)
                break;
            }
          }
          if (i < 7)
          {
            szDateW += nTemp;
            nWeekDay = (int)i;
            break;
          }
          //else skip phrase
pfs_skip_phrase:
          while (*szDateW != 0)
          {
            if (((*szDateW < L'A' || *szDateW > L'Z') && (*szDateW < L'a' || *szDateW > L'z')) &&
                (*szDateW < L'0' || *szDateW > '9') && *szDateW < 128)
              break;
            if (*szDateW == *szFormatW)
              break;
            szDateW++;
          }
          break;

        case L'b': // abbreviated month name
        case L'B': // full month name
          //try full first
          for (i = 0; i < 12; i++)
          {
            nTemp = CompareStrW(szDateW, sCustomW.szLongMonthNamesW[i]);
            if (nTemp != 0)
              break;
          }
          if (i >= 12)
          {
            //no match... try short
            for (i = 0; i < 12; i++)
            {
              nTemp = CompareStrW(szDateW, sCustomW.szShortMonthNamesW[i]);
              if (nTemp != 0)
                break;
            }
          }
          if (i >= 12)
            goto pfs_skip_phrase;
          szDateW += nTemp;
          nMonth = (int)(i+1);
          break;

        case L'd': // mday in decimal (01-31)
        case L'H': // 24-hour decimal (00-23)
        case L'I': // 12-hour decimal (01-12)
        case L'j': // yday in decimal (001-366)
        case L'm': // month in decimal (01-12)
        case L'M': // minute in decimal (00-59)
        case L'S': // secs in decimal (00-59)
        case L'f': // milliseconds in decimal (0-999)
        case L'w': // week day in decimal (0-6)
        case L'U': // sunday week number (00-53)
        case L'W': // monday week number (00-53)
        case L'y': // year w/o century (00-99)
        case L'Y': // year w/ century
          nMaxDigits = 2;
          switch (*szFormatW)
          {
            case L'j': // yday in decimal (001-366)
            case L'f': // milliseconds in decimal (0-999)
              nMaxDigits = 3;
              break;
            case L'w': // week day in decimal (0-6)
              nMaxDigits = 1;
              break;
            case L'Y': // year w/ century
              nMaxDigits = 4;
              break;
          }

          if (*szDateW < L'0' || *szDateW > L'9')
            return E_FAIL;
          nTemp = 0;
          while (*szDateW >= L'0' && *szDateW <= L'9' && nMaxDigits > 0)
          {
            nTemp = (nTemp * 10) + (SIZE_T)(*szDateW - L'0');
            szDateW++;
            nMaxDigits--;
          }

          switch (*szFormatW)
          {
            case L'd': // mday in decimal (01-31)
              if (nTemp < 1 || nTemp > 31)
                return MX_E_ArithmeticOverflow;
              nDay = (int)nTemp;
              break;

            case L'H': // 24-hour decimal (00-23)
              if (nTemp > 23)
                return MX_E_ArithmeticOverflow;
              nHours = (int)nTemp;
              nHourFlag = 0;
              break;

            case L'I': // 12-hour decimal (01-12)
              if (nTemp < 1 || nTemp > 12)
                return MX_E_ArithmeticOverflow;
              nHours = (int)nTemp - 1;
              nHourFlag = -1;
              break;

            case L'j': // yday in decimal (001-366)
              if (nTemp < 1 || nTemp > 366)
                return MX_E_ArithmeticOverflow;
              nYDay = (int)nTemp;
              break;

            case L'm': // month in decimal (01-12)
              if (nTemp < 1 || nTemp > 12)
                return MX_E_ArithmeticOverflow;
              nMonth = (int)nTemp;
              break;

            case L'M': // minute in decimal (00-59)
              if (nTemp > 59)
                return MX_E_ArithmeticOverflow;
              nMinutes = (int)nTemp;
              break;

            case L'S': // secs in decimal (00-59)
              if (nTemp > 59)
                return MX_E_ArithmeticOverflow;
              nSeconds = (int)nTemp;
              break;

            case L'f': // milliseconds in decimal (0-999)
              if (nTemp > 999)
                return MX_E_ArithmeticOverflow;
              nMilliSeconds = (int)nTemp;
              break;

            case L'w': // week day in decimal (0-6)
              if (nTemp > 6)
                return MX_E_ArithmeticOverflow;
              nWeekDay = (int)nTemp;
              break;

            case L'U': // sunday week number (00-53)
            case L'W': // monday week number (00-53)
              if (nTemp > 53)
                return MX_E_ArithmeticOverflow;
              nWeekNumber = (int)((*szFormatW == L'U') ? nTemp : (nTemp+100));
              break;

            case L'y': // year w/o century (00-99)
            case L'Y': // year w/ century
              nYear = (int)nTemp;
              if (nYear <= 99)
              {
                //do two digits year adjustment
                hRes = DoTwoDigitsYearAdjustment(nYear, nTwoDigitsYearRule);
                if (FAILED(hRes))
                  return hRes;
              }
              if (nYear < 1 || nYear > 99999999)
                return MX_E_ArithmeticOverflow;
              break;
          }
          break;

        case L'p': // AM/PM designation
          for (i = 0; i < 2; i++)
          {
            nTemp = CompareStrW(szDateW, sCustomW.szTimeAmPmW[i]);
            if (nTemp != 0)
              break;
          }
          if (i >= 2)
            goto pfs_skip_phrase;
          szDateW += nTemp;
          nHourFlag = (int)(i+1);
          break;

        case L'x': // date display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if ((cStrTempFormatW.Copy((bAlternateFlag == FALSE) ? sCustomW.szLongDateFormatW :
                                                                   sCustomW.szShortDateFormatW)) == FALSE)
          {
            return E_OUTOFMEMORY;
          }
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'X': // time display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if (cStrTempFormatW.Copy(sCustomW.szTimeFormatW) == FALSE)
            return E_OUTOFMEMORY;
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'c': // date and time display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if (cStrTempFormatW.Copy((bAlternateFlag == FALSE) ? sCustomW.szLongDateFormatW :
                                                                  sCustomW.szShortDateFormatW) == FALSE ||
              cStrTempFormatW.Concat(L" ") == FALSE ||
              cStrTempFormatW.Concat(sCustomW.szTimeFormatW) == FALSE)
          {
            return E_OUTOFMEMORY;
          }
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'z': // time zone name, if any
          //assume it may be [AAA][+|-]nn[:nn[:nn]]
          sTimeZone.nOffset = 0;
          sTimeZone.bOffsetSpecified = FALSE;
          //get name
          if (*szDateW == L'<')
            return E_NOTIMPL;
          if ((*szDateW >= L'A' && *szDateW <= L'Z') || (*szDateW >= L'a' && *szDateW <= L'z'))
          {
            //only GMT and UT strings are supported
            if (StrNCompareW(szDateW, L"GMT", 3, TRUE) == 0)
            {
              szDateW += 3;
            }
            else if (StrNCompareW(szDateW, L"UT", 2, TRUE) == 0)
            {
              szDateW += 2;
            }
            else
            {
              return MX_E_Unsupported;
            }
            if ((*szDateW >= L'A' && *szDateW <= L'Z') || (*szDateW >= L'a' && *szDateW <= L'z'))
              return E_FAIL;
            sTimeZone.bOffsetSpecified = TRUE;
          }
          else if (*szDateW == L'+' || *szDateW == L'-')
          {
            //use numeric form
            nSign = (*szDateW++ == L'+') ? 1 : -1;

            //get hours
            if (szDateW[0] < L'0' || szDateW[0] > L'9' || szDateW[1] < L'0' || szDateW[1] > L'9')
              return E_FAIL;
            nTemp = (SIZE_T)(szDateW[0] - L'0')*10 + (SIZE_T)(szDateW[1] - L'0');
            if (nTemp > 14)
              return MX_E_ArithmeticOverflow;
            nTemp *= 60;
            szDateW += 2;

            //time separator?
            if (*szDateW == L':')
              szDateW++;

            //get minutes
            if (*szDateW >= L'0' && *szDateW <= L'9')
            {
              if (szDateW[1] < L'0' || szDateW[1] > L'9')
                return E_FAIL;
              nTemp2 = (SIZE_T)(szDateW[0] - L'0')*10 + (SIZE_T)(szDateW[1] - L'0');
              if (nTemp2 >= 60)
                return MX_E_ArithmeticOverflow;
              nTemp += nTemp2;
              szDateW += 2;
              //time separator?
              if (*szDateW == L':')
                szDateW++;
            }

            //get seconds (not added to final gmt offset)
            if (*szDateW >= L'0' && *szDateW <= L'9')
            {
              if (szDateW[1] < L'0' || szDateW[1] > L'9')
                return E_FAIL;
              nTemp2 = (SIZE_T)(szDateW[0] - L'0')*10 + (SIZE_T)(szDateW[1] - L'0');
              if (nTemp2 >= 60)
                return MX_E_ArithmeticOverflow;
              szDateW += 2;
            }

            //set time offset
            sTimeZone.nOffset = (int)nTemp * nSign;
            if (sTimeZone.nOffset < -12*60 || sTimeZone.nOffset > 14*60)
              return MX_E_ArithmeticOverflow;
            sTimeZone.bOffsetSpecified = TRUE;
          }
          break;

        case L'%': // percent sign
          if (*szDateW != L'%')
            return E_FAIL;
          szDateW++;
          break;
      }
      szFormatW++;
    }
    else if (*szFormatW == L'/')
    {
      nTemp = StrLenW(sCustomW.szDateSeparatorW);
      if (StrNCompareW(szDateW, sCustomW.szDateSeparatorW, nTemp, TRUE) != 0)
        return E_FAIL;
      szDateW += nTemp;
      szFormatW++;
    }
    else if (*szFormatW == L':')
    {
      nTemp = StrLenW(sCustomW.szTimeSeparatorW);
      if (StrNCompareW(szDateW, sCustomW.szTimeSeparatorW, nTemp, TRUE) != 0)
        return E_FAIL;
      szDateW += nTemp;
      szFormatW++;
    }
    else if (*szFormatW == L'\\')
    {
      szFormatW++;
      if (*szFormatW != 0)
      {
        if (*szDateW != *szFormatW)
          return E_FAIL;
        szFormatW++;
      }
    }
    else if (*szFormatW == L' ' || *szFormatW == L'\t')
    {
      while (*szFormatW == L' ' || *szFormatW == L'\t')
        szFormatW++;
      if (*szDateW != L' ' && *szDateW != L'\t')
        return E_FAIL;
      while (*szDateW == L' ' || *szDateW == L'\t')
        szDateW++;
    }
    else if (CharToUpperW(*szDateW) == CharToUpperW(*szFormatW))
    {
      szFormatW++;
      szDateW++;
    }
    else
    {
      return E_FAIL;
    }
  }
  if (*szDateW != 0 && szOldFormatW != NULL)
  {
    szFormatW = szOldFormatW;
    goto sfs_restart;
  }
  if (*szDateW == 0 && *szFormatW != 0)
    return E_FAIL;

  if (nHourFlag == 2 && nHours < 12)
    nHours += 12; //convert to pm
  if (nYear >= 0 && (nMonth < 0 || nDay < 0))
  {
    if (nYDay < 0 && nWeekDay >= 0 && nWeekNumber >= 0)
    {
      //convert from weekday/weeknumber to dayofyear
      nYDay = (nWeekNumber % 100) * 7 + nWeekDay + 1;
      if (nWeekNumber >= 100)
        nYDay++;
    }
    if (nYDay > 0)
    {
      //convert from dayofyear to day/month
      bAlternateFlag = IsLeapYear(nYear);
      nDay = nYDay;
      for (nMonth=1; nMonth<=12; nMonth++)
      {
        nTemp = (SIZE_T)aDateTimeMonthDays[nMonth] - (SIZE_T)aDateTimeMonthDays[nMonth-1] +
                ((bAlternateFlag != FALSE && i== 2) ? 1 : 0);
        if (nTemp >= (SIZE_T)nDay)
          break;
        nDay -= (int)nTemp;
      }
      if (nMonth > 12)
        return MX_E_ArithmeticOverflow;
    }
  }

  if (nYear<0 || nMonth<0 || nDay<0)
    nYear = nMonth = nDay = 1;
  hRes = SetDateTime(nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds);
  if (FAILED(hRes))
    return hRes;

  nGmtOffset = (sTimeZone.bOffsetSpecified != FALSE) ? sTimeZone.nOffset : 0;
  return S_OK;
}

HRESULT CDateTime::SetFromNow(_In_ BOOL bLocal)
{
  SYSTEMTIME sSi;
  TIME_ZONE_INFORMATION sTzi;
  HRESULT hRes;

  if (bLocal != FALSE)
    ::GetLocalTime(&sSi);
  else
    ::GetSystemTime(&sSi);
  hRes = SetFromSystemTime(sSi);
  if (FAILED(hRes))
    return hRes;
  nGmtOffset = 0;
  if (bLocal != FALSE)
  {
    MxMemSet(&sTzi, 0, sizeof(sTzi));
    if (::GetTimeZoneInformation(&sTzi) != TIME_ZONE_ID_INVALID)
      nGmtOffset = (int)(sTzi.Bias);
  }
  //done
  return S_OK;
}

HRESULT CDateTime::SetFromUnixTime(_In_ int nTime)
{
  return SetFromUnixTime((LONGLONG)nTime);
}

HRESULT CDateTime::SetFromUnixTime(_In_ LONGLONG nTime)
{
  LONGLONG nTicks;

  nTicks = nTime * MX_DATETIME_TICKS_PER_SECOND;
  if (nTime >= 0) {
    if (nTicks < nTime)
      return MX_E_ArithmeticOverflow;
  }
  else
  {
    if (nTicks > nTime)
      return MX_E_ArithmeticOverflow;
  }
  nTicks += MX_DATETIME_EPOCH_TICKS;
  if (nTicks < 0 || nTicks > X_MAX_VALUE_TICKS)
    return MX_E_ArithmeticOverflow;
  cTicks.SetFromTicks(nTicks);
  return S_OK;
}

HRESULT CDateTime::SetFromOleAutDate(_In_ double nValue)
{
  CDateTime cDt;
  double nDays, nHours;
  LONGLONG nTicks;
  HRESULT hRes;

  //nValue must be negative 657435.0 through positive 2958466.0.
  if (nValue <= -657435.0 || nValue >= 2958466.0)
    return E_INVALIDARG;
  cDt.SetFromTicks(599264352000000000i64);
  if (nValue < 0.0)
  {
    nDays = ceil(nValue);
    // integer part is the number of days (negative)
    hRes = GetRoundedMillisecondsForOA(nDays, &nTicks);
    if (SUCCEEDED(hRes))
      hRes = cDt.Add(nTicks, UnitsTicks);
    // but decimals are the number of hours (in days fractions) and positive
    if (SUCCEEDED(hRes))
    {
      nHours = (nDays - nValue);
      hRes = GetRoundedMillisecondsForOA(nHours, &nTicks);
    }
    if (SUCCEEDED(hRes))
      hRes = cDt.Add(nTicks, UnitsTicks);
  }
  else
  {
    nDays = floor(nValue);
    hRes = GetRoundedMillisecondsForOA(nDays, &nTicks);
    if (SUCCEEDED(hRes))
      hRes = cDt.Add(nTicks, UnitsTicks);
  }
  if (SUCCEEDED(hRes))
    cTicks = cDt.cTicks;
  return hRes;
}

HRESULT CDateTime::SetGmtOffset(_In_ int _nGmtOffset, _In_ BOOL bAdjustTime)
{
  HRESULT hRes;

  if (_nGmtOffset < -12*60 || _nGmtOffset > 14*60)
    return E_INVALIDARG;
  if (bAdjustTime != FALSE)
  {
    hRes = Add((LONGLONG)_nGmtOffset - (LONGLONG)nGmtOffset, CDateTime::UnitsMinutes);
    if (FAILED(hRes))
      return hRes;
  }
  nGmtOffset = _nGmtOffset;
  return S_OK;
}

VOID CDateTime::GetDate(_Out_ int *lpnYear, _Out_ int *lpnMonth, _Out_ int *lpnDay)
{
  int nDayOfYear, nYear;

  TicksToYearAndDayOfYear(cTicks, &nYear, &nDayOfYear);
  if (lpnYear != NULL)
    *lpnYear = nYear;
  CalculateDayMonth(nDayOfYear, nYear, lpnMonth, lpnDay);
  return;
}

VOID CDateTime::GetTime(_Out_ int *lpnHour, _Out_ int *lpnMinutes, _Out_ int *lpnSeconds,
                        _Out_opt_ int *lpnMilliSeconds)
{
  if (lpnHour != NULL)
    *lpnHour = cTicks.GetHours();
  if (lpnMinutes != NULL)
    *lpnMinutes = cTicks.GetMinutes();
  if (lpnSeconds != NULL)
    *lpnSeconds = cTicks.GetSeconds();
  if (lpnMilliSeconds != NULL)
    *lpnMilliSeconds = cTicks.GetMilliSeconds();
  return;
}

VOID CDateTime::GetDateTime(_Out_ int *lpnYear, _Out_ int *lpnMonth, _Out_ int *lpnDay, _Out_ int *lpnHours,
                            _Out_ int *lpnMinutes, _Out_ int *lpnSeconds, _Out_opt_ int *lpnMilliSeconds)
{
  GetDate(lpnYear, lpnMonth, lpnDay);
  GetTime(lpnHours, lpnMinutes, lpnSeconds, lpnMilliSeconds);
  return;
}

VOID CDateTime::GetSystemTime(_Out_ SYSTEMTIME &sSysTime)
{
  int nYear, nMonth, nDay, nHours, nMinutes, nSeconds;

  GetDate(&nYear, &nMonth, &nDay);
  GetTime(&nHours, &nMinutes, &nSeconds);
  sSysTime.wYear = (WORD)nYear;
  sSysTime.wMonth = (WORD)nMonth;
  sSysTime.wDay = (WORD)nDay;
  sSysTime.wHour = (WORD)nHours;
  sSysTime.wMinute = (WORD)nMinutes;
  sSysTime.wSecond = (WORD)nSeconds;
  sSysTime.wMilliseconds = 0;
  sSysTime.wDayOfWeek = (WORD)GetDayOfWeek();
  return;
}

HRESULT CDateTime::GetFileTime(_Out_ FILETIME &sFileTime)
{
  SYSTEMTIME sSysTime;

  GetSystemTime(sSysTime);
  if (::SystemTimeToFileTime(&sSysTime, &sFileTime) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  return S_OK;
}

HRESULT CDateTime::GetUnixTime(_Out_ int *lpnTime)
{
  LONGLONG nTemp;
  HRESULT hRes;

  if (lpnTime == NULL)
    return E_POINTER;
  hRes = GetUnixTime(&nTemp);
  if (SUCCEEDED(hRes) && nTemp > 0x7FFFFFFFi64)
    hRes = MX_E_ArithmeticOverflow;
  *lpnTime = (SUCCEEDED(hRes)) ? (int)nTemp : 0;
  return hRes;
}

HRESULT CDateTime::GetUnixTime(_Out_ LONGLONG *lpnTime)
{
  if (lpnTime == NULL)
    return E_POINTER;
  *lpnTime = cTicks.GetTicks();
  if ((*lpnTime) < MX_DATETIME_EPOCH_TICKS)
  {
    *lpnTime = 0;
    return MX_E_ArithmeticOverflow;
  }
  (*lpnTime) -= MX_DATETIME_EPOCH_TICKS;
  (*lpnTime) /= MX_DATETIME_TICKS_PER_SECOND;
  return S_OK;
}

HRESULT CDateTime::GetOleAutDate(_Out_ double *lpnValue)
{
  CTimeSpan cTs;
  double nRes, d;
  HRESULT hRes;

  if (lpnValue == NULL)
    return E_POINTER;
  *lpnValue = -657435.001;
  if (cTicks.GetTicks() >= 31242239136000000i64)
  {
    cTs = cTicks;
    hRes = cTs.Add(-599264352000000000i64);
    if (FAILED(hRes))
      return hRes;
    nRes = cTs.GetTotalDays();
    if (cTicks.GetTicks() < 599264352000000000i64)
    {
      //negative days (int) but decimals are positive
      d = ceil(nRes);
      nRes = d - 2.0 - (nRes - d);
    }
    else
    {
      //we can't reach maximum value
      if (nRes >= 2958466.0)
        nRes = 2958466.0 - 0.00000001;
    }
    *lpnValue = nRes;
  }
  //done
  return S_OK;
}

LONGLONG CDateTime::GetTicks() const
{
  return cTicks.GetTicks();
}

int CDateTime::GetYear() const
{
  int nYear;

  TicksToYearAndDayOfYear(cTicks, &nYear, NULL);
  return nYear;
}

int CDateTime::GetMonth() const
{
  int nDayOfYear, nYear, nMonth, nDay;

  TicksToYearAndDayOfYear(cTicks, &nYear, &nDayOfYear);
  CalculateDayMonth(nDayOfYear, nYear, &nMonth, &nDay);
  return nMonth;
}

int CDateTime::GetDay() const
{
  int nDayOfYear, nYear, nMonth, nDay;

  TicksToYearAndDayOfYear(cTicks, &nYear, &nDayOfYear);
  CalculateDayMonth(nDayOfYear, nYear, &nMonth, &nDay);
  return nDay;
}

int CDateTime::GetHours() const
{
  return cTicks.GetHours();
}

int CDateTime::GetMinutes() const
{
  return cTicks.GetMinutes();
}

int CDateTime::GetSeconds() const
{
  return cTicks.GetSeconds();
}

int CDateTime::GetMilliSeconds() const
{
  return cTicks.GetMilliSeconds();
}

int CDateTime::GetDayOfWeek() const
{
  int nYear, nMonth, nDay;

  const_cast<CDateTime*>(this)->GetDate(&nYear, &nMonth, &nDay);
  return GetDayOfWeek(nYear, nMonth, nDay);
}

int CDateTime::GetDayOfYear() const
{
  int nDayOfYear;

  TicksToYearAndDayOfYear(cTicks, NULL, &nDayOfYear);
  return nDayOfYear;
}

VOID CDateTime::GetTimeOfDay(_Out_ CTimeSpan &cDestTs)
{
  cDestTs.SetFromTicks(cTicks.GetTicks() % MX_DATETIME_TICKS_PER_DAY);
  return;
}

int CDateTime::GetGmtOffset() const
{
  return nGmtOffset;
}

BOOL CDateTime::IsLeapYear() const
{
  return IsLeapYear(GetYear());
}

CDateTime& CDateTime::operator=(_In_ const CDateTime& cSrc)
{
  if (&cSrc != this)
  {
    cTicks = cSrc.cTicks;
    nGmtOffset = cSrc.nGmtOffset;
  }
  return *this;
}

bool CDateTime::operator==(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() == cDt.GetTicks()) ? true : false;
}

bool CDateTime::operator!=(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() != cDt.GetTicks()) ? true : false;
}

bool CDateTime::operator<(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() < cDt.GetTicks()) ? true : false;
}

bool CDateTime::operator>(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() > cDt.GetTicks()) ? true : false;
}

bool CDateTime::operator<=(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() <= cDt.GetTicks()) ? true : false;
}

bool CDateTime::operator>=(_In_ const CDateTime& cDt) const
{
  return (cTicks.GetTicks() >= cDt.GetTicks()) ? true : false;
}

HRESULT CDateTime::Add(_In_ CTimeSpan &cTs)
{
  return Add(cTs.GetTicks(), UnitsTicks);
}

HRESULT CDateTime::Add(_In_ int nCount, _In_ CDateTime::eUnits nUnits)
{
  return Add((LONGLONG)nCount, nUnits);
}

HRESULT CDateTime::Add(_In_ LONGLONG nCount, _In_ CDateTime::eUnits nUnits)
{
  CTimeSpan cNewTicks, cTs;
  int k, nYear, nMonth, nDay;
  LONGLONG nMonth64, nYear64;
  HRESULT hRes = E_INVALIDARG;

  switch (nUnits)
  {
    case UnitsYear:
    case UnitsMonth:
      GetDate(&nYear, &nMonth, &nDay);
      if (nUnits == UnitsMonth && nCount != 0)
      {
        nMonth64 = (LONGLONG)nMonth + (nCount - 1);
        if (nMonth64 < 0)
        {
          nUnits = UnitsYear;
          nCount = ((nMonth64+1) / 12) - 1;
          nMonth = (int)(nMonth64 - nCount*12);
        }
        else if (nMonth64 > 11)
        {
          nUnits = UnitsYear;
          nCount = nMonth64 / 12;
          nMonth = (int)(nMonth64 % 12) + 1;
        }
        else
        {
          nCount = 0;
          nMonth = (int)nMonth64 + 1;
        }
      }
      if (nUnits == UnitsYear && nCount != 0)
      {
        nYear64 = (LONGLONG)nYear + nCount;
        if (nYear64<1 || nYear64>9999)
          return MX_E_ArithmeticOverflow;
        nYear = (int)nYear64;
      }
      k = GetDaysInMonth(nMonth, nYear);
      if (k < 1)
        return MX_E_ArithmeticOverflow;
      if (nDay > k)
        nDay = k;
      return SetDate(nYear, nMonth, nDay);

    case UnitsDay:
      hRes = cTs.SetFromDays((double)nCount);
      break;
    case UnitsHours:
      hRes = cTs.SetFromHours((double)nCount);
      break;
    case UnitsMinutes:
      hRes = cTs.SetFromMinutes((double)nCount);
      break;
    case UnitsSeconds:
      hRes = cTs.SetFromSeconds((double)nCount);
      break;
    case UnitsMilliseconds:
      hRes = cTs.SetFromMilliSeconds((double)nCount);
      break;
    case UnitsTicks:
      hRes = cTs.SetFromTicks(nCount);
      break;
  }
  if (SUCCEEDED(hRes))
  {
    cNewTicks = cTicks;
    hRes = cNewTicks.Add(cTs.GetTicks());
  }
  if (SUCCEEDED(hRes) &&
      (cNewTicks.GetTicks() < 0 || cNewTicks.GetTicks() > X_MAX_VALUE_TICKS))
    hRes = MX_E_ArithmeticOverflow;
  if (SUCCEEDED(hRes))
    cTicks = cNewTicks;
  return hRes;
}

HRESULT CDateTime::Sub(_In_ CTimeSpan &cTs)
{
  return Sub(cTs.GetTicks(), UnitsTicks);
}

HRESULT CDateTime::Sub(_In_ int nCount, _In_ eUnits nUnits)
{
  return Sub((LONGLONG)nCount, nUnits);
}

HRESULT CDateTime::Sub(_In_ LONGLONG nCount, _In_ eUnits nUnits)
{
  if (nCount == LONGLONG_MIN)
    return MX_E_ArithmeticOverflow;
  return Add(-nCount, nUnits);
}

LONGLONG CDateTime::GetDiff(_In_ const CDateTime& cFromDt, _In_ eUnits nUnits)
{
  int i, nYear[2], nMonth[2], nDay[2], nTemp[2];
  double nDblTemp[2];
  LONGLONG nTicks, nDiff;
  CTimeSpan cTs;

  switch (nUnits)
  {
    case UnitsDay:
    case UnitsHours:
    case UnitsMinutes:
    case UnitsSeconds:
    case UnitsMilliseconds:
    case UnitsTicks:
      nTicks = cFromDt.GetTicks();
      if (nTicks == LONGLONG_MIN)
      {
        //this should not happen because datetime has no
        //negative ticks
        MX_ASSERT(FALSE);
        return MX_E_ArithmeticOverflow;
      }
      cTs = cTicks;
      if (FAILED(cTs.Add(-nTicks)))
        return 0;
      switch (nUnits)
      {
        case UnitsDay:
          nDiff = (LONGLONG)cTs.GetTotalDays();
          break;
        case UnitsHours:
          nDiff = (LONGLONG)cTs.GetTotalHours();
          break;
        case UnitsMinutes:
          nDiff = (LONGLONG)cTs.GetTotalMinutes();
          break;
        case UnitsSeconds:
          nDiff = (LONGLONG)cTs.GetTotalSeconds();
          break;
        case UnitsMilliseconds:
          nDiff = (LONGLONG)cTs.GetTotalMilliSeconds();
          break;
        default:
          nDiff = cTs.GetTicks();
          break;
      }
      break;

    case UnitsMonth:
    case UnitsYear:
      GetDate(&nYear[0], &nMonth[0], &nDay[0]);
      const_cast<CDateTime&>(cFromDt).GetDate(&nYear[1], &nMonth[1], &nDay[1]);
      for (i=0; i<2; i++) {
        nTemp[i] = (aDateTimeMonthDays[nMonth[i]] - aDateTimeMonthDays[nMonth[i]-1]);
        if (IsLeapYear(nYear[i]) != FALSE)
          nTemp[i]++;
        nDblTemp[i] = (double)(nYear[i] * 12 + nMonth[i]) + (double)(nDay[i] - 1) / (double)nTemp[i];
      }
      nDiff = (LONGLONG)(nDblTemp[0] - nDblTemp[1]);
      if (nUnits == UnitsYear)
        nDiff /= 12;
      break;

    default:
      return 0;
  }
  return nDiff;
}

HRESULT CDateTime::Format(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szFormatA, _In_opt_ LPCUSTOMSETTINGSA lpCustomA)
{
  cStrDestA.Empty();
  return AppendFormat(cStrDestA, szFormatA, lpCustomA);
}

HRESULT CDateTime::Format(_Inout_ CStringW &cStrDestW, _In_z_ LPCWSTR szFormatW, _In_opt_ LPCUSTOMSETTINGSW lpCustomW)
{
  cStrDestW.Empty();
  return AppendFormat(cStrDestW, szFormatW, lpCustomW);
}

HRESULT CDateTime::AppendFormat(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szFormatA,
                                _In_opt_ LPCUSTOMSETTINGSA lpCustomA)
{
  CStringW cStrFormatW, cStrTempW;
  CUSTOMSETTINGSW sCustomW;
  HRESULT hRes;

  if (szFormatA == NULL)
    return E_POINTER;
  if (cStrFormatW.Copy(szFormatA) == FALSE)
    return E_OUTOFMEMORY;
  if (lpCustomA == NULL)
  {
    hRes = AppendFormat(cStrTempW, (LPWSTR)cStrFormatW, NULL);
  }
  else
  {
    if (ConvertCustomSettingsA2W(&sCustomW, lpCustomA) != FALSE)
    {
      hRes = AppendFormat(cStrTempW, (LPWSTR)cStrFormatW, &sCustomW);
      FreeCustomSettings(&sCustomW);
    }
    else
    {
      hRes = E_OUTOFMEMORY;
    }
  }
  if (SUCCEEDED(hRes))
  {
    if (cStrDestA.Concat((LPCWSTR)cStrTempW) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  return hRes;
}

HRESULT CDateTime::AppendFormat(_Inout_ CStringW &cStrDestW, _In_z_ LPCWSTR szFormatW,
                                _In_opt_ LPCUSTOMSETTINGSW lpCustomW)
{
  CUSTOMSETTINGSW sCustomW;
  int nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds;
  int nWeekDay, nDayOfYear, nTemp, nTemp2;
  BOOL bAlternateFlag, bAlternateFlag2;
  LPWSTR szOldFormatW;
  CStringW cStrTempFormatW;
  HRESULT hRes;

  if (szFormatW == NULL)
    return E_POINTER;
  FixCustomSettings(&sCustomW, lpCustomW);
  GetDateTime(&nYear, &nMonth, &nDay, &nHours, &nMinutes, &nSeconds, &nMilliSeconds);
  nWeekDay = nDayOfYear = -1;

fmt_restart:
  szOldFormatW = NULL;
  while (*szFormatW != 0)
  {
    if (*szFormatW == L'%')
    {
      szFormatW++;
      bAlternateFlag = bAlternateFlag2 = FALSE;
      if (*szFormatW == L'#')
      {
        szFormatW++;
        bAlternateFlag = TRUE;
      }
      if (szOldFormatW != NULL && *szFormatW == L'!')
      {
        szFormatW++;
        bAlternateFlag2 = TRUE;
      }
      if (*szFormatW == 0)
        continue;
      switch (*szFormatW)
      {
        case L'a': // abbreviated weekday name
        case L'A': // full weekday name
          if (nWeekDay < 0)
            nWeekDay = GetDayOfWeek();
#pragma warning(suppress : 6385)
          if (cStrDestW.Concat((*szFormatW == L'a') ? sCustomW.szShortDayNamesW[nWeekDay] :
                                                      sCustomW.szLongDayNamesW[nWeekDay]) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case L'b': // abbreviated month name
        case L'B': // full month name
          if (cStrDestW.Concat((*szFormatW == L'b') ? sCustomW.szShortMonthNamesW[nMonth-1] :
                                                      sCustomW.szLongMonthNamesW[nMonth-1]) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case L'c': // date and time display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if (cStrTempFormatW.Copy((bAlternateFlag == FALSE) ? sCustomW.szLongDateFormatW :
                                                               sCustomW.szShortDateFormatW) == FALSE ||
              cStrTempFormatW.Concat(L" ") == FALSE ||
              cStrTempFormatW.Concat(sCustomW.szTimeFormatW) == FALSE)
            return E_OUTOFMEMORY;
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'd': // day in decimal (01-31)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nDay, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'H': // 24-hour decimal (00-23)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nHours, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'I': // 12-hour decimal (01-12)
          if ((nTemp = nHours % 12) == 0)
            nTemp = 12;
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nTemp, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'j': // day of year in decimal (001-366)
          if (nDayOfYear < 0)
            nDayOfYear = GetDayOfYear();
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nDayOfYear+1, 3);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'm': // month in decimal (01-12)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nMonth, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'M': // minute in decimal (00-59)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nMinutes, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'p': // AM/PM designation
          {
          SIZE_T nLen = cStrDestW.GetLength();

          if (cStrDestW.Concat(sCustomW.szTimeAmPmW[(nHours<12) ? 0 : 1]) == FALSE)
            return E_OUTOFMEMORY;
          if (bAlternateFlag == FALSE)
            StrToUpperW((LPWSTR)cStrDestW + nLen);
          else
            StrToLowerW((LPWSTR)cStrDestW + nLen);
          }
          break;

        case L'S': // secs in decimal (00-59)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nSeconds, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'f': // milliseconds in decimal (0-999)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nMilliSeconds, (bAlternateFlag == FALSE) ? 3 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'w': // week day in decimal (0-6)
          if (nWeekDay < 0)
            nWeekDay = GetDayOfWeek();
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nWeekDay, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'U': // sunday week number (00-53)
        case L'W': // monday week number (00-53)
          if (nWeekDay < 0)
            nWeekDay = GetDayOfWeek();
          if (*szFormatW == L'U')
            nTemp = nWeekDay;
          else
            nTemp = (nWeekDay == 0) ? 6 : (nWeekDay-1);
          if (nDayOfYear < 0)
            nDayOfYear = GetDayOfYear();
          if (nDayOfYear < nTemp)
          {
            nTemp2 = 0;
          }
          else
          {
            nTemp2 = nDayOfYear / 7;
            if ((nDayOfYear%7) >= nTemp)
              nTemp2++;
          }
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nTemp2, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'x': // date display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if (cStrTempFormatW.Copy((bAlternateFlag == FALSE) ? sCustomW.szLongDateFormatW :
                                                               sCustomW.szShortDateFormatW) == FALSE)
            return E_OUTOFMEMORY;
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'X': // time display
          if (szOldFormatW != NULL)
            return E_INVALIDARG;
          if (cStrTempFormatW.Copy(sCustomW.szTimeFormatW) == FALSE)
            return E_OUTOFMEMORY;
          szOldFormatW = (LPWSTR)szFormatW+1;
          szFormatW = (LPWSTR)cStrTempFormatW;
          szFormatW--; //fixed below at switch end
          break;

        case L'y': // year w/o century (00-99)
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nYear % 100, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'Y': // year w/ century
          hRes = Format_AddNumber(cStrDestW, (SIZE_T)(unsigned int)nYear, (bAlternateFlag == FALSE) ? 2 : 1);
          if (FAILED(hRes))
            return hRes;
          break;

        case L'z': // time zone
          if (nGmtOffset == 0)
          {
            if (cStrDestW.Concat((bAlternateFlag == FALSE) ? L"GMT" : L"+0000") == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            if (cStrDestW.Concat((nGmtOffset >= 0) ? L"+" : L"-") == FALSE)
              return E_OUTOFMEMORY;
            nTemp = nGmtOffset;
            if (nTemp < 0)
              nTemp = -nTemp;
            hRes = Format_AddNumber(cStrDestW, (SIZE_T)(nTemp / 60), 2);
            if (SUCCEEDED(hRes))
              hRes = Format_AddNumber(cStrDestW, (SIZE_T)(nTemp % 60), 2);
            if (FAILED(hRes))
              return hRes;
          }
          break;

        case L'%': // percent sign
          if (cStrDestW.Concat(L"%") == FALSE)
            return E_OUTOFMEMORY;
          break;
      }
      szFormatW++;
    }
    else if (*szFormatW == L'/')
    {
      if (cStrDestW.Concat(sCustomW.szDateSeparatorW) == FALSE)
        return E_OUTOFMEMORY;
      szFormatW++;
    }
    else if (*szFormatW == L':')
    {
      if (cStrDestW.Concat(sCustomW.szTimeSeparatorW) == FALSE)
        return E_OUTOFMEMORY;
      szFormatW++;
    }
    else if (*szFormatW == L'\\')
    {
      szFormatW++;
      if (*szFormatW != 0)
      {
        if (cStrDestW.ConcatN(szFormatW, 1) == FALSE)
          return E_OUTOFMEMORY;
        szFormatW++;
      }
    }
    else
    {
      if (cStrDestW.ConcatN(szFormatW++, 1) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (szOldFormatW != NULL)
  {
    szFormatW = szOldFormatW;
    goto fmt_restart;
  }
  return S_OK;
}

BOOL CDateTime::IsDateValid(_In_ int nYear, _In_ int nMonth, _In_ int nDay)
{
  int nDaysInMonth;

  if (nDay < 1)
    return FALSE;
  nDaysInMonth = GetDaysInMonth(nMonth, nYear);
  return (nDaysInMonth > 0 && nDay <= nDaysInMonth) ? TRUE : FALSE;
}

BOOL CDateTime::IsTimeValid(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds)
{
  return (nHours<0 || nHours>23 || nMinutes<0 || nMinutes>59 || nSeconds<0 || nSeconds>59) ? FALSE : TRUE;
}

BOOL CDateTime::IsLeapYear(_In_ int nYear)
{
  if (nYear < 1)
    return FALSE;
  return (((nYear&3)==0) && ((nYear%100)!=0 || (nYear%400)==0)) ? TRUE : FALSE;
}

int CDateTime::GetDaysInMonth(_In_ int nMonth, _In_ int nYear)
{
  if (nYear<1 || nYear>9999 || nMonth<1 || nMonth>12)
    return 0;
  return aDateTimeMonthDays[nMonth] - aDateTimeMonthDays[nMonth-1] +
         ((IsLeapYear(nYear) != FALSE && nMonth == 2) ? 1 : 0);
}

int CDateTime::GetDayOfWeek(_In_ int nYear, _In_ int nMonth, _In_ int nDay)
{
  _In_ int nTemp;

  if (IsDateValid(nYear, nMonth, nDay) == FALSE)
    return -1;
  //it is a valid date; make Jan 1, 1AD be 1
  nTemp = (nYear*365) + (nYear/4) - (nYear/100) + (nYear/400) + aDateTimeMonthDays[nMonth-1] + nDay;
  //If leap year and it's before March, subtract 1:
  if (nMonth <= 2 && IsLeapYear(nYear) != FALSE)
    nTemp--;
  return ((nTemp - 1) % 7);
}

int CDateTime::GetDayOfYear(_In_ int nYear, _In_ int nMonth, _In_ int nDay)
{
  if (IsDateValid(nYear, nMonth, nDay) == FALSE)
    return -1;
  return aDateTimeMonthDays[nMonth-1] + (nDay-1) + ((IsLeapYear(nYear) != FALSE && nMonth > 2) ? 1 : 0);
}

int CDateTime::GetAbsoluteDay(_In_ int nYear, _In_ int nMonth, _In_ int nDay)
{
  int nM, nTemp, nLeap;

  if (IsDateValid(nYear, nMonth, nDay) == FALSE)
    return -1;
  nLeap = (IsLeapYear(nYear) != FALSE) ? 1 : 0;
  for (nM=1,nTemp=0; nM<nMonth; nM++)
    nTemp += aDateTimeMonthDays[nM] - aDateTimeMonthDays[nM-1] + ((nM == 2) ? nLeap : 0);
  nYear--;
  return (nDay-1) + nTemp + (365*nYear) + (nYear/4) - (nYear/100) + (nYear/400);
}

HRESULT CDateTime::CalculateDayMonth(_In_ int nDayOfYear, _In_ int nYear, _Out_ int *lpnMonth, _Out_ int *lpnDay)
{
  int nLeapYear, nMonth, nTemp;

  if (lpnMonth != NULL)
    *lpnMonth = 0;
  if (lpnDay != NULL)
    *lpnDay = 0;
  if (nYear < 1 || nYear > 9999 || nDayOfYear < 1)
    return E_INVALIDARG;
  nLeapYear = (IsLeapYear(nYear) != FALSE) ? 1 : 0;
  if (nDayOfYear > 365+nLeapYear)
    return E_INVALIDARG;
  nMonth = 1;
  while (true)
  {
    nTemp = aDateTimeMonthDays[nMonth] - aDateTimeMonthDays[nMonth-1] + ((nMonth == 2) ? nLeapYear : 0);
    if (nDayOfYear <= nTemp)
      break;
    nDayOfYear -= nTemp;
    nMonth++;
  }
  if (lpnMonth != NULL)
    *lpnMonth = nMonth;
  if (lpnDay != NULL)
    *lpnDay = nDayOfYear;
  return S_OK;
}

HRESULT CDateTime::CalculateEasterInYear(_In_ int nYear, _Out_ int *lpnMonth, _Out_ int *lpnDay,
                                         _In_ BOOL bOrthodoxChurchesMethod)
{
  int nFirstDig, nRemain19, nTemp, tA, tB, tC, tD, tE;

  if (lpnMonth != NULL)
    *lpnMonth = 0;
  if (lpnDay != NULL)
    *lpnDay = 0;
  if (nYear < 1583 || nYear > 4099)
    return E_INVALIDARG;
  if (lpnMonth == NULL || lpnDay == NULL)
    return E_POINTER;
  nFirstDig = nYear / 100;
  nRemain19 = nYear % 19;
  if (bOrthodoxChurchesMethod != FALSE)
  {
    //calculate PFM date
    tA = ((225 - 11 * nRemain19) % 30) + 21;
    //find the next Sunday
    tB = (tA - 19) % 7;
    tC = (40 - nFirstDig) % 7;
    nTemp = nYear % 100;
    tD = (nTemp + (nTemp / 4)) % 7;
    tE = ((20 - tB - tC - tD) % 7) + 1;
    *lpnDay = tA + tE;
    //convert Julian to Gregorian date
    //10 nDays were 'skipped' in the Gregorian calendar from 5-14 Oct 1582
    nTemp = 10;
    //Only 1 in every 4 century years are leap years in the Gregorian
    //calendar (every century is a leap year in the Julian calendar)
    if (nYear > 1600)
      nTemp += nFirstDig - 16 - ((nFirstDig - 16) / 4);
    *lpnDay += nTemp;
  }
  else
  {
    //calculate PFM date
    nTemp = ((nFirstDig - 15) / 2) + 202 - 11 * nRemain19;
    switch (nFirstDig)
    {
      case 21:
      case 24:
      case 25:
      case 27:
      case 28:
      case 29:
      case 30:
      case 31:
      case 32:
      case 34:
      case 35:
      case 38:
        nTemp--;
        break;
      case 33:
      case 36:
      case 37:
      case 39:
      case 40:
        nTemp -= 2;
        break;
    }
    nTemp %= 30;
    tA = nTemp + 21;
    if (nTemp == 29 || (nTemp == 28 && nRemain19 > 10))
      tA--;
    //find the next Sunday
    tB = (tA - 19) % 7;
    tC = (40 - nFirstDig) & 3;
    if (tC == 3)
      tC++;
    if (tC > 1)
      tC++;
    nTemp = nYear % 100;
    tD = (nTemp + (nTemp / 4)) % 7;
    tE = ((20 - tB - tC - tD) % 7) + 1;
    *lpnDay = tA + tE;
  }
  //return the date
  if (*lpnDay > 61)
  {
    *lpnDay -= 61;
    *lpnMonth = 5; //for method 2, Easter Sunday can occur in May
  }
  else if (*lpnDay > 31)
  {
    *lpnDay -= 31;
    *lpnMonth = 4;
  }
  else
  {
    *lpnMonth = 3;
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT Format_AddNumber(_Inout_ MX::CStringW &cStrDestW, _In_ SIZE_T nNumber, _In_ SIZE_T nMinDigits)
{
  static const LPCWSTR szZeroesW = L"00000000";
  SIZE_T k, nTotalDigits;
  WCHAR aDigitsW[64];

  nTotalDigits = 0;
  k = nNumber;
  while (nTotalDigits < MX_ARRAYLEN(aDigitsW))
  {
    nTotalDigits++;
    aDigitsW[MX_ARRAYLEN(aDigitsW)-nTotalDigits] = L'0' + (WCHAR)(k % 10);
    if ((k /= 10) == 0)
      break;
  }
  if (nTotalDigits >= MX_ARRAYLEN(aDigitsW))
    return MX_E_ArithmeticOverflow;
  k = nTotalDigits;
  if (nTotalDigits < nMinDigits)
  {
    for (k=nMinDigits-nTotalDigits; k>=8; k-=8)
    {
      if (cStrDestW.Concat(szZeroesW) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (k > 0 && cStrDestW.ConcatN(szZeroesW, k) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (cStrDestW.ConcatN(&aDigitsW[MX_ARRAYLEN(aDigitsW)-nTotalDigits], nTotalDigits) == FALSE)
    return E_OUTOFMEMORY;
  return S_OK;
}

static HRESULT DoTwoDigitsYearAdjustment(_Inout_ int &nYear, _In_ int nTwoDigitsYearRule)
{
  int i, k;

  MX_ASSERT(nYear < 100);
  MX_ASSERT(nTwoDigitsYearRule==0 || nTwoDigitsYearRule>=101);
  if (nTwoDigitsYearRule == 0)
  {
    DWORD dw;

    if (::GetCalendarInfoW(LOCALE_USER_DEFAULT, CAL_GREGORIAN, CAL_NOUSEROVERRIDE | CAL_ITWODIGITYEARMAX |
                           CAL_RETURN_NUMBER, NULL, 0, &dw) != 0 &&
        dw >= 101)
    {
      nTwoDigitsYearRule = (int)dw;
    }
    else
    {
      //cannot get calendar so get current date and add 20 years
      MX::CDateTime cDt;
      HRESULT hRes;

      hRes = cDt.SetFromNow(TRUE);
      if (FAILED(hRes))
        return hRes;
      nTwoDigitsYearRule = cDt.GetYear() + 20;
    }
  }
  //use custom rule
  k = (nTwoDigitsYearRule % 100);
  i = nTwoDigitsYearRule - k;
  if (nYear > k)
    i -= 100;
  nYear += i;
  return S_OK;
}

static VOID TicksToYearAndDayOfYear(_In_ const MX::CTimeSpan &cTicks, _Out_opt_ int *lpnYear,
                                    _Out_opt_ int *lpnDayOfYear)
{
  int nTotalDays, nYears400, nDays400, nYears100, nDays100, nYears4, nDays4, nYears1;

  nTotalDays = cTicks.GetDays();
  nYears400 = nTotalDays / 146097;
  nDays400 = nTotalDays % 146097;
  nYears100 = nDays400 / 36524;
  nDays100 =  nDays400 % 36524;
  nYears4 = nDays100 / 1461;
  nDays4 = nDays100 % 1461;
  nYears1 = nDays4 / 365;
  if (lpnYear != NULL)
    *lpnYear = (int)(400*nYears400 + 100*nYears100 + 4*nYears4 + nYears1);
  if (nYears100==4 || nYears1==4)
  {
    if (lpnDayOfYear != NULL)
      *lpnDayOfYear = 366;
  }
  else
  {
    if (lpnDayOfYear != NULL)
      *lpnDayOfYear = (nDays4 % 365) + 1;
    if (lpnYear != NULL)
      (*lpnYear)++;
  }
  return;
}

static BOOL ConvertCustomSettingsA2W(_Out_ MX::CDateTime::LPCUSTOMSETTINGSW lpCustomW,
                                     _In_ MX::CDateTime::LPCUSTOMSETTINGSA lpCustomA)
{
  int i;

  MxMemSet(lpCustomW, 0, sizeof(MX::CDateTime::CUSTOMSETTINGSW));
  for (i=0; i<7; i++)
  {
    if (lpCustomA->szShortDayNamesA[i] != NULL)
    {
      lpCustomW->szShortDayNamesW[i] = Ansi2Wide(lpCustomA->szShortDayNamesA[i]);
      if (lpCustomW->szShortDayNamesW[i] == NULL)
      {
onerr:  FreeCustomSettings(lpCustomW);
        return FALSE;
      }
    }
    if (lpCustomA->szLongDayNamesA[i] != NULL)
    {
      lpCustomW->szLongDayNamesW[i] = Ansi2Wide(lpCustomA->szLongDayNamesA[i]);
      if (lpCustomW->szLongDayNamesW[i] == NULL)
        goto onerr;
    }
  }
  for (i=0; i<12; i++)
  {
    if (lpCustomA->szShortMonthNamesA[i] != NULL)
    {
      lpCustomW->szShortMonthNamesW[i] = Ansi2Wide(lpCustomA->szShortMonthNamesA[i]);
      if (lpCustomW->szShortMonthNamesW[i] == NULL)
        goto onerr;
    }
    if (lpCustomA->szLongMonthNamesA[i] != NULL)
    {
      lpCustomW->szLongMonthNamesW[i] = Ansi2Wide(lpCustomA->szLongMonthNamesA[i]);
      if (lpCustomW->szLongMonthNamesW[i] == NULL)
        goto onerr;
    }
  }
  for (i=0; i<2; i++)
  {
    if (lpCustomA->szTimeAmPmA[i] != NULL)
    {
      lpCustomW->szTimeAmPmW[i] = Ansi2Wide(lpCustomA->szTimeAmPmA[i]);
      if (lpCustomW->szTimeAmPmW[i] == NULL)
        goto onerr;
    }
  }
  if (lpCustomA->szDateSeparatorA != NULL)
  {
    lpCustomW->szDateSeparatorW = Ansi2Wide(lpCustomA->szDateSeparatorA);
    if (lpCustomW->szDateSeparatorW == NULL)
      goto onerr;
  }
  if (lpCustomA->szTimeSeparatorA != NULL)
  {
    lpCustomW->szTimeSeparatorW = Ansi2Wide(lpCustomA->szTimeSeparatorA);
    if (lpCustomW->szTimeSeparatorW == NULL)
      goto onerr;
  }
  if (lpCustomA->szShortDateFormatA != NULL)
  {
    lpCustomW->szShortDateFormatW = Ansi2Wide(lpCustomA->szShortDateFormatA);
    if (lpCustomW->szShortDateFormatW == NULL)
      goto onerr;
  }
  if (lpCustomA->szLongDateFormatA != NULL)
  {
    lpCustomW->szLongDateFormatW = Ansi2Wide(lpCustomA->szLongDateFormatA);
    if (lpCustomW->szLongDateFormatW == NULL)
      goto onerr;
  }
  if (lpCustomA->szTimeFormatA != NULL)
  {
    lpCustomW->szTimeFormatW = Ansi2Wide(lpCustomA->szTimeFormatA);
    if (lpCustomW->szTimeFormatW == NULL)
      goto onerr;
  }
  return TRUE;
}

static VOID FreeCustomSettings(_In_ MX::CDateTime::LPCUSTOMSETTINGSW lpCustomW)
{
  int i;

  for (i=0; i<7; i++)
  {
    MX_FREE(lpCustomW->szShortDayNamesW[i]);
    MX_FREE(lpCustomW->szLongDayNamesW[i]);
  }
  for (i=0; i<12; i++)
  {
    MX_FREE(lpCustomW->szShortMonthNamesW[i]);
    MX_FREE(lpCustomW->szLongMonthNamesW[i]);
  }
  for (i=0; i<2; i++)
  {
    MX_FREE(lpCustomW->szTimeAmPmW[i]);
  }
  MX_FREE(lpCustomW->szDateSeparatorW);
  MX_FREE(lpCustomW->szTimeSeparatorW);
  MX_FREE(lpCustomW->szShortDateFormatW);
  MX_FREE(lpCustomW->szLongDateFormatW);
  MX_FREE(lpCustomW->szTimeFormatW);
  return;
}

static VOID FixCustomSettings(_Out_ MX::CDateTime::LPCUSTOMSETTINGSW lpOut, _In_ MX::CDateTime::LPCUSTOMSETTINGSW lpIn)
{
  static LPCWSTR szShortDayNamesW[7] = {
    L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
  };
  static LPCWSTR szLongDayNamesW[7] = {
    L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday"
  };
  static LPCWSTR szShortMonthNamesW[12] = {
    L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
  };
  static LPCWSTR szLongMonthNamesW[12] = {
    L"January", L"February", L"March", L"April", L"May", L"June",
    L"July", L"August", L"September", L"October", L"November", L"December"
  };
  static LPCWSTR szTimeAmPmW[2] = { L"AM", L"PM" };
  int i;

  if (lpIn != NULL)
    ::MxMemCopy(lpOut, lpIn, sizeof(MX::CDateTime::CUSTOMSETTINGSW));
  else
    ::MxMemSet(lpOut, 0, sizeof(MX::CDateTime::CUSTOMSETTINGSW));
  for (i=0; i<7; i++)
  {
    if (lpOut->szShortDayNamesW[i] == NULL)
      lpOut->szShortDayNamesW[i] = szShortDayNamesW[i];
    if (lpOut->szLongDayNamesW[i] == NULL)
      lpOut->szLongDayNamesW[i] = szLongDayNamesW[i];
  }
  for (i=0; i<12; i++)
  {
    if (lpOut->szShortMonthNamesW[i] == NULL)
      lpOut->szShortMonthNamesW[i] = szShortMonthNamesW[i];
    if (lpOut->szLongMonthNamesW[i] == NULL)
      lpOut->szLongMonthNamesW[i] = szLongMonthNamesW[i];
  }
  for (i=0; i<2; i++)
  {
    if (lpOut->szTimeAmPmW[i] == NULL)
      lpOut->szTimeAmPmW[i] = szTimeAmPmW[i];
  }
  if (lpOut->szDateSeparatorW == NULL)
    lpOut->szDateSeparatorW = L"/";
  if (lpOut->szTimeSeparatorW == NULL)
    lpOut->szTimeSeparatorW = L":";
  if (lpOut->szShortDateFormatW == NULL)
    lpOut->szShortDateFormatW = L"%m/%d/%y";
  if (lpOut->szLongDateFormatW == NULL)
    lpOut->szLongDateFormatW = L"%A, %B %#d, %Y";
  if (lpOut->szTimeFormatW == NULL)
    lpOut->szTimeFormatW = L"%I:%M:%S %p";
  return;
}

static HRESULT GetRoundedMillisecondsForOA(_In_ double nMs, _Out_ LONGLONG *lpnRes)
{
  LONGLONG nTicks;
  double nTempDbl;

  nTempDbl = nMs * 86400000.0;
  if (nMs >= 0.0)
  {
    if (nTempDbl < nMs)
      return MX_E_ArithmeticOverflow;
  }
  else
  {
    if (nTempDbl > nMs)
      return MX_E_ArithmeticOverflow;
  }
  nTicks = (LONGLONG)(nTempDbl + (nTempDbl >= 0.0) ? 0.5 : (-0.5));
  *lpnRes = nTicks * MX_DATETIME_TICKS_PER_MILLISECOND;
  if (nTicks >= 0)
  {
    if ((*lpnRes) < nTicks)
      return MX_E_ArithmeticOverflow;
  }
  else
  {
    if ((*lpnRes) > nTicks)
      return MX_E_ArithmeticOverflow;
  }
  return S_OK;
}

static LPWSTR Ansi2Wide(__in_nz_opt LPCSTR szSrcA)
{
  MX::CStringW cStrTempW;

  if (szSrcA != NULL && szSrcA[0] != 0)
  {
    if (cStrTempW.Copy(szSrcA) == FALSE)
      return NULL;
  }
  else
  {
    if (cStrTempW.EnsureBuffer(1) == FALSE)
      return NULL;
  }
  return cStrTempW.Detach();
}

static SIZE_T CompareStrW(_In_z_ LPCWSTR szStrW, _In_z_ LPCWSTR szCompareToW)
{
  SIZE_T nLen;

  nLen = MX::StrLenW(szCompareToW);
  if (MX::StrNCompareW(szStrW, szCompareToW, nLen, TRUE) != 0 ||
      ((szStrW[nLen] >= L'A' && szStrW[nLen] <= L'Z') || (szStrW[nLen] >= L'a' && szStrW[nLen] <= L'z')))
    return 0;
  return nLen;
}

static int FixGmtOffset(_In_ int nGmtOffset)
{
  if (nGmtOffset < -12*60)
    nGmtOffset = -12*60;
  else if (nGmtOffset > 14*60)
    nGmtOffset = 14*60;
  return nGmtOffset;
}

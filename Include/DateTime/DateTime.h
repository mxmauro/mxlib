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
#ifndef _MX_DATETIME_H
#define _MX_DATETIME_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

#define MX_DATETIME_TICKS_PER_DAY            864000000000i64
#define MX_DATETIME_TICKS_PER_HOUR            36000000000i64
#define MX_DATETIME_TICKS_PER_MILLISECOND           10000i64
#define MX_DATETIME_TICKS_PER_MINUTE            600000000i64
#define MX_DATETIME_TICKS_PER_SECOND             10000000i64

#define MX_DATETIME_EPOCH_TICKS        621355968000000000i64

//-----------------------------------------------------------

//%a: Abbreviated weekday name.
//%A: Full weekday name.
//%b: Abbreviated month name.
//%B: Full month name.
//%c: Date and time representation appropriate for locale.
//%d: Day of month as decimal number (01 - 31).
//%H: Hour in 24-hour format (00 - 23).
//%I: Hour in 12-hour format (01 - 12).
//%j: Day of year as decimal number (001 - 366).
//%m: Month as decimal number (01 - 12).
//%M: Minute as decimal number (00 - 59).
//%p: Current locale's A.M./P.M. indicator for 12-hour clock.
//%S: Second as decimal number (00 - 59).
//%f: Milliseconds as decimal number (0 - 999).
//%U: Week of year as decimal number, with Sunday as first day of week (00 - 53).
//%w: Weekday as decimal number (0 - 6; Sunday is 0).
//%W: Week of year as decimal number, with Monday as first day of week (00 - 53).
//%x: Date representation for current locale.
//%X: Time representation for current locale.
//%y: Year without century, as decimal number (00 - 99).
//%Y: Year with century, as decimal number.
//%z: Either the time-zone name or time zone abbreviation. For output, GMT string is printed if offset is zero, else
//    [+/-]#### format is used.
//%%: Percent sign.
//
//The # flag may prefix any formatting code:
//
//%#c: Long date and time representation, appropriate for current locale. I.e.: "Tuesday, March 14, 1995, 12:41:29".
//%#x: Long date representation, appropriate to current locale. I.e.: "Tuesday, March 14, 1995".
//%#d, %#H, %#I, %#j, %#m, %#M, %#S, %#U, %#w, %#W, %#y, %#Y: Remove leading zeros (if any).
//%#z: Prints +0000 instead of GMT string if GMT offset is zero.

//-----------------------------------------------------------

namespace MX {

class CTimeSpan : public virtual CBaseMemObj
{
public:
  CTimeSpan(_In_ const CTimeSpan& cSrc);
  CTimeSpan(_In_opt_ LONGLONG nValue=0);
  CTimeSpan(_In_ int nDays, _In_ int nHours, _In_ int nMinutes, _In_ int nSeconds, _In_opt_ int nMilliSeconds=0);
  CTimeSpan(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds);

  int GetDays() const;
  int GetHours() const;
  int GetMinutes() const;
  int GetSeconds() const;
  int GetMilliSeconds() const;
  LONGLONG GetTicks() const
    {
    return nTicks;
    };
  LONGLONG GetDuration() const;

  double GetTotalDays() const;
  double GetTotalHours() const;
  double GetTotalMinutes() const;
  double GetTotalSeconds() const;
  double GetTotalMilliSeconds() const;

  HRESULT Add(_In_ LONGLONG nValue);
  HRESULT Add(_In_ CTimeSpan &cTs);
  HRESULT Sub(_In_ LONGLONG nValue);
  HRESULT Sub(_In_ CTimeSpan &cTs);

  HRESULT SetFrom(_In_ double nValue, _In_ LONGLONG nTickMultiplicator);
  HRESULT SetFromDays(_In_ double nValue);
  HRESULT SetFromHours(_In_ double nValue);
  HRESULT SetFromMinutes(_In_ double nValue);
  HRESULT SetFromSeconds(_In_ double nValue);
  HRESULT SetFromMilliSeconds(_In_ double nValue);
  HRESULT SetFromTicks(_In_ LONGLONG nValue);
  HRESULT SetFrom(_In_ int nDays, _In_ int nHours, _In_ int nMinutes, _In_ int nSeconds, _In_opt_ int nMilliSeconds=0);
  HRESULT SetFrom(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds);

  HRESULT Negate();

  HRESULT Parse(_In_z_ LPCSTR szSrcA, _In_ SIZE_T nLength=(SIZE_T)-1, _Out_opt_ LPSTR *lpszEndStringA=NULL);
  HRESULT Parse(_In_z_ LPCWSTR szSrcW, _In_ SIZE_T nLength=(SIZE_T)-1, _Out_opt_ LPWSTR *lpszEndStringW=NULL);

  HRESULT Format(_Inout_ CStringW& cStrDestW);
  HRESULT AppendFormat(_Inout_ CStringW& cStrDestW);

  CTimeSpan& operator=(_In_ const CTimeSpan& cSrc);

  bool operator==(_In_ const CTimeSpan& cTs) const;
  bool operator!=(_In_ const CTimeSpan& cTs) const;
  bool operator<(_In_ const CTimeSpan& cTs) const;
  bool operator>(_In_ const CTimeSpan& cTs) const;
  bool operator<=(_In_ const CTimeSpan& cTs) const;
  bool operator>=(_In_ const CTimeSpan& cTs) const;

private:
  LONGLONG nTicks;
};

//-----------------------------------------------------------

class CDateTime : public virtual CBaseMemObj
{
public:
  typedef enum {
    UnitsYear=1,
    UnitsMonth,
    UnitsDay,
    UnitsHours,
    UnitsMinutes,
    UnitsSeconds,
    UnitsMilliseconds,
    UnitsTicks,
  } eUnits;

  typedef struct {
    LPCSTR szShortDayNamesA[7];
    LPCSTR szLongDayNamesA[7];
    LPCSTR szShortMonthNamesA[12];
    LPCSTR szLongMonthNamesA[12];
    LPCSTR szTimeAmPmA[2];
    LPCSTR szDateSeparatorA;
    LPCSTR szTimeSeparatorA;
    LPCSTR szShortDateFormatA;
    LPCSTR szLongDateFormatA;
    LPCSTR szTimeFormatA;
  } CUSTOMSETTINGSA, *LPCUSTOMSETTINGSA;

  typedef struct {
    LPCWSTR szShortDayNamesW[7];
    LPCWSTR szLongDayNamesW[7];
    LPCWSTR szShortMonthNamesW[12];
    LPCWSTR szLongMonthNamesW[12];
    LPCWSTR szTimeAmPmW[2];
    LPCWSTR szDateSeparatorW;
    LPCWSTR szTimeSeparatorW;
    LPCWSTR szShortDateFormatW;
    LPCWSTR szLongDateFormatW;
    LPCWSTR szTimeFormatW;
  } CUSTOMSETTINGSW, *LPCUSTOMSETTINGSW;

public:
  CDateTime(_In_ const CDateTime& cSrc);
  CDateTime(_In_opt_ LONGLONG nTicks = 0, _In_opt_ int nGmtOffset = 0);
  CDateTime(_In_ CTimeSpan &cTs, _In_opt_ int nGmtOffset = 0);
  CDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_opt_ int nGmtOffset = 0);
  CDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_ int nHours, _In_ int nMinutes, _In_ int nSeconds,
            _In_opt_ int nMilliSeconds=0, _In_opt_ int nGmtOffset=0);
  ~CDateTime();

  VOID Clear();

  HRESULT SetDate(_In_ int nYear, _In_ int nMonth, _In_ int nDay);
  HRESULT SetTime(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds, _In_opt_ int nMilliSeconds=0);
  HRESULT SetDateTime(_In_ int nYear, _In_ int nMonth, _In_ int nDay, _In_ int nHours, _In_ int nMinutes,
                      _In_ int nSeconds, _In_opt_ int nMilliSeconds=0);
  HRESULT SetFromTicks(_In_ LONGLONG nTicks);
  HRESULT SetFromSystemTime(_In_ const SYSTEMTIME& sSrcSysTime);
  HRESULT SetFromFileTime(_In_ const FILETIME& sSrcFileTime);
  HRESULT SetFromString(_In_z_ LPCSTR szDateA, _In_z_ LPCSTR szFormatA, _In_opt_ LPCUSTOMSETTINGSA lpCustomA = NULL,
                        _In_opt_ int nTwoDigitsYearRule = 0); //0=use OS
  HRESULT SetFromString(_In_z_ LPCWSTR szDateW, _In_z_ LPCWSTR szFormatW, _In_opt_ LPCUSTOMSETTINGSW lpCustomW = NULL,
                        _In_opt_ int nTwoDigitsYearRule = 0); //0=use OS
  HRESULT SetFromNow(_In_ BOOL bLocal);
  HRESULT SetFromUnixTime(_In_ int nTime);
  HRESULT SetFromUnixTime(_In_ LONGLONG nTime);
  HRESULT SetFromOleAutDate(_In_ double nValue);
  HRESULT SetGmtOffset(_In_ int nGmtOffset, _In_ BOOL bAdjustTime=FALSE);

  VOID GetDate(_Out_ int *lpnYear, _Out_ int *lpnMonth, _Out_ int *lpnDay);
  VOID GetTime(_Out_ int *lpnHours, _Out_ int *lpnMinutes, _Out_ int *lpnSeconds, _Out_opt_ int *lpnMilliSeconds=NULL);
  VOID GetDateTime(_Out_ int *lpnYear, _Out_ int *lpnMonth, _Out_ int *lpnDay, _Out_ int *lpnHours,
                   _Out_ int *lpnMinutes, _Out_ int *lpnSeconds, _Out_opt_ int *lpnMilliSeconds=NULL);
  VOID GetSystemTime(_Out_ SYSTEMTIME &sSysTime);
  HRESULT GetFileTime(_Out_ FILETIME &sFileTime);
  HRESULT GetUnixTime(_Out_ int *lpnTime);
  HRESULT GetUnixTime(_Out_ LONGLONG *lpnTime);
  HRESULT GetOleAutDate(_Out_ double *lpnValue);

  LONGLONG GetTicks() const;
  int GetYear() const;
  int GetMonth() const;
  int GetDay() const;
  int GetHours() const;
  int GetMinutes() const;
  int GetSeconds() const;
  int GetMilliSeconds() const;
  int GetDayOfWeek() const;
  int GetDayOfYear() const;
  VOID GetTimeOfDay(_Out_ CTimeSpan &cDestTs);
  int GetGmtOffset() const;

  BOOL IsLeapYear() const;

  CDateTime& operator=(_In_ const CDateTime& cSrc);

  bool operator==(_In_ const CDateTime& cDt) const;
  bool operator!=(_In_ const CDateTime& cDt) const;
  bool operator<(_In_ const CDateTime& cDt) const;
  bool operator>(_In_ const CDateTime& cDt) const;
  bool operator<=(_In_ const CDateTime& cDt) const;
  bool operator>=(_In_ const CDateTime& cDt) const;

  HRESULT Add(_In_ CTimeSpan &cTs);
  HRESULT Add(_In_ int nCount, _In_ eUnits nUnits);
  HRESULT Add(_In_ LONGLONG nCount, _In_ eUnits nUnits);
  HRESULT Sub(_In_ CTimeSpan &cTs);
  HRESULT Sub(_In_ int nCount, _In_ eUnits nUnits);
  HRESULT Sub(_In_ LONGLONG nCount, _In_ eUnits nUnits);

  LONGLONG GetDiff(_In_ const CDateTime& cFromDt, _In_ eUnits nUnits=UnitsSeconds);

  HRESULT Format(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szFormatA, _In_opt_ LPCUSTOMSETTINGSA lpCustomA=NULL);
  HRESULT Format(_Inout_ CStringW &cStrDestW, _In_z_ LPCWSTR szFormatW, _In_opt_ LPCUSTOMSETTINGSW lpCustomW=NULL);
  HRESULT AppendFormat(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szFormatA,
                       _In_opt_ LPCUSTOMSETTINGSA lpCustomA=NULL);
  HRESULT AppendFormat(_Inout_ CStringW &cStrDestW, _In_z_ LPCWSTR szFormatW,
                       _In_opt_ LPCUSTOMSETTINGSW lpCustomW=NULL);

  static BOOL IsDateValid(_In_ int nYear, _In_ int nMonth, _In_ int nDay);
  static BOOL IsTimeValid(_In_ int nHours, _In_ int nMinutes, _In_ int nSeconds);
  static BOOL IsLeapYear(_In_ int nYear);

  static int GetDaysInMonth(_In_ int nMonth, _In_ int nYear);
  static int GetDayOfWeek(_In_ int nYear, _In_ int nMonth, _In_ int nDay);
  static int GetDayOfYear(_In_ int nYear, _In_ int nMonth, _In_ int nDay);
  static int GetAbsoluteDay(_In_ int nYear, _In_ int nMonth, _In_ int nDay);
  static HRESULT CalculateDayMonth(_In_ int nDayOfYear, _In_ int nYear, _Out_ int *lpnMonth, _Out_ int *lpnDay);
  static HRESULT CalculateEasterInYear(_In_ int nYear, _Out_ int *lpnMonth, _Out_ int *lpnDay,
                                       _In_ BOOL bOrthodoxChurchesMethod);

private:
  CTimeSpan cTicks;
  int nGmtOffset; //in minutes
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_DATETIME_H

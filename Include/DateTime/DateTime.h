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
  CTimeSpan(__in const CTimeSpan& cSrc);
  CTimeSpan(__in_opt LONGLONG nValue=0);
  CTimeSpan(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds, __in_opt int nMilliSeconds=0);
  CTimeSpan(__in int nHours, __in int nMinutes, __in int nSeconds);

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

  HRESULT Add(__in LONGLONG nValue);
  HRESULT Add(__in CTimeSpan &cTs);
  HRESULT Sub(__in LONGLONG nValue);
  HRESULT Sub(__in CTimeSpan &cTs);

  HRESULT SetFrom(__in double nValue, __in LONGLONG nTickMultiplicator);
  HRESULT SetFromDays(__in double nValue);
  HRESULT SetFromHours(__in double nValue);
  HRESULT SetFromMinutes(__in double nValue);
  HRESULT SetFromSeconds(__in double nValue);
  HRESULT SetFromMilliSeconds(__in double nValue);
  HRESULT SetFromTicks(__in LONGLONG nValue);
  HRESULT SetFrom(__in int nDays, __in int nHours, __in int nMinutes, __in int nSeconds, __in_opt int nMilliSeconds=0);
  HRESULT SetFrom(__in int nHours, __in int nMinutes, __in int nSeconds);

  HRESULT Negate();

  HRESULT Parse(__in_z LPCSTR szSrcA, __in SIZE_T nLength=(SIZE_T)-1, __out_opt LPSTR *lpszEndStringA=NULL);
  HRESULT Parse(__in_z LPCWSTR szSrcW, __in SIZE_T nLength=(SIZE_T)-1, __out_opt LPWSTR *lpszEndStringW=NULL);

  HRESULT Format(__inout CStringW& cStrDestW);
  HRESULT AppendFormat(__inout CStringW& cStrDestW);

  CTimeSpan& operator=(__in const CTimeSpan& cSrc);

  bool operator==(__in const CTimeSpan& cTs) const;
  bool operator!=(__in const CTimeSpan& cTs) const;
  bool operator<(__in const CTimeSpan& cTs) const;
  bool operator>(__in const CTimeSpan& cTs) const;
  bool operator<=(__in const CTimeSpan& cTs) const;
  bool operator>=(__in const CTimeSpan& cTs) const;

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
  CDateTime(__in const CDateTime& cSrc);
  CDateTime(__in_opt LONGLONG nTicks=0, __in_opt int nGmtOffset=0);
  CDateTime(__in CTimeSpan &cTs, __in_opt int nGmtOffset=0);
  CDateTime(__in int nYear, __in int nMonth, __in int nDay, __in_opt int nGmtOffset=0);
  CDateTime(__in int nYear, __in int nMonth, __in int nDay, __in int nHours, __in int nMinutes, __in int nSeconds,
            __in_opt int nMilliSeconds=0, __in_opt int nGmtOffset=0);
  ~CDateTime();

  VOID Clear();

  HRESULT SetDate(__in int nYear, __in int nMonth, __in int nDay);
  HRESULT SetTime(__in int nHours, __in int nMinutes, __in int nSeconds, __in_opt int nMilliSeconds=0);
  HRESULT SetDateTime(__in int nYear, __in int nMonth, __in int nDay, __in int nHours, __in int nMinutes,
                      __in int nSeconds, __in_opt int nMilliSeconds=0);
  HRESULT SetFromTicks(__in LONGLONG nTicks);
  HRESULT SetFromSystemTime(__in const SYSTEMTIME& sSrcSysTime);
  HRESULT SetFromFileTime(__in const FILETIME& sSrcFileTime);
  HRESULT SetFromString(__in_z LPCSTR szDateA, __in_z LPCSTR szFormatA, __in_opt LPCUSTOMSETTINGSA lpCustomA=NULL,
                        __in_opt int nTwoDigitsYearRule=0); //0=use OS
  HRESULT SetFromString(__in_z LPCWSTR szDateW, __in_z LPCWSTR szFormatW, __in_opt LPCUSTOMSETTINGSW lpCustomW=NULL,
                        __in_opt int nTwoDigitsYearRule=0); //0=use OS
  HRESULT SetFromNow(__in BOOL bLocal);
  HRESULT SetFromUnixTime(__in int nTime);
  HRESULT SetFromUnixTime(__in LONGLONG nTime);
  HRESULT SetFromOleAutDate(__in double nValue);
  HRESULT SetGmtOffset(__in int nGmtOffset, __in BOOL bAdjustTime=FALSE);

  VOID GetDate(__out int *lpnYear, __out int *lpnMonth, __out int *lpnDay);
  VOID GetTime(__out int *lpnHours, __out int *lpnMinutes, __out int *lpnSeconds, __out_opt int *lpnMilliSeconds=NULL);
  VOID GetDateTime(__out int *lpnYear, __out int *lpnMonth, __out int *lpnDay, __out int *lpnHours,
                   __out int *lpnMinutes, __out int *lpnSeconds, __out_opt int *lpnMilliSeconds=NULL);
  VOID GetSystemTime(__out SYSTEMTIME &sSysTime);
  HRESULT GetFileTime(__out FILETIME &sFileTime);
  HRESULT GetUnixTime(__out int *lpnTime);
  HRESULT GetUnixTime(__out LONGLONG *lpnTime);
  HRESULT GetOleAutDate(__out double *lpnValue);

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
  VOID GetTimeOfDay(__out CTimeSpan &cDestTs);
  int GetGmtOffset() const;

  BOOL IsLeapYear() const;

  CDateTime& operator=(__in const CDateTime& cSrc);

  bool operator==(__in const CDateTime& cDt) const;
  bool operator!=(__in const CDateTime& cDt) const;
  bool operator<(__in const CDateTime& cDt) const;
  bool operator>(__in const CDateTime& cDt) const;
  bool operator<=(__in const CDateTime& cDt) const;
  bool operator>=(__in const CDateTime& cDt) const;

  HRESULT Add(__in CTimeSpan &cTs);
  HRESULT Add(__in int nCount, __in eUnits nUnits);
  HRESULT Add(__in LONGLONG nCount, __in eUnits nUnits);
  HRESULT Sub(__in CTimeSpan &cTs);
  HRESULT Sub(__in int nCount, __in eUnits nUnits);
  HRESULT Sub(__in LONGLONG nCount, __in eUnits nUnits);

  LONGLONG GetDiff(__in const CDateTime& cFromDt, __in eUnits nUnits=UnitsSeconds);

  HRESULT Format(__inout CStringA &cStrDestA, __in_z LPCSTR szFormatA, __in_opt LPCUSTOMSETTINGSA lpCustomA=NULL);
  HRESULT Format(__inout CStringW &cStrDestW, __in_z LPCWSTR szFormatW, __in_opt LPCUSTOMSETTINGSW lpCustomW=NULL);
  HRESULT AppendFormat(__inout CStringA &cStrDestA, __in_z LPCSTR szFormatA,
                       __in_opt LPCUSTOMSETTINGSA lpCustomA=NULL);
  HRESULT AppendFormat(__inout CStringW &cStrDestW, __in_z LPCWSTR szFormatW,
                       __in_opt LPCUSTOMSETTINGSW lpCustomW=NULL);

  static BOOL IsDateValid(__in int nYear, __in int nMonth, __in int nDay);
  static BOOL IsTimeValid(__in int nHours, __in int nMinutes, __in int nSeconds);
  static BOOL IsLeapYear(__in int nYear);

  static int GetDaysInMonth(__in int nMonth, __in int nYear);
  static int GetDayOfWeek(__in int nYear, __in int nMonth, __in int nDay);
  static int GetDayOfYear(__in int nYear, __in int nMonth, __in int nDay);
  static int GetAbsoluteDay(__in int nYear, __in int nMonth, __in int nDay);
  static HRESULT CalculateDayMonth(__in int nDayOfYear, __in int nYear, __out int *lpnMonth, __out int *lpnDay);
  static HRESULT CalculateEasterInYear(__in int nYear, __out int *lpnMonth, __out int *lpnDay,
                                       __in BOOL bOrthodoxChurchesMethod);

private:
  CTimeSpan cTicks;
  int nGmtOffset; //in minutes
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_DATETIME_H

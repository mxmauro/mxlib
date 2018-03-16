/**
 * Copyright (C) 2011 by Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * C++ adaptation by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 */
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\ArrayList.h"

//-----------------------------------------------------------

// punycode parameters, see http://tools.ietf.org/html/rfc3492#section-5
#define BASE 36
#define TMIN 1
#define TMAX 26
#define SKEW 38
#define DAMP 700
#define INITIAL_N 128
#define INITIAL_BIAS 72

#if defined(_M_IX86)
  #define __SIZE_T_MAX 0xFFFFFFFFUL
#elif defined(_M_X64)
  #define __SIZE_T_MAX 0xFFFFFFFFFFFFFFFFui64
#else
  #error Platform not supported
#endif

//-----------------------------------------------------------

SIZE_T GetWideChar(_In_ LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);
static SIZE_T adapt_bias(_In_ SIZE_T delta, _In_ SIZE_T n_points, _In_ bool is_first);
static CHAR encode_digit(_In_ SIZE_T c);
static HRESULT encode_var_int(_In_ SIZE_T bias, _In_ SIZE_T delta, _Inout_ MX::CStringA &cStrDestA);
static SIZE_T decode_digit(_In_ CHAR v);

//-----------------------------------------------------------

namespace MX {

HRESULT Punycode_Encode(_Inout_ CStringA &cStrDestA, _In_z_ LPCWSTR szSrcW, _In_opt_ SIZE_T nSrcLen)
{
  SIZE_T b, h, delta, bias, m, n, si, chW, nHyphenInsertionPoint;
  HRESULT hRes;

  cStrDestA.Empty();
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenW(szSrcW);
  if (szSrcW == NULL && nSrcLen > 0)
    return E_POINTER;
  for (si=0; si<nSrcLen; si++)
  {
    chW = GetWideChar(szSrcW+si, nSrcLen-si);
    if (chW == 0)
      return 0;
    if (chW < 128)
    {
      CHAR _ch = (CHAR)chW;
      if (cStrDestA.ConcatN(&_ch, 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (chW >= 0x10000)
      si++;
  }
  b = h = nHyphenInsertionPoint = cStrDestA.GetLength();

  n = INITIAL_N;
  bias = INITIAL_BIAS;
  delta = 0;
  for (; h<nSrcLen; n++, delta++)
  {
    //find next smallest non-basic code point
    m = __SIZE_T_MAX;
    for (si=0; si<nSrcLen; si++)
    {
      chW = GetWideChar(szSrcW+si, nSrcLen-si);
      if (chW >= n && chW < m)
        m = chW;
      if (chW >= 0x10000)
        si++;
    }

    if ((m - n) > (__SIZE_T_MAX - delta) / (h + 1))
      return E_FAIL; //overflow

    delta += (m - n) * (h + 1);
    n = m;
    for (si=0; si<nSrcLen; si++)
    {
      chW = GetWideChar(szSrcW+si, nSrcLen-si);
      if (chW >= 0x10000)
        si++;
      if (chW < n)
      {
        if ((++delta) == 0)
          return E_FAIL; //overflow
      }
      else if (chW == n)
      {
        hRes = encode_var_int(bias, delta, cStrDestA);
        if (FAILED(hRes))
          return hRes;
        bias = adapt_bias(delta, h+1, h==b);
        delta = 0;
        h++;
      }
    }
  }

  //write out delimiter if any basic code points were processed
  if (cStrDestA.GetLength() > nHyphenInsertionPoint)
  {
    if (cStrDestA.InsertN("-", nHyphenInsertionPoint, 1) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT Punycode_Decode(_Inout_ CStringW &cStrDestW, _In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen)
{
  SIZE_T n, t, i, k, w, si, digit, org_i, bias;
  LPCSTR p;
  TArrayList<ULONG> aCodepoints;
  WCHAR _ch[2];

  cStrDestW.Empty();
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenA(szSrcA);
  if (szSrcA == NULL)
    return E_POINTER;
  //ensure that the input contains only ASCII characters.
  for (si=0; si<nSrcLen; si++)
  {
    if ((szSrcA[si] & 0x80) != 0)
      return E_INVALIDARG;
  }

  //reverse-search for delimiter in input
  for (p=szSrcA+nSrcLen; p>szSrcA && *(p-1)!='-'; p--);
  if (p == szSrcA)
  {
    //no delimiter found
    for (i=0; i<nSrcLen; i++)
    {
      _ch[0] = (WCHAR)(UCHAR)szSrcA[i];
      if (cStrDestW.ConcatN(_ch, 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    return S_OK;
  }

  //copy basic code points to output
  si = (SIZE_T)(p - szSrcA);
  for (i=0; i<si; i++)
  {
    if (aCodepoints.AddElement((ULONG)(UCHAR)szSrcA[i]) == FALSE)
      return E_OUTOFMEMORY;
  }
  si++; //skip delimiter

  i = 0;
  n = INITIAL_N;
  bias = INITIAL_BIAS;
  while (si < nSrcLen)
  {
    org_i = i;
    for (w=1,k=BASE; ; k+=BASE)
    {
      if (si >= nSrcLen)
        return E_FAIL;
      digit = decode_digit(szSrcA[si++]);

      if (digit == __SIZE_T_MAX)
        return E_FAIL;
      if (digit > (__SIZE_T_MAX - i) / w)
        return MX_E_ArithmeticOverflow;

      i += digit * w;

      if (k <= bias)
        t = TMIN;
      else if (k >= bias + TMAX)
        t = TMAX;
      else
        t = k - bias;
      if (digit < t)
        break;

      if (w > __SIZE_T_MAX / (BASE - t))
        return MX_E_ArithmeticOverflow; // OVERFLOW

      w *= BASE - t;
    }
    bias = adapt_bias(i-org_i, aCodepoints.GetCount()+1, org_i==0);
    if (i / (aCodepoints.GetCount()+1) > __SIZE_T_MAX - n)
      return MX_E_ArithmeticOverflow; // OVERFLOW

    n += i / (aCodepoints.GetCount()+1);
    i %= (aCodepoints.GetCount()+1);
    if (aCodepoints.InsertElementAt((ULONG)n, i) == FALSE)
      return E_OUTOFMEMORY;
  }

  t = aCodepoints.GetCount();
  for (i=0; i<t; i++)
  {
    n = aCodepoints[i];
    if (n < 0x10000)
    {
      _ch[0] = (WCHAR)n;
      if (cStrDestW.ConcatN(_ch, 1) == FALSE)
        return E_OUTOFMEMORY;
      i++;
    }
    else
    {
      n -= 0x10000;
      _ch[0] = (WCHAR)((n >> 10) + 0xD800);
      _ch[1] = (WCHAR)((n & 0x3FF) + 0xDC00);
      if (cStrDestW.ConcatN(_ch, 2) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

SIZE_T GetWideChar(_In_ LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
{
  if (szSrcW[0] < 0xD800 || szSrcW[0] > 0xDBFF)
    return (SIZE_T)szSrcW[0];
  if (nSrcLen < 1)
    return 0;
  if (szSrcW[1] < 0xDC00 || szSrcW[1] > 0xDFFF)
    return 0;
  return (((SIZE_T)szSrcW[0] - 0xD800) << 10) + 0x10000 + ((SIZE_T)szSrcW[1] - 0xDC00);
}

static SIZE_T adapt_bias(_In_ SIZE_T delta, _In_ SIZE_T n_points, _In_ bool is_first)
{
  SIZE_T k;

  delta /= is_first ? DAMP : 2;
  delta += delta / n_points;

  // while delta > 455: delta /= 35
  for (k = 0; delta > ((BASE - TMIN) * TMAX) / 2; k += BASE)
  {
    delta /= (BASE - TMIN);
  }
  return k + (((BASE - TMIN + 1) * delta) / (delta + SKEW));
}

static CHAR encode_digit(_In_ SIZE_T c)
{
  MX_ASSERT(c >= 0 && c <= BASE - TMIN);
  if (c > 25)
    return (char)(c + 22); // '0'..'9'
  return (char)(c + 97); // 'a'..'z'
}

// Encode as a generalized variable-length integer. Returns number of bytes written.
static HRESULT encode_var_int(_In_ SIZE_T bias, _In_ SIZE_T delta, _Inout_ MX::CStringA &cStrDestA)
{
  SIZE_T i, k, q, t;
  CHAR _ch;

  i = 0;
  k = BASE;
  q = delta;

  while (1)
  {
    if (k <= bias)
      t = TMIN;
    else if (k >= bias + TMAX)
      t = TMAX;
    else
      t = k - bias;
    if (q < t)
      break;
    _ch = encode_digit(t + (q - t) % (BASE - t));
    if (cStrDestA.ConcatN(&_ch, 1) == FALSE)
      return E_OUTOFMEMORY;
    q = (q - t) / (BASE - t);
    k += BASE;
  }

  _ch = encode_digit(q);
  return (cStrDestA.ConcatN(&_ch, 1) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

static SIZE_T decode_digit(_In_ CHAR v)
{
  if (isdigit(v))
    return 22 + (v - '0');
  if (islower(v))
    return v - 'a';
  if (isupper(v))
    return v - 'A';
  return (SIZE_T)-1;
}

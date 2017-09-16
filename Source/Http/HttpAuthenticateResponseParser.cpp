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
#include "..\..\Include\Http\HttpAuthenticateResponseParser.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

CHttpAuthenticateResponseParser::CHttpAuthenticateResponseParser() : CBaseMemObj()
{
  nClass = ClassUnknown;
  nType = TypeUnknown;
  return;
}

CHttpAuthenticateResponseParser::~CHttpAuthenticateResponseParser()
{
  Reset();
  return;
}

VOID CHttpAuthenticateResponseParser::Reset()
{
  cTokenValuesList.RemoveAllElements();
  nClass = ClassUnknown;
  nType = TypeUnknown;
  return;
}

HRESULT CHttpAuthenticateResponseParser::Parse(__in_z LPCSTR szResponseA)
{
  TAutoFreePtr<TOKEN_VALUE> cNewTokenValue;
  LPCSTR szNameStartA, szNameEndA, szValueStartA, szValueEndA;
  CHAR chQuoteA;

  if (szResponseA == NULL)
    return E_POINTER;
  Reset();
  if (StrNCompareA(szResponseA, "WWW-Authenticate", 16) == 0)
  {
    nClass = ClassWWW;
    szResponseA += 16;
  }
  else if (StrNCompareA(szResponseA, "Proxy-Authenticate", 18) == 0)
  {
    nClass = ClassProxy;
    szResponseA += 18;
  }
  else
  {
    return E_FAIL;
  }
  if (*szResponseA != ':')
    return E_FAIL;
  szResponseA++;
  while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
    szResponseA++;
  if (StrNCompareA(szResponseA, "Basic", 5) == 0)
  {
    nType = TypeBasic;
    szResponseA += 5;
  }
  else if (StrNCompareA(szResponseA, "Digest", 6) == 0)
  {
    nType = TypeDigest;
    szResponseA += 6;
  }
  else
  {
    return E_FAIL;
  }
  while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
    szResponseA++;
  if (*szResponseA == 0)
    return (nType == TypeBasic) ? S_OK : E_FAIL;
  while (*szResponseA != 0)
  {
    //skip spaces
    while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
      szResponseA++;
    //get key
    if (*szResponseA == ',')
    {
      szResponseA++;
      continue;
    }
    //get key name
    szNameStartA = szResponseA;
    while (*((UCHAR*)szResponseA) > 32)
      szResponseA++;
    szNameEndA = szResponseA;
    if (szNameEndA == szNameStartA)
      return E_INVALIDARG;
    //skip spaces
    while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
      szResponseA++;
    //check for equal sign
    if (*szResponseA++ != '=')
      return E_INVALIDARG;
    //skip spaces
    while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
      szResponseA++;
    //is value quoted?
    if (*szResponseA == '\'' || *szResponseA == '\"')
    {
      chQuoteA = *szResponseA++;
      szValueStartA = szResponseA;
      while (*szResponseA != 0 && *szResponseA != chQuoteA)
        szResponseA++;
      if (*szResponseA != chQuoteA)
        return E_INVALIDARG;
      szValueEndA = szResponseA;
    }
    else
    {
      szValueStartA = szResponseA;
      while (*((UCHAR*)szResponseA) > 32)
        szResponseA++;
      szValueEndA = szResponseA;
    }
    //skip spaces
    while (*szResponseA != 0 && *((UCHAR*)szResponseA) <= 32)
      szResponseA++;
    //add new key/pair to list
    cNewTokenValue.Attach((LPTOKEN_VALUE)MX_MALLOC(sizeof(TOKEN_VALUE) + (SIZE_T)(szNameEndA-szNameStartA) +
                                                   (szValueEndA-szValueStartA) + 2));
    if (!cNewTokenValue)
      return E_OUTOFMEMORY;
    MemCopy(cNewTokenValue->szNameA, szNameStartA, (SIZE_T)(szNameEndA-szNameStartA));
    cNewTokenValue->szNameA[(SIZE_T)(szNameEndA-szNameStartA)] = 0;
    cNewTokenValue->szValueA = (LPSTR)((LPBYTE)cNewTokenValue.Get() + sizeof(TOKEN_VALUE) +
                                       (SIZE_T)(szNameEndA-szNameStartA) + 1);
    MemCopy(cNewTokenValue->szValueA, szNameStartA, (SIZE_T)(szNameEndA-szNameStartA));
    cNewTokenValue->szValueA[(SIZE_T)(szNameEndA-szNameStartA)] = 0;
    if (cTokenValuesList.AddElement(cNewTokenValue.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cNewTokenValue.Detach();
  }
  return S_OK;
}

LPCSTR CHttpAuthenticateResponseParser::GetValue(__in_z LPCSTR szKeyA) const
{
  SIZE_T i, nCount;

  if (szKeyA != NULL && szKeyA[0] != 0)
  {
    nCount = cTokenValuesList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(szKeyA, cTokenValuesList[i]->szNameA, TRUE) == 0)
        return cTokenValuesList[i]->szValueA;
    }
  }
  return NULL;
}

HRESULT CHttpAuthenticateResponseParser::MakeAutoAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA,
                                                               __in_z LPCSTR szPasswordA)
{
  switch (nType)
  {
    case TypeBasic:
      return MakeBasicAuthorization(cStrDestA, szUserNameA, szPasswordA, (nClass == ClassProxy) ? TRUE : FALSE);

    case TypeDigest:
      return E_NOTIMPL;
  }
  return MX_E_NotReady;
}

HRESULT CHttpAuthenticateResponseParser::MakeBasicAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA,
                                                                __in_z LPCSTR szPasswordA, __in BOOL bIsProxy)
{
  CStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  if (szUserNameA == NULL)
    return E_POINTER;
  if (*szUserNameA == 0)
    return E_INVALIDARG;
  if (cStrTempA.Format("%s:%s", szUserNameA, (szPasswordA!=NULL) ? szPasswordA : "") != FALSE)
  {
    CBase64Encoder cBase64Enc;

    hRes = cBase64Enc.Begin(cBase64Enc.GetRequiredSpace(cStrTempA.GetLength()));
    if (SUCCEEDED(hRes))
      hRes = cBase64Enc.Process((LPSTR)cStrTempA, cStrTempA.GetLength());
    if (SUCCEEDED(hRes))
      hRes = cBase64Enc.End();
    if (SUCCEEDED(hRes) && cStrDestA.Format("Authorization: Basic %s", cBase64Enc.GetBuffer()) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hRes) && bIsProxy != FALSE && cStrDestA.Insert("Proxy-", 0) == FALSE)
    hRes = E_OUTOFMEMORY;
  if (FAILED(hRes))
    cStrDestA.Empty();
  MemSet((LPSTR)cStrTempA, '|', cStrTempA.GetLength());
  return hRes;
}

HRESULT CHttpAuthenticateResponseParser::MakeDigestAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA,
                                                                 __in_z LPCSTR szRealmA, __in_z LPCSTR szPasswordA,
                                                                 __in_z LPCSTR szAlgorithmA, __in_z LPCSTR szMethodA,
                                                                 __in_z LPCSTR szNonceA, __in SIZE_T nNonceCount,
                                                                 __in_z LPCSTR szQopA, __in_z LPCSTR szDigestUriA,
                                                                 __in LPVOID lpBodyMD5Checksum, __in_z LPCSTR szOpaqueA,
                                                                 __in BOOL bIsProxy)
{
  cStrDestA.Empty();
  return E_NOTIMPL;
  /*
  CSLDigestAlgorithmMessageDigest cMd5Calc;
  CSLStringAnsi cTempStr;
  SL_CHAR szCNonce[16], szA1[36], szA2[36];
  SL_CHAR szNonceCount[12], szTemp[64];
  SL_INT nAlg, nQop;
  SL_BYTE aA1[16], aA2[16], aResponse[16];
  SL_UINT i;

  cStrTarget.Empty();
  if (szRealm == NULL) { // || szUserName[0]==0
    slSetLastError(SLERR_InvalidParameter);
    return FALSE;
  }
  if (szUserName==NULL || szUserName[0]==0) {
    slSetLastError(SLERR_InvalidParameter);
    return FALSE;
  }
  if (szAlgorithm!=NULL && szAlgorithm[0]!=0) {
    if (slStrICmpAnsi(szAlgorithm, "md5") == 0)
      nAlg = 1;
    else if (slStrICmpAnsi(szAlgorithm, "md5-sess") == 0)
      nAlg = 2;
    else {
      slSetLastError(SLERR_InvalidParameter);
      return FALSE;
    }
  }
  else
    nAlg = 1;
  if (szQop!=NULL && szQop[0]!=0) {
    if (slStrICmpAnsi(szQop, "auth") == 0)
      nQop = 1;
    else if (slStrICmpAnsi(szQop, "auth-int") == 0)
      nQop = 2;
    else {
      slSetLastError(SLERR_InvalidParameter);
      return FALSE;
    }
  }
  else
    nQop = 0;
  if (szNonce==NULL || szNonce[0]==0) {
    slSetLastError(SLERR_InvalidParameter);
    return FALSE;
  }
  if (szMethod==NULL || szMethod[0]==0) {
    slSetLastError(SLERR_InvalidParameter);
    return FALSE;
  }
  if (bIsProxy != FALSE) {
    if ((cStrTarget += "Proxy-") == FALSE)
      return FALSE;
  }
  if ((cStrTarget += "Authorization: Digest username=\"") == FALSE ||
      (cStrTarget += szUserName) == FALSE ||
      (cStrTarget += "\", \r\n  realm=\"") == FALSE ||
      (cStrTarget += szRealm) == FALSE ||
      (cStrTarget += "\", \r\n") == FALSE)
    return FALSE;
  if (cMd5Calc.BeginDigest(CSLDigestAlgorithmMessageDigest::algMD5) == FALSE ||
      cMd5Calc.DigestStream(szUserName, slStrLengthAnsi(szUserName)) == FALSE ||
      cMd5Calc.DigestStream(":", 1) == FALSE ||
      cMd5Calc.DigestStream(szRealm, slStrLengthAnsi(szRealm)) == FALSE ||
      cMd5Calc.DigestStream(":", 1) == FALSE)
    return FALSE;
  if (szPassword != NULL &&
      cMd5Calc.DigestStream(szPassword, slStrLengthAnsi(szPassword)) == FALSE)
    return FALSE;
  if (cMd5Calc.EndDigest() == FALSE)
    return FALSE;
  slMemCopy(aA1, cMd5Calc.GetResult(), 16);
  srand((unsigned int)time(NULL));
  if (nQop != 0) {
    for (i=0; i<15; i++)
      szCNonce[i] = SL_INTHLP_szHexaNumbersA[rand() % 15];
    szCNonce[0] = 0;
    slStrLowerAnsi(szCNonce);
  }
  else
    szCNonce[0] = 0;
  if (nAlg == 2) {
    if (cMd5Calc.BeginDigest(CSLDigestAlgorithmMessageDigest::algMD5) == FALSE ||
        cMd5Calc.DigestStream(aA1, 16) == FALSE ||
        cMd5Calc.DigestStream(":", 1) == FALSE ||
        cMd5Calc.DigestStream(szNonce, slStrLengthAnsi(szNonce)) == FALSE ||
        cMd5Calc.DigestStream(":", 1) == FALSE ||
        cMd5Calc.DigestStream(szCNonce, slStrLengthAnsi(szCNonce)) == FALSE ||
        cMd5Calc.EndDigest() == FALSE)
      return FALSE;
    slMemCopy(aA1, cMd5Calc.GetResult(), 16);
  }
  for (i=0; i<16; i++) {
    szA1[i<<1] = SL_INTHLP_szHexaNumbersA[aA1[i] >> 4];
    szA1[(i<<1)+1] = SL_INTHLP_szHexaNumbersA[aA1[i] & 0x0F];
  }
  szA1[32] = 0;
  slStrLowerAnsi(szA1);
  if (nQop != 0) {
    for (i=0; i<8; i++)
      szNonceCount[7-i] = SL_INTHLP_szHexaNumbersA[(nNonceCount >> (i<<2))  & 0x0F];
    szNonceCount[8] = 0;
  }
  else
    szNonceCount[0] = 0;
  if (cMd5Calc.BeginDigest(CSLDigestAlgorithmMessageDigest::algMD5) == FALSE ||
      cMd5Calc.DigestStream(szMethod, slStrLengthAnsi(szMethod)) == FALSE ||
      cMd5Calc.DigestStream(":", 1) == FALSE ||
      cMd5Calc.DigestStream(szDigestUri, slStrLengthAnsi(szDigestUri)) == FALSE)
    return FALSE;
  if (nQop == 2) {
    if (lpBodyMD5Checksum == NULL) {
      slSetLastError(SLERR_InvalidParameter);
      return FALSE;
    }
    for (i=0; i<16; i++) {
      szTemp[i<<1] = SL_INTHLP_szHexaNumbersA[lpBodyMD5Checksum[i] >> 4];
      szTemp[(i<<1)+1] = SL_INTHLP_szHexaNumbersA[lpBodyMD5Checksum[i] & 0x0F];
    }
    szTemp[32] = 0;
    slStrLowerAnsi(szTemp);
    if (cMd5Calc.DigestStream(":", 1) == FALSE ||
        cMd5Calc.DigestStream(szTemp, 32) == FALSE)
      return FALSE;
  }
  if (cMd5Calc.EndDigest() == FALSE)
    return FALSE;
  slMemCopy(aA2, cMd5Calc.GetResult(), 16);
  for (i=0; i<16; i++) {
    szA2[i<<1] = SL_INTHLP_szHexaNumbersA[aA2[i] >> 4];
    szA2[(i<<1)+1] = SL_INTHLP_szHexaNumbersA[aA2[i] & 0x0F];
  }
  szA2[32] = 0;
  slStrLowerAnsi(szA1);
  if (cMd5Calc.BeginDigest(CSLDigestAlgorithmMessageDigest::algMD5) == FALSE ||
      cMd5Calc.DigestStream(szA1, 32) == FALSE ||
      cMd5Calc.DigestStream(":", 1) == FALSE ||
      cMd5Calc.DigestStream(szNonce, slStrLengthAnsi(szNonce)) == FALSE ||
      cMd5Calc.DigestStream(":", 1) == FALSE)
    return FALSE;
  if (nQop != 0) {
    if (cMd5Calc.DigestStream(szNonceCount, slStrLengthAnsi(szNonceCount)) == FALSE ||
        cMd5Calc.DigestStream(":", 1) == FALSE ||
        cMd5Calc.DigestStream(szCNonce, slStrLengthAnsi(szCNonce)) == FALSE ||
        cMd5Calc.DigestStream(":", 1) == FALSE ||
        cMd5Calc.DigestStream(szQop, slStrLengthAnsi(szQop)) == FALSE ||
        cMd5Calc.DigestStream(":", 1) == FALSE)
      return FALSE;
  }
  if (cMd5Calc.DigestStream(szA2, 32) == FALSE ||
      cMd5Calc.EndDigest() == FALSE)
    return FALSE;
  slMemCopy(aResponse, cMd5Calc.GetResult(), 16);
  if ((cStrTarget += "nonce=\"") == FALSE ||
      (cStrTarget += szNonce) == FALSE ||
      (cStrTarget += "\",\r\n  digest-uri=\"") == FALSE ||
      (cStrTarget += szDigestUri) == FALSE ||
      (cStrTarget += "\",\r\n") == FALSE)
    return FALSE;
  if (nQop != 0) {
    if ((cStrTarget += "  qop=") == FALSE ||
        (cStrTarget += szQop) == FALSE ||
        (cStrTarget += ",\r\n") == FALSE)
      return FALSE;
  }
  if (szNonceCount[0] != 0) {
    if ((cStrTarget += "  nc=") == FALSE ||
        (cStrTarget += szNonceCount) == FALSE ||
        (cStrTarget += ",\r\n") == FALSE)
      return FALSE;
  }
  if (szCNonce[0] != 0) {
    if ((cStrTarget += "  cnonce=\"") == FALSE ||
        (cStrTarget += szCNonce) == FALSE ||
        (cStrTarget += "\",\r\n") == FALSE)
      return FALSE;
  }
  if ((cStrTarget += "  response=\"") == FALSE)
    return FALSE;
  for (i=0; i<16; i++) {
    szTemp[i<<1] = SL_INTHLP_szHexaNumbersA[aResponse[i] >> 4];
    szTemp[(i<<1)+1] = SL_INTHLP_szHexaNumbersA[aResponse[i] & 0x0F];
  }
  szTemp[32] = 0;
  if ((cStrTarget += szTemp) == FALSE ||
      (cStrTarget += "\"") == FALSE)
    return FALSE;
  if (szOpaque!=NULL && szOpaque[0]!=0) {
    if ((cStrTarget += "\r\n  opaque=\"") == FALSE ||
        (cStrTarget += szOpaque) == FALSE ||
        (cStrTarget += "\"\r\n") == FALSE)
      return FALSE;
  }
  else {
    if ((cStrTarget += "\r\n") == FALSE)
      return FALSE;
  }
  slSetLastError(SLERR_NoError);
  return TRUE;
  */
}

} //namespace MX

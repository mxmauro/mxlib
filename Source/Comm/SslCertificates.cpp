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
#include "..\..\Include\Comm\SslCertificates.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Utf8.h"
#include <OpenSSL\x509.h>
#include <OpenSSL\x509v3.h>

#define _x509                                ((X509*)lpX509)
#define _x509Crl                      ((X509_CRL*)lpX509Crl)

//-----------------------------------------------------------

static HRESULT Asn1TimeToDateTime(__in const ASN1_TIME *lpTime, __out MX::CDateTime &cDt);
static HRESULT GetName(__in X509_NAME *lpName, __in MX::CSslCertificate::eInformation nInfo,
                       __inout MX::CStringW &cStrW);

//-----------------------------------------------------------

namespace MX {

CSslCertificate::CSslCertificate() : CBaseMemObj()
{
  lpX509 = NULL;
  return;
}

CSslCertificate::~CSslCertificate()
{
  if (lpX509 != NULL)
    X509_free(_x509);
  return;
}

LONG CSslCertificate::GetVersion() const
{
  return (lpX509 != NULL) ? (LONG)X509_get_version(_x509) : 0;
}

LPBYTE CSslCertificate::GetSerial() const
{
  ASN1_INTEGER *bs;

  if (lpX509 == NULL)
    return NULL;
  bs = X509_get_serialNumber(_x509);
  return (LPBYTE)(bs->data);
}

SIZE_T CSslCertificate::GetSerialLength() const
{
  ASN1_INTEGER *bs;

  if (lpX509 == NULL)
    return NULL;
  bs = X509_get_serialNumber(_x509);
  return (SIZE_T)(unsigned int)(bs->length);
}

HRESULT CSslCertificate::GetSubject(__in eInformation nInfo, __inout CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_subject_name(_x509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetIssuer(__in eInformation nInfo, __inout CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_issuer_name(_x509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetValidFrom(__inout CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notBefore(_x509), cDt);
}

HRESULT CSslCertificate::GetValidUntil(__inout CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notAfter(_x509), cDt);
}

BOOL CSslCertificate::IsCaCert() const
{
  return (lpX509 != NULL && X509_check_ca(_x509) >= 1) ? TRUE : FALSE;
}

HRESULT CSslCertificate::operator=(const CSslCertificate& cSrc)
{
  if (this != &cSrc)
  {
    X509 *lpNewX509;

    //duplicate
    lpNewX509 = NULL;
    if (cSrc.lpX509 != NULL)
    {
      lpNewX509 = X509_dup((X509*)(cSrc.lpX509));
      if (lpNewX509 == NULL)
        return E_OUTOFMEMORY;
    }
    //replace original
    if (lpX509 != NULL)
      X509_free(_x509);
    lpX509 = lpNewX509;
  }
  return S_OK;
}

LPVOID CSslCertificate::GetOpenSSL_X509()
{
  return lpX509;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CSslCertificateCrl::CSslCertificateCrl() : CBaseMemObj()
{
  lpX509Crl = NULL;
  return;
}

CSslCertificateCrl::~CSslCertificateCrl()
{
  if (lpX509Crl != NULL)
    X509_CRL_free(_x509Crl);
  return;
}

LONG CSslCertificateCrl::GetVersion() const
{
  return (lpX509Crl != NULL) ? (LONG)X509_CRL_get_version(_x509Crl) : 0;
}

HRESULT CSslCertificateCrl::GetIssuer(__in eInformation nInfo, __inout CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return GetName(X509_CRL_get_issuer(_x509Crl), (MX::CSslCertificate::eInformation)nInfo, cStrW);
}

HRESULT CSslCertificateCrl::GetUpdate(__inout CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_CRL_get_lastUpdate(_x509Crl), cDt);
}

HRESULT CSslCertificateCrl::GetNextUpdate(__inout CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_CRL_get_nextUpdate(_x509Crl), cDt);
}

SIZE_T CSslCertificateCrl::GetRevokedEntriesCount() const
{
  STACK_OF(X509_REVOKED) *rev;
  int count;

  if (lpX509Crl == NULL)
    return 0;
  rev = X509_CRL_get_REVOKED(_x509Crl);
  count = sk_X509_REVOKED_num(rev);
  return (count > 0) ? (SIZE_T)count : 0;
}

LPBYTE CSslCertificateCrl::GetRevokedEntrySerial(__in SIZE_T nEntryIndex) const
{
  STACK_OF(X509_REVOKED) *rev;
  X509_REVOKED *r;
  const ASN1_INTEGER *lpSerial;
  int count;

  if (lpX509Crl == NULL)
    return NULL;
  rev = X509_CRL_get_REVOKED(_x509Crl);
  count = sk_X509_REVOKED_num(rev);
  if (count <= 0 || nEntryIndex >= (SIZE_T)count)
    return NULL;
  r = sk_X509_REVOKED_value(rev, (int)nEntryIndex);
  lpSerial = X509_REVOKED_get0_serialNumber(r);
  return (LPBYTE)(lpSerial->data);
}

SIZE_T CSslCertificateCrl::GetRevokedEntrySerialLength(__in SIZE_T nEntryIndex) const
{
  STACK_OF(X509_REVOKED) *rev;
  X509_REVOKED *r;
  const ASN1_INTEGER *lpSerial;
  int count;

  if (lpX509Crl == NULL)
    return 0;
  rev = X509_CRL_get_REVOKED(_x509Crl);
  count = sk_X509_REVOKED_num(rev);
  if (count <= 0 || nEntryIndex >= (SIZE_T)count)
    return 0;
  r = sk_X509_REVOKED_value(rev, (int)nEntryIndex);
  lpSerial = X509_REVOKED_get0_serialNumber(r);
  return (SIZE_T)(lpSerial->length);
}

HRESULT CSslCertificateCrl::GetRevokedEntryDate(__in SIZE_T nEntryIndex, __inout CDateTime &cDt)
{
  STACK_OF(X509_REVOKED) *rev;
  X509_REVOKED *r;
  int count;

  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  rev = X509_CRL_get_REVOKED(_x509Crl);
  count = sk_X509_REVOKED_num(rev);
  if (count <= 0 || nEntryIndex >= (SIZE_T)count)
    return E_INVALIDARG;
  r = sk_X509_REVOKED_value(rev, (int)nEntryIndex);
  return Asn1TimeToDateTime(X509_REVOKED_get0_revocationDate(r), cDt);
}

HRESULT CSslCertificateCrl::operator=(const CSslCertificateCrl& cSrc)
{
  if (this != &cSrc)
  {
    X509_CRL *lpNewX509Crl;

    //duplicate
    lpNewX509Crl = NULL;
    if (cSrc.lpX509Crl != NULL)
    {
      lpNewX509Crl = X509_CRL_dup((X509_CRL*)(cSrc.lpX509Crl));
      if (lpNewX509Crl == NULL)
        return E_OUTOFMEMORY;
    }
    //replace original
    if (lpX509Crl != NULL)
      X509_CRL_free(_x509Crl);
    lpX509Crl = lpNewX509Crl;
  }
  return S_OK;
}

LPVOID CSslCertificateCrl::GetOpenSSL_X509Crl()
{
  return lpX509Crl;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT Asn1TimeToDateTime(__in const ASN1_TIME *lpTime, __out MX::CDateTime &cDt)
{
  HRESULT hRes;

  cDt.Clear();
  cDt.SetGmtOffset(0);
  switch (lpTime->type)
  {
    case V_ASN1_UTCTIME: //two digit year
      hRes = cDt.SetFromString((LPCSTR)(lpTime->data), "%y%m%d%H%M%S");
      break;
    case V_ASN1_GENERALIZEDTIME: //four digit year
      hRes = cDt.SetFromString((LPCSTR)(lpTime->data), "%Y%m%d%H%M%S");
      break;
    default:
      hRes = MX_E_InvalidData;
      break;
  }
  return (hRes != E_FAIL) ? hRes : MX_E_InvalidData;
}

static HRESULT GetName(__in X509_NAME *lpName, __in MX::CSslCertificate::eInformation nInfo,
                       __inout MX::CStringW &cStrW)
{
  MX::TAutoFreePtr<CHAR> aTempBuf;
  int i, nid;

  if (lpName == NULL)
    return S_OK;
  switch (nInfo)
  {
    case MX::CSslCertificate::InfoOrganization:
      nid = NID_organizationName;
      break;
    case MX::CSslCertificate::InfoUnit:
      nid = NID_organizationalUnitName;
      break;
    case MX::CSslCertificate::InfoCommonName:
      nid = NID_commonName;
      break;
    case MX::CSslCertificate::InfoCountry:
      nid = NID_countryName;
      break;
    case MX::CSslCertificate::InfoStateProvince:
      nid = NID_stateOrProvinceName;
      break;
    case MX::CSslCertificate::InfoTown:
      nid = NID_localityName;
      break;
    default:
      return E_INVALIDARG;
  }
  i = X509_NAME_get_text_by_NID(lpName, nid, NULL, 0);
  if (i <= 0)
    return S_OK;
  aTempBuf.Attach((LPSTR)MX_MALLOC((SIZE_T)i + 1));
  if (!aTempBuf)
    return E_OUTOFMEMORY;
  i = X509_NAME_get_text_by_NID(lpName, nid, aTempBuf.Get(), (i+1));
  return MX::Utf8_Decode(cStrW, aTempBuf.Get(), (SIZE_T)i);
}

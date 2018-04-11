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

static HRESULT Asn1TimeToDateTime(_In_ const ASN1_TIME *lpTime, _Out_ MX::CDateTime &cDt);
static HRESULT GetName(_In_ X509_NAME *lpName, _In_ MX::CSslCertificate::eInformation nInfo,
                       _Inout_ MX::CStringW &cStrW);

//-----------------------------------------------------------

namespace MX {

CSslCertificate::CSslCertificate() : CBaseMemObj()
{
  lpX509 = NULL;
  return;
}

CSslCertificate::CSslCertificate(_In_ const CSslCertificate& cSrc) throw(...) : CBaseMemObj()
{
  lpX509 = NULL;
  operator=(cSrc);
  return;
}

CSslCertificate::~CSslCertificate()
{
  if (lpX509 != NULL)
    X509_free(_x509);
  return;
}

CSslCertificate& CSslCertificate::operator=(_In_ const CSslCertificate& cSrc) throw(...)
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
        throw (LONG)E_OUTOFMEMORY;
    }
    //replace original
    if (lpX509 != NULL)
      X509_free(_x509);
    lpX509 = lpNewX509;
  }
  return *this;
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

HRESULT CSslCertificate::GetSubject(_In_ eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_subject_name(_x509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_issuer_name(_x509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetValidFrom(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notBefore(_x509), cDt);
}

HRESULT CSslCertificate::GetValidUntil(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notAfter(_x509), cDt);
}

HRESULT CSslCertificate::IsDateValid()
{
  CDateTime cDtNow, cDt;
  HRESULT hRes;

  hRes = cDtNow.SetFromNow(FALSE);
  if (hRes == S_OK)
    hRes = GetValidFrom(cDt);
  if (hRes == S_OK && cDtNow < cDt)
    hRes = S_FALSE;
  if (hRes == S_OK)
    hRes = GetValidUntil(cDt);
  if (hRes == S_OK && cDtNow >= cDt)
    hRes = S_FALSE;
  return hRes;
}

BOOL CSslCertificate::IsCaCert() const
{
  return (lpX509 != NULL && X509_check_ca(_x509) >= 1) ? TRUE : FALSE;
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

HRESULT CSslCertificateCrl::GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return GetName(X509_CRL_get_issuer(_x509Crl), (MX::CSslCertificate::eInformation)nInfo, cStrW);
}

HRESULT CSslCertificateCrl::GetUpdate(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_CRL_get_lastUpdate(_x509Crl), cDt);
}

HRESULT CSslCertificateCrl::GetNextUpdate(_Inout_ CDateTime &cDt)
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

LPBYTE CSslCertificateCrl::GetRevokedEntrySerial(_In_ SIZE_T nEntryIndex) const
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

SIZE_T CSslCertificateCrl::GetRevokedEntrySerialLength(_In_ SIZE_T nEntryIndex) const
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

HRESULT CSslCertificateCrl::GetRevokedEntryDate(_In_ SIZE_T nEntryIndex, _Inout_ CDateTime &cDt)
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

static HRESULT Asn1TimeToDateTime(_In_ const ASN1_TIME *lpTime, _Out_ MX::CDateTime &cDt)
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

static HRESULT GetName(_In_ X509_NAME *lpName, _In_ MX::CSslCertificate::eInformation nInfo,
                       _Inout_ MX::CStringW &cStrW)
{
  MX::TAutoFreePtr<CHAR> aTempBuf;
  ASN1_OBJECT *obj;
  int idx, utf8_data_len;
  unsigned char *utf8_data;
  HRESULT hRes;

  ASN1_STRING *data;

  if (lpName == NULL)
    return S_OK;
  obj = NULL;
  switch (nInfo)
  {
    case MX::CSslCertificate::InfoOrganization:
      obj = OBJ_nid2obj(NID_organizationName);
      break;
    case MX::CSslCertificate::InfoUnit:
      obj = OBJ_nid2obj(NID_organizationalUnitName);
      break;
    case MX::CSslCertificate::InfoCommonName:
      obj = OBJ_nid2obj(NID_commonName);
      break;
    case MX::CSslCertificate::InfoCountry:
      obj = OBJ_nid2obj(NID_countryName);
      break;
    case MX::CSslCertificate::InfoStateProvince:
      obj = OBJ_nid2obj(NID_stateOrProvinceName);
      break;
    case MX::CSslCertificate::InfoTown:
      obj = OBJ_nid2obj(NID_localityName);
      break;
  }
  if (obj == NULL)
    return E_INVALIDARG;

  idx = X509_NAME_get_index_by_OBJ(lpName, obj, -1);
  if (idx < 0)
    return MX_E_NotFound;
  data = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(lpName, idx));

  utf8_data_len = ASN1_STRING_to_UTF8(&utf8_data, data);
  if (utf8_data_len < 0)
    return MX::Internals::OpenSSL::GetLastErrorCode(TRUE);

  hRes = MX::Utf8_Decode(cStrW, (LPCSTR)utf8_data, (SIZE_T)(unsigned int)utf8_data_len);
  OPENSSL_free(utf8_data);
  return hRes;
}

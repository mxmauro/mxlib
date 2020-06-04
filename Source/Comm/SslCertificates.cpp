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
#include "..\..\Include\Comm\SslCertificates.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Utf8.h"
#include <OpenSSL\x509.h>
#include <OpenSSL\x509v3.h>

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

CSslCertificate::CSslCertificate(_In_ const CSslCertificate &cSrc) throw(...) : CBaseMemObj()
{
  lpX509 = NULL;
  operator=(cSrc);
  return;
}

CSslCertificate::~CSslCertificate()
{
  if (lpX509 != NULL)
    X509_free(lpX509);
  return;
}

CSslCertificate& CSslCertificate::operator=(_In_ const CSslCertificate &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    HRESULT hRes;

    hRes = Internals::OpenSSL::Init();
    if (FAILED(hRes))
      throw (LONG)hRes;
    //deref old
    if (lpX509 != NULL)
      X509_free(lpX509);
    //increment reference on new
    if (cSrc.lpX509 != NULL)
      X509_up_ref(cSrc.lpX509);
    lpX509 = cSrc.lpX509;
  }
  return *this;
}

LONG CSslCertificate::GetVersion() const
{
  return (lpX509 != NULL) ? (LONG)X509_get_version(lpX509) : 0;
}

LPBYTE CSslCertificate::GetSerial() const
{
  ASN1_INTEGER *bs;

  if (lpX509 == NULL)
    return NULL;
  bs = X509_get_serialNumber(lpX509);
  return (LPBYTE)(bs->data);
}

SIZE_T CSslCertificate::GetSerialLength() const
{
  ASN1_INTEGER *bs;

  if (lpX509 == NULL)
    return NULL;
  bs = X509_get_serialNumber(lpX509);
  return (SIZE_T)(unsigned int)(bs->length);
}

HRESULT CSslCertificate::GetSubject(_In_ MX::CSslCertificate::eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_subject_name(lpX509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetIssuer(_In_ MX::CSslCertificate::eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return GetName(X509_get_issuer_name(lpX509), nInfo, cStrW);
}

HRESULT CSslCertificate::GetValidFrom(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notBefore(lpX509), cDt);
}

HRESULT CSslCertificate::GetValidUntil(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509 == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_get_notAfter(lpX509), cDt);
}

HRESULT CSslCertificate::IsDateValid(_Out_opt_ PULONG lpnRemainingSecs)
{
  CDateTime cDtNow, cDt;
  HRESULT hRes;

  if (lpnRemainingSecs != NULL)
    *lpnRemainingSecs = 0;

  hRes = cDtNow.SetFromNow(FALSE);
  if (FAILED(hRes))
    return hRes;

  hRes = GetValidFrom(cDt);
  if (FAILED(hRes))
    return hRes;
  if (cDtNow < cDt)
    return S_FALSE;

  hRes = GetValidUntil(cDt);
  if (FAILED(hRes))
    return hRes;
  if (cDtNow >= cDt)
    return S_FALSE;
  if (lpnRemainingSecs != NULL)
  {
    LONGLONG llSecs = cDtNow.GetDiff(cDt, CDateTime::UnitsSeconds);
    if (llSecs >= 0)
    {
      *lpnRemainingSecs = (llSecs <= 0xFFFFFFFFi64) ? llSecs : 0xFFFFFFFF;
    }
  }

  //done
  return S_OK;
}

BOOL CSslCertificate::IsCaCert() const
{
  return (lpX509 != NULL && X509_check_ca(lpX509) >= 1) ? TRUE : FALSE;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CSslCertificateCrl::CSslCertificateCrl() : CBaseMemObj()
{
  lpX509Crl = NULL;
  return;
}

CSslCertificateCrl::CSslCertificateCrl(_In_ const CSslCertificateCrl &cSrc) throw(...) : CBaseMemObj()
{
  lpX509Crl = NULL;
  operator=(cSrc);
  return;
}

CSslCertificateCrl::~CSslCertificateCrl()
{
  if (lpX509Crl != NULL)
    X509_CRL_free(lpX509Crl);
  return;
}

CSslCertificateCrl& CSslCertificateCrl::operator=(const CSslCertificateCrl &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    HRESULT hRes;

    hRes = Internals::OpenSSL::Init();
    if (FAILED(hRes))
      throw (LONG)hRes;
    //deref old
    if (lpX509Crl != NULL)
      X509_CRL_free(lpX509Crl);
    //increment reference on new
    if (cSrc.lpX509Crl != NULL)
      X509_CRL_up_ref(cSrc.lpX509Crl);
    lpX509Crl = cSrc.lpX509Crl;
  }
  return *this;
}

LONG CSslCertificateCrl::GetVersion() const
{
  return (lpX509Crl != NULL) ? (LONG)X509_CRL_get_version(lpX509Crl) : 0;
}

HRESULT CSslCertificateCrl::GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW)
{
  cStrW.Empty();
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return GetName(X509_CRL_get_issuer(lpX509Crl), (MX::CSslCertificate::eInformation)nInfo, cStrW);
}

HRESULT CSslCertificateCrl::GetUpdate(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_CRL_get_lastUpdate(lpX509Crl), cDt);
}

HRESULT CSslCertificateCrl::GetNextUpdate(_Inout_ CDateTime &cDt)
{
  cDt.Clear();
  cDt.SetGmtOffset(0);
  if (lpX509Crl == NULL)
    return MX_E_NotReady;
  return Asn1TimeToDateTime(X509_CRL_get_nextUpdate(lpX509Crl), cDt);
}

SIZE_T CSslCertificateCrl::GetRevokedEntriesCount() const
{
  STACK_OF(X509_REVOKED) *rev;
  int count;

  if (lpX509Crl == NULL)
    return 0;
  rev = X509_CRL_get_REVOKED(lpX509Crl);
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
  rev = X509_CRL_get_REVOKED(lpX509Crl);
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
  rev = X509_CRL_get_REVOKED(lpX509Crl);
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
  rev = X509_CRL_get_REVOKED(lpX509Crl);
  count = sk_X509_REVOKED_num(rev);
  if (count <= 0 || nEntryIndex >= (SIZE_T)count)
    return E_INVALIDARG;
  r = sk_X509_REVOKED_value(rev, (int)nEntryIndex);
  return Asn1TimeToDateTime(X509_REVOKED_get0_revocationDate(r), cDt);
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
    return MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);

  hRes = MX::Utf8_Decode(cStrW, (LPCSTR)utf8_data, (SIZE_T)(unsigned int)utf8_data_len);
  OPENSSL_free(utf8_data);
  return hRes;
}

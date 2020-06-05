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
#ifndef _MX_SSLCERTIFICATES_H
#define _MX_SSLCERTIFICATES_H

#include "..\Defines.h"
#include "..\RefCounted.h"
#include "..\DateTime\DateTime.h"
#include "..\ArrayList.h"
#include "..\Crypto\EncryptionKey.h"
#include "..\Strings\Strings.h"
typedef struct x509_st X509;
typedef struct X509_crl_st X509_CRL;

//-----------------------------------------------------------

class CSslCertificate;
class CSslCertificateCrl;
class CSslCertificateArray;

//-----------------------------------------------------------

namespace MX {

class CSslCertificate : public virtual TRefCounted<CBaseMemObj>
{
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

  CSslCertificate();
  CSslCertificate(_In_ const CSslCertificate& cSrc) throw(...);
  ~CSslCertificate();

  CSslCertificate& operator=(_In_ const CSslCertificate& cSrc) throw(...);

  HRESULT InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                            _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  LONG GetVersion() const;

  LPBYTE GetSerial() const;
  SIZE_T GetSerialLength() const;

  HRESULT GetSubject(_In_ MX::CSslCertificate::eInformation nInfo, _Inout_ CStringW &cStrW);
  HRESULT GetIssuer(_In_ MX::CSslCertificate::eInformation nInfo, _Inout_ CStringW &cStrW);

  HRESULT GetValidFrom(_Inout_ CDateTime &cDt);
  HRESULT GetValidUntil(_Inout_ CDateTime &cDt);

  HRESULT IsDateValid(_Out_opt_ PULONG lpnRemainingSecs = NULL); //returns S_OK if valid, S_FALSE if not, or an error
  BOOL IsCaCert() const;

  X509* GetX509() const
    {
    return lpX509;
    };

private:
  X509 *lpX509;
};

//-----------------------------------------------------------

class CSslCertificateCrl : public virtual TRefCounted<CBaseMemObj>
{
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

public:
  CSslCertificateCrl();
  CSslCertificateCrl(_In_ const CSslCertificateCrl &cSrc) throw(...);
  ~CSslCertificateCrl();

  CSslCertificateCrl& operator=(_In_ const CSslCertificateCrl &cSrc) throw(...);

  HRESULT InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                            _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  LONG GetVersion() const;

  HRESULT GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW);

  HRESULT GetUpdate(_Inout_ CDateTime &cDt);
  HRESULT GetNextUpdate(_Inout_ CDateTime &cDt);

  SIZE_T GetRevokedEntriesCount() const;

  LPBYTE GetRevokedEntrySerial(_In_ SIZE_T nEntryIndex) const;
  SIZE_T GetRevokedEntrySerialLength(_In_ SIZE_T nEntryIndex) const;
  HRESULT GetRevokedEntryDate(_In_ SIZE_T nEntryIndex, _Inout_ CDateTime &cDt);

  X509_CRL* GetX509Crl() const
    {
    return lpX509Crl;
    };

private:
  X509_CRL *lpX509Crl;
};

//-----------------------------------------------------------

class CSslCertificateArray : public virtual TRefCounted<CBaseMemObj>, public CNonCopyableObj
{
public:
  CSslCertificateArray();
  ~CSslCertificateArray();

  VOID Reset();

  HRESULT AddFromMemory(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA = NULL);
  HRESULT AddFromFile(_In_z_ LPCWSTR szFileNameW, _In_opt_z_ LPCSTR szPasswordA = NULL);

  HRESULT AddPublicKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT AddPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                              _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  HRESULT AddPrivateKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT AddPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                               _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  HRESULT AddCertificateFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA = NULL);
  HRESULT AddCertificateFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                                _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  HRESULT AddCrlFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT AddCrlFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                        _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);

  HRESULT ImportFromWindowsStore();

public:
  TArrayListWithRelease<CSslCertificate*> cCertsList;
  TArrayListWithRelease<CSslCertificateCrl*> cCertCrlsList;
  TArrayListWithRelease<CEncryptionKey*> cKeysList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SSLCERTIFICATES_H

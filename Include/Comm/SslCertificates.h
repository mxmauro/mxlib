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
#ifndef _MX_SSLCERTIFICATES_H
#define _MX_SSLCERTIFICATES_H

#include "..\Defines.h"
#include "..\DateTime\DateTime.h"
#include "..\ArrayList.h"
#include "..\Crypto\CryptoRSA.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

class CSslCertificate;
class CSslCertificateCrl;
class CSslCertificateArray;

//-----------------------------------------------------------

namespace MX {

class CSslCertificate : public virtual CBaseMemObj
{
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

  CSslCertificate();
  CSslCertificate(_In_ const CSslCertificate& cSrc) throw(...);
  ~CSslCertificate();

  CSslCertificate& operator=(_In_ const CSslCertificate& cSrc) throw(...);

  HRESULT InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL, _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  LONG GetVersion() const;

  LPBYTE GetSerial() const;
  SIZE_T GetSerialLength() const;

  HRESULT GetSubject(_In_ eInformation nInfo, _Inout_ CStringW &cStrW);
  HRESULT GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW);

  HRESULT GetValidFrom(_Inout_ CDateTime &cDt);
  HRESULT GetValidUntil(_Inout_ CDateTime &cDt);

  HRESULT IsDateValid(); //returns S_OK if valid, S_FALSE if not, or an error
  BOOL IsCaCert() const;

  LPVOID GetOpenSSL_X509();

private:
  LPVOID lpX509;
};

//-----------------------------------------------------------

class CSslCertificateCrl : public virtual CBaseMemObj
{
private:
  CSslCertificateCrl(_In_ const CSslCertificateCrl& cSrc);
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

  CSslCertificateCrl();
  ~CSslCertificateCrl();

  HRESULT InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL, _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  LONG GetVersion() const;

  HRESULT GetIssuer(_In_ eInformation nInfo, _Inout_ CStringW &cStrW);

  HRESULT GetUpdate(_Inout_ CDateTime &cDt);
  HRESULT GetNextUpdate(_Inout_ CDateTime &cDt);

  SIZE_T GetRevokedEntriesCount() const;

  LPBYTE GetRevokedEntrySerial(_In_ SIZE_T nEntryIndex) const;
  SIZE_T GetRevokedEntrySerialLength(_In_ SIZE_T nEntryIndex) const;
  HRESULT GetRevokedEntryDate(_In_ SIZE_T nEntryIndex, _Inout_ CDateTime &cDt);

  HRESULT operator=(const CSslCertificateCrl& cSrc);

  LPVOID GetOpenSSL_X509Crl();

private:
  LPVOID lpX509Crl;
};

//-----------------------------------------------------------

class CSslCertificateArray : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CSslCertificateArray);
public:
  CSslCertificateArray();
  ~CSslCertificateArray();

  VOID Reset();

  HRESULT AddFromMemory(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT AddFromFile(_In_z_ LPCWSTR szFileNameW, _In_opt_z_ LPCSTR szPasswordA=NULL);

  HRESULT AddPublicKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT AddPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL,
                              _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddPrivateKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT AddPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL,
                               _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddCertificateFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT AddCertificateFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL,
                                _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddCrlFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT AddCrlFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL, _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT ImportFromWindowsStore();

public:
  TArrayListWithDelete<CSslCertificate*> cCertsList;
  TArrayListWithDelete<CSslCertificateCrl*> cCertCrlsList;
  TArrayListWithDelete<CCryptoRSA*> cRsaKeysList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SSLCERTIFICATES_H

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
private:
  CSslCertificate(__in const CSslCertificate& cSrc);
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

  CSslCertificate();
  ~CSslCertificate();

  HRESULT InitializeFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen, __in_z_opt LPCSTR szPasswordA=NULL);
  HRESULT InitializeFromPEM(__in LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL, __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  LONG GetVersion() const;

  LPBYTE GetSerial() const;
  SIZE_T GetSerialLength() const;

  HRESULT GetSubject(__in eInformation nInfo, __inout CStringW &cStrW);
  HRESULT GetIssuer(__in eInformation nInfo, __inout CStringW &cStrW);

  HRESULT GetValidFrom(__inout CDateTime &cDt);
  HRESULT GetValidUntil(__inout CDateTime &cDt);

  BOOL IsCaCert() const;

  HRESULT operator=(const CSslCertificate& cSrc);

  LPVOID GetOpenSSL_X509();

private:
  LPVOID lpX509;
};

//-----------------------------------------------------------

class CSslCertificateCrl : public virtual CBaseMemObj
{
private:
  CSslCertificateCrl(__in const CSslCertificateCrl& cSrc);
public:
  typedef enum {
    InfoOrganization = 1, InfoUnit, InfoCommonName, InfoCountry, InfoStateProvince, InfoTown
  } eInformation;

  CSslCertificateCrl();
  ~CSslCertificateCrl();

  HRESULT InitializeFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen);
  HRESULT InitializeFromPEM(__in LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL, __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  LONG GetVersion() const;

  HRESULT GetIssuer(__in eInformation nInfo, __inout CStringW &cStrW);

  HRESULT GetUpdate(__inout CDateTime &cDt);
  HRESULT GetNextUpdate(__inout CDateTime &cDt);

  SIZE_T GetRevokedEntriesCount() const;

  LPBYTE GetRevokedEntrySerial(__in SIZE_T nEntryIndex) const;
  SIZE_T GetRevokedEntrySerialLength(__in SIZE_T nEntryIndex) const;
  HRESULT GetRevokedEntryDate(__in SIZE_T nEntryIndex, __inout CDateTime &cDt);

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

  HRESULT AddFromMemory(__in LPCVOID lpData, __in SIZE_T nDataLen, __in_z_opt LPCSTR szPasswordA=NULL);
  HRESULT AddFromFile(__in_z LPCWSTR szFileNameW, __in_z_opt LPCSTR szPasswordA=NULL);

  HRESULT AddPublicKeyFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen, __in_z_opt LPCSTR szPasswordA=NULL);
  HRESULT AddPublicKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL,
                              __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddPrivateKeyFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen, __in_z_opt LPCSTR szPasswordA=NULL);
  HRESULT AddPrivateKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL,
                               __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddCertificateFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen, __in_z_opt LPCSTR szPasswordA=NULL);
  HRESULT AddCertificateFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL,
                                __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT AddCrlFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen);
  HRESULT AddCrlFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL, __in_opt SIZE_T nPemLen=(SIZE_T)-1);

  HRESULT ImportFromWindowsStore();

public:
  TArrayListWithDelete<CSslCertificate*> cCertsList;
  TArrayListWithDelete<CSslCertificateCrl*> cCertCrlsList;
  TArrayListWithDelete<CCryptoRSA*> cRsaKeysList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SSLCERTIFICATES_H

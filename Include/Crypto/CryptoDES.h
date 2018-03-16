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
#ifndef _MX_CRYPTO_DES_H
#define _MX_CRYPTO_DES_H

#include "BaseCrypto.h"

//-----------------------------------------------------------

namespace MX {

class CCryptoDES : public CBaseCrypto
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCryptoDES);
public:
  typedef enum {
    AlgorithmDesEcb=0, AlgorithmDesCbc, AlgorithmDesCfb,
    AlgorithmDesOfb, AlgorithmDesCfb1, AlgorithmDesCfb8,
    AlgorithmTripleDesEcb, AlgorithmTripleDesCbc, AlgorithmTripleDesCfb,
    AlgorithmTripleDesOfb, AlgorithmTripleDesCfb1, AlgorithmTripleDesCfb8
  } eAlgorithm;

  CCryptoDES();
  ~CCryptoDES();

  HRESULT BeginEncrypt();
  HRESULT BeginEncrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey=NULL, _In_opt_ SIZE_T nKeyLen=0,
                       _In_opt_ LPCVOID lpInitVector=NULL, _In_opt_ BOOL bUsePadding=TRUE);
  HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndEncrypt();

  HRESULT BeginDecrypt();
  HRESULT BeginDecrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey=NULL, _In_opt_ SIZE_T nKeyLen=0,
                       _In_opt_ LPCVOID lpInitVector=NULL, _In_opt_ BOOL bUsePadding=TRUE);
  HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndDecrypt();

private:
  VOID CleanUp(_In_ BOOL bEncoder, _In_ BOOL bZeroData);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CRYPTO_DES_H

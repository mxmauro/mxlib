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
#ifndef _MX_CRYPTO_AES_H
#define _MX_CRYPTO_AES_H

#include "BaseCrypto.h"

//-----------------------------------------------------------

namespace MX {

class CCryptoAES : public CBaseCrypto
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCryptoAES);
public:
  typedef enum {
    AlgorithmAes128Ecb=0, AlgorithmAes128Cbc, AlgorithmAes128Cfb,
    AlgorithmAes128Ofb, AlgorithmAes128Cfb1, AlgorithmAes128Cfb8,
    AlgorithmAes192Ecb, AlgorithmAes192Cbc, AlgorithmAes192Cfb,
    AlgorithmAes192Ofb, AlgorithmAes192Cfb1, AlgorithmAes192Cfb8,
    AlgorithmAes256Ecb, AlgorithmAes256Cbc, AlgorithmAes256Cfb,
    AlgorithmAes256Ofb, AlgorithmAes256Cfb1, AlgorithmAes256Cfb8
  } eAlgorithm;

  CCryptoAES();
  ~CCryptoAES();

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

#endif //_MX_CRYPTO_AES_H

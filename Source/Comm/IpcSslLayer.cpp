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
#include "..\..\Include\Comm\IpcSslLayer.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\CircularBuffer.h"
#include "..\..\Include\Comm\SslCertificates.h"
#include "..\OpenSSL\Source\crypto\x509\x509_lcl.h"
#include "..\OpenSSL\Source\crypto\include\internal\x509_int.h"

//-----------------------------------------------------------

#define FLAG_Disconnected                              0x0001
#define FLAG_HandshakeCompleted                        0x0002
#define FLAG_WantsRead                                 0x0004

#ifdef _DEBUG
  #define SSLLAYER_PRINT_ERROR
#else //_DEBUG
  //#define SSLLAYER_PRINT_ERROR
#endif //_DEBUG

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class COpenSSLCircularBufferBIO : public virtual MX::CBaseMemObj
{
public:
  COpenSSLCircularBufferBIO() : MX::CBaseMemObj()
    {
    _InterlockedExchange(&nMutex, 0);
    return;
    };

public:
  LONG volatile nMutex;
  MX::CCircularBuffer cBuffer;
};

} //namespace Internals

} //namespace MX

typedef struct tagSSL_LAYER_DATA {
  LONG volatile nFlags;
  BOOL bServerSide;
  MX::CIpcSslLayer::eProtocol nProtocol;
  //----
  SSL_CTX *lpSslCtx;
  SSL *lpSslSession;
  BIO_METHOD *lpBioMthd;
  BIO *lpInBio;
  BIO *lpOutBio;
  MX::Internals::COpenSSLCircularBufferBIO *lpBufferedOut;
  MX::CSslCertificateArray *lpCertArray;
  struct tagPeerCert {
    MX::CSslCertificate *lpCert;
    BOOL bIsValid;
  } sPeerCert;
} SSL_LAYER_DATA;

#define ssl_data ((SSL_LAYER_DATA*)lpInternalData)

//-----------------------------------------------------------

static int my_X509_STORE_get1_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x);
static STACK_OF(X509) *my_X509_STORE_get1_certs(X509_STORE_CTX *ctx, X509_NAME *nm);
static STACK_OF(X509_CRL) *my_X509_STORE_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm);
static X509* lookup_cert_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name);
static X509_CRL* lookup_crl_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name);

#ifdef SSLLAYER_PRINT_ERROR
static int DebugPrintSslError(const char *str, size_t len, void *u);
#endif //SSLLAYER_PRINT_ERROR

static BIO_METHOD *BIO_circular_buffer_mem();

//-----------------------------------------------------------

namespace MX {

CIpcSslLayer::CIpcSslLayer() : CIpc::CLayer()
{
  _InterlockedExchange(&nMutex, 0);
  _InterlockedExchange(&hNetworkError, 0);
  lpInternalData = NULL;
  if (Internals::OpenSSL::Init() != FALSE)
  {
    lpInternalData = MX_MALLOC(sizeof(SSL_LAYER_DATA));
    if (lpInternalData != NULL)
      MemSet(lpInternalData, 0, sizeof(SSL_LAYER_DATA));
  }
  return;
}

CIpcSslLayer::~CIpcSslLayer()
{
  if (lpInternalData != NULL)
  {
    if (ssl_data->lpSslSession != NULL)
    {
      SSL_set_session(ssl_data->lpSslSession, NULL);
      SSL_free(ssl_data->lpSslSession);
      ssl_data->lpSslSession = NULL;
    }
    MX_DELETE(ssl_data->lpBufferedOut);
    MX_DELETE(ssl_data->sPeerCert.lpCert);
    if (ssl_data->lpBioMthd != NULL)
    {
      BIO_meth_free(ssl_data->lpBioMthd);
      ssl_data->lpBioMthd = NULL;
    }
    MemSet(lpInternalData, 0, sizeof(SSL_LAYER_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CIpcSslLayer::Initialize(_In_ BOOL bServerSide, _In_ eProtocol nProtocol,
                                 _In_opt_ CSslCertificateArray *lpCheckCertificates, _In_opt_ CSslCertificate *lpSelfCert,
                                 _In_opt_ CCryptoRSA *lpPrivKey)
{
  X509_STORE *lpStore;

  if (nProtocol != ProtocolTLSv1_0 && nProtocol != ProtocolTLSv1_1 && nProtocol != ProtocolTLSv1_2)
    return E_INVALIDARG;
  if (bServerSide != FALSE && lpSelfCert == NULL)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  ssl_data->bServerSide = bServerSide;
  ssl_data->nProtocol = nProtocol;
  //create SSL context
  ERR_clear_error();
  switch (nProtocol)
  {
    case ProtocolTLSv1_0:
      ssl_data->lpSslCtx = Internals::OpenSSL::GetSslContext(bServerSide, "tls1.0");
      break;
    case ProtocolTLSv1_1:
      ssl_data->lpSslCtx = Internals::OpenSSL::GetSslContext(bServerSide, "tls1.1");
      break;
    case ProtocolTLSv1_2:
      ssl_data->lpSslCtx = Internals::OpenSSL::GetSslContext(bServerSide, "tls1.2");
      break;
  }
  if (ssl_data->lpSslCtx == NULL)
    return E_OUTOFMEMORY;
  //create session
  ssl_data->lpSslSession = SSL_new(ssl_data->lpSslCtx);
  if (ssl_data->lpSslSession == NULL)
    return E_OUTOFMEMORY;
  //init some stuff
  SSL_set_verify(ssl_data->lpSslSession, SSL_VERIFY_NONE, NULL);
  SSL_set_verify_depth(ssl_data->lpSslSession, 4);
  SSL_set_options(ssl_data->lpSslSession, SSL_OP_NO_SSLv2|SSL_OP_NO_COMPRESSION|SSL_OP_LEGACY_SERVER_CONNECT|SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
  SSL_set_mode(ssl_data->lpSslSession, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_RELEASE_BUFFERS);
  if (ssl_data->bServerSide == FALSE)
    SSL_set_connect_state(ssl_data->lpSslSession);
  else
    SSL_set_accept_state(ssl_data->lpSslSession);
  //create i/o objects
  ssl_data->lpBufferedOut = MX_DEBUG_NEW MX::Internals::COpenSSLCircularBufferBIO();
  if (ssl_data->lpBufferedOut == NULL)
    return E_OUTOFMEMORY;
  ssl_data->lpBioMthd = BIO_circular_buffer_mem();
  if (ssl_data->lpBioMthd == NULL)
    return E_OUTOFMEMORY;
  ssl_data->lpInBio = BIO_new(ssl_data->lpBioMthd);
  if (ssl_data->lpInBio == NULL)
    return E_OUTOFMEMORY;
  ssl_data->lpOutBio = BIO_new(ssl_data->lpBioMthd);
  if (ssl_data->lpOutBio == NULL)
  {
    BIO_free(ssl_data->lpInBio);
    ssl_data->lpInBio = NULL;
    return E_OUTOFMEMORY;
  }
  SSL_set_bio(ssl_data->lpSslSession, ssl_data->lpInBio, ssl_data->lpOutBio);
  if (SSL_set_ex_data(ssl_data->lpSslSession, 0, (void*)ssl_data) <= 0)
    return E_OUTOFMEMORY;
  lpStore = X509_STORE_new();
  if (lpStore == NULL)
    return E_OUTOFMEMORY;
  SSL_ctrl(ssl_data->lpSslSession, SSL_CTRL_SET_CHAIN_CERT_STORE, 0, lpStore); //don't add reference
  SSL_ctrl(ssl_data->lpSslSession, SSL_CTRL_SET_VERIFY_CERT_STORE, 1, lpStore); //do add reference
  if (X509_STORE_set_ex_data(lpStore, 0, (void*)ssl_data) <= 0)
    return E_OUTOFMEMORY;
  ((x509_store_st*)lpStore)->get_issuer = &my_X509_STORE_get1_issuer;
  ((x509_store_st*)lpStore)->lookup_certs = &my_X509_STORE_get1_certs;
  ((x509_store_st*)lpStore)->lookup_crls = &my_X509_STORE_get1_crls;
  //setup server/client certificate if provided
  ssl_data->lpCertArray = lpCheckCertificates;
  if (lpSelfCert != NULL)
  {
    if (lpSelfCert->GetOpenSSL_X509() == NULL)
      return MX_E_NotReady;
    if (SSL_use_certificate(ssl_data->lpSslSession, (X509*)(lpSelfCert->GetOpenSSL_X509())) <= 0)
      return Internals::OpenSSL::GetLastErrorCode(TRUE);
    //setup private key if provided
    if (lpPrivKey != NULL)
    {
      TAutoFreePtr<BYTE> aTempBuf;
      SIZE_T nSize;

      nSize = lpPrivKey->GetPrivateKey(NULL);
      if (nSize == 0)
        return E_INVALIDARG;
      aTempBuf.Attach((LPBYTE)MX_MALLOC(nSize));
      if (!aTempBuf)
        return E_OUTOFMEMORY;
      lpPrivKey->GetPrivateKey(aTempBuf.Get());
      if (SSL_use_PrivateKey_ASN1(EVP_PKEY_RSA, ssl_data->lpSslSession, aTempBuf.Get(), (long)nSize) <= 0)
        return Internals::OpenSSL::GetLastErrorCode(TRUE);
      if (SSL_check_private_key(ssl_data->lpSslSession) == 0)
        return MX_E_InvalidData;
    }
  }
  //done
  return S_OK;
}

HRESULT CIpcSslLayer::OnConnect()
{
  HRESULT hRes;

  hRes = HandleSsl(TRUE);
  //done
  if (FAILED(hRes))
    _InterlockedCompareExchange(&hNetworkError, (LONG)hRes, 0);
  //done
  return hRes;
}

HRESULT CIpcSslLayer::OnDisconnect()
{
  if (lpInternalData == NULL)
    return E_FAIL;
  _InterlockedOr(&(ssl_data->nFlags), FLAG_Disconnected);
  //shutdown
  {
    CFastLock cLock(&nMutex);

    ERR_clear_error();
    SSL_shutdown(ssl_data->lpSslSession);
  }
  //process ssl
  return HandleSsl(FALSE);
}

HRESULT CIpcSslLayer::OnData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
{
  HRESULT hRes;

  if (lpInternalData == NULL)
    return E_FAIL;
  hRes = (HRESULT)__InterlockedRead(&hNetworkError);
  if (FAILED(hRes))
    return hRes;
  if (nDataSize == 0)
    return S_OK;
  //add to buffered input
  hRes = (BIO_write(ssl_data->lpInBio, lpData, (int)nDataSize) == (int)nDataSize) ? S_OK : E_OUTOFMEMORY;
  //process ssl
  if (SUCCEEDED(hRes))
  {
    _InterlockedAnd(&(ssl_data->nFlags), ~FLAG_WantsRead);
    hRes = HandleSsl(TRUE);
  }
  //done
  if (FAILED(hRes))
    _InterlockedCompareExchange(&hNetworkError, (LONG)hRes, 0);
  return hRes;
}

HRESULT CIpcSslLayer::OnSendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
{
  HRESULT hRes;

  if (lpInternalData == NULL)
    return E_FAIL;
  hRes = (HRESULT)__InterlockedRead(&hNetworkError);
  if (FAILED(hRes))
    return hRes;
  if (nDataSize == 0)
    return S_OK;
  //DebugPrint("CIpcSslLayer::OnSendMsg %lums\n", ::GetTickCount());
  //add to buffered output
  {
    CFastLock cBufferedOutLock(&(ssl_data->lpBufferedOut->nMutex));

    hRes = ssl_data->lpBufferedOut->cBuffer.Write(lpData, nDataSize);
  }
  //process ssl
  if (SUCCEEDED(hRes))
    hRes = HandleSsl(TRUE);
  //done
  if (FAILED(hRes))
    _InterlockedCompareExchange(&hNetworkError, (LONG)hRes, 0);
  return hRes;
}

HRESULT CIpcSslLayer::HandleSsl(_In_ BOOL bCanWrite)
{
  CFastLock cLock(&nMutex);
  BOOL bLoop = TRUE;
  LONG nCurrFlags;
  HRESULT hRes;

  while (bLoop != FALSE)
  {
    bLoop = FALSE;
    nCurrFlags = __InterlockedRead(&(ssl_data->nFlags));
    //process encrypted input
    hRes = ExecSslRead();
    if (FAILED(hRes))
      return hRes;
    if (hRes == S_OK)
      bLoop = TRUE;
    //process unencrypted output
    if ((nCurrFlags & FLAG_WantsRead) == 0)
    {
      hRes = ProcessDataToSend();
      if (FAILED(hRes))
        return hRes;
      if (hRes == S_OK)
        bLoop = TRUE;
    }
    //send encrypted output
    if (bCanWrite != FALSE)
    {
      hRes = SendEncryptedOutput();
      if (FAILED(hRes))
        return hRes;
      if (hRes == S_OK)
        bLoop = TRUE;
    }
    //process handshake
    if ((nCurrFlags & FLAG_HandshakeCompleted) == 0)
    {
      if (SSL_in_init(ssl_data->lpSslSession) == 0)
      {
        _InterlockedOr(&(ssl_data->nFlags), FLAG_HandshakeCompleted);
        hRes = FinalizeHandshake();
        if (FAILED(hRes))
          return hRes;
        bLoop = TRUE;
      }
    }
  }
  //done
  return S_OK;
}

HRESULT CIpcSslLayer::ExecSslRead()
{
  BYTE aBuffer[4096];
  int r;
  BOOL bSomethingProcessed;
  HRESULT hRes;

  bSomethingProcessed = FALSE;
  do
  {
    ERR_clear_error();
    r = SSL_read(ssl_data->lpSslSession, aBuffer, (int)sizeof(aBuffer));
    if (r > 0)
    {
      hRes = SendProcessedDataToNextLayer(aBuffer, (SIZE_T)r);
      if (FAILED(hRes))
        return hRes;
      bSomethingProcessed = TRUE;
    }
  }
  while (r > 0);
  switch (SSL_get_error(ssl_data->lpSslSession, r))
  {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_NONE:
    case SSL_ERROR_ZERO_RETURN:
      hRes = (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
      break;

    default:
#ifdef SSLLAYER_PRINT_ERROR
      ERR_print_errors_cb(&DebugPrintSslError, "ExecSslRead");
#endif //SSLLAYER_PRINT_ERROR
      hRes = MX_E_InvalidData;
      break;
  }
  //done
  return hRes;
}

HRESULT CIpcSslLayer::SendEncryptedOutput()
{
  BYTE aBuffer[4096];
  BOOL bSomethingProcessed;
  SIZE_T nAvailable;
  int nToProcess;
  HRESULT hRes;

  bSomethingProcessed = FALSE;
  //send processed output from ssl engine
  while ((nAvailable = BIO_ctrl_pending(ssl_data->lpOutBio)) > 0)
  {
    if (nAvailable > sizeof(aBuffer))
      nAvailable = sizeof(aBuffer);
    nToProcess = BIO_read(ssl_data->lpOutBio, aBuffer, (int)nAvailable);
    if (nToProcess <= 0)
      break;
    hRes = SendMsgToNextLayer(aBuffer, nToProcess);
    if (FAILED(hRes))
      return hRes;
    bSomethingProcessed = TRUE;
  }
  //done
  return (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpcSslLayer::ProcessDataToSend()
{
  BYTE aTempBuf[2048];
  BOOL bSomethingWritten;
  SIZE_T nAvail;
  int err;

  bSomethingWritten = FALSE;
  do
  {
    ERR_clear_error();
    {
      CFastLock cBufferedOutLock(&(ssl_data->lpBufferedOut->nMutex));

      nAvail = ssl_data->lpBufferedOut->cBuffer.Peek(aTempBuf, sizeof(aTempBuf));
      if (nAvail > 0)
      {
        err = SSL_write(ssl_data->lpSslSession, aTempBuf, (int)nAvail);
        if (err > 0)
        {
          ssl_data->lpBufferedOut->cBuffer.AdvanceReadPtr((SIZE_T)err);
          bSomethingWritten = TRUE;
        }
      }
      else
      {
        err = 1;
        break;
      }
    }
  }
  while (err > 0);
  switch (SSL_get_error(ssl_data->lpSslSession, err))
  {
    case SSL_ERROR_WANT_READ:
      _InterlockedOr(&(ssl_data->nFlags), FLAG_WantsRead);
      //fall into...

    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_NONE:
      break;

    default:
#ifdef SSLLAYER_PRINT_ERROR
      ERR_print_errors_cb(&DebugPrintSslError, "ProcessDataToSend");
#endif //SSLLAYER_PRINT_ERROR
      return MX_E_InvalidData;
  }
  //done
  return (bSomethingWritten != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpcSslLayer::FinalizeHandshake()
{
  unsigned char *lpTempBuf[2];
  int r, nLen;
  X509 *lpPeer;
  HRESULT hRes;

  ERR_clear_error();
  r = SSL_get_verify_result(ssl_data->lpSslSession);
  ssl_data->sPeerCert.bIsValid = (r == X509_V_OK) ? TRUE : FALSE;
  lpPeer = SSL_get_peer_certificate(ssl_data->lpSslSession);
  if (lpPeer != NULL)
  {
    nLen = i2d_X509(lpPeer, NULL);
    lpTempBuf[0] = lpTempBuf[1] = (unsigned char*)OPENSSL_malloc(nLen);
    if (lpTempBuf[0] == NULL)
      return E_OUTOFMEMORY;
    nLen = i2d_X509(lpPeer, &lpTempBuf[1]);
    if (nLen < 0)
    {
      OPENSSL_free(lpTempBuf[0]);
      return MX_E_InvalidData;
    }
    ssl_data->sPeerCert.lpCert = MX_DEBUG_NEW CSslCertificate();
    if (ssl_data->sPeerCert.lpCert == NULL)
    {
      OPENSSL_free(lpTempBuf[0]);
      return E_OUTOFMEMORY;
    }
    hRes = ssl_data->sPeerCert.lpCert->InitializeFromDER(lpTempBuf[0], (SIZE_T)nLen);
    OPENSSL_free(lpTempBuf[0]);
    if (FAILED(hRes))
    {
      MX_DELETE(ssl_data->sPeerCert.lpCert);
      return hRes;
    }
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static int my_X509_STORE_get1_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)
{
  X509_STORE *lpStore;
  SSL_LAYER_DATA *lpSslLayerData;
  X509 *cert;

  lpStore = X509_STORE_CTX_get0_store(ctx);
  lpSslLayerData = (SSL_LAYER_DATA*)X509_STORE_get_ex_data(lpStore, 0);
  if (lpSslLayerData->lpCertArray != NULL)
  {
    cert = lookup_cert_by_subject(lpSslLayerData->lpCertArray, X509_get_issuer_name(x));
    if (cert != NULL && ctx->check_issued(ctx, x, cert))
    {
      X509_up_ref(cert);
      *issuer = cert;
      return 1;
    }
  }
  return 0;
}

static STACK_OF(X509) *my_X509_STORE_get1_certs(X509_STORE_CTX *ctx, X509_NAME *nm)
{
  X509_STORE *lpStore;
  SSL_LAYER_DATA *lpSslLayerData;
  STACK_OF(X509) *sk;
  X509 *cert;

  lpStore = X509_STORE_CTX_get0_store(ctx);
  lpSslLayerData = (SSL_LAYER_DATA*)X509_STORE_get_ex_data(lpStore, 0);
  if (lpSslLayerData->lpCertArray != NULL)
  {
    cert = lookup_cert_by_subject(lpSslLayerData->lpCertArray, nm);
    if (cert != NULL)
    {
      sk = sk_X509_new_null();
      if (sk != NULL)
      {
        if (sk_X509_push(sk, cert) > 0)
        {
          X509_up_ref(cert);
          return sk;
        }
        sk_X509_free(sk);
      }
    }
  }
  return NULL;
}

static STACK_OF(X509_CRL) *my_X509_STORE_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm)
{
  X509_STORE *lpStore;
  SSL_LAYER_DATA *lpSslLayerData;
  STACK_OF(X509_CRL) *sk;
  X509_CRL *cert;

  lpStore = X509_STORE_CTX_get0_store(ctx);
  lpSslLayerData = (SSL_LAYER_DATA*)X509_STORE_get_ex_data(lpStore, 0);
  if (lpSslLayerData->lpCertArray != NULL)
  {
    cert = lookup_crl_by_subject(lpSslLayerData->lpCertArray, nm);
    if (cert != NULL)
    {
      sk = sk_X509_CRL_new_null();
      if (sk != NULL)
      {
        if (sk_X509_CRL_push(sk, cert) > 0)
        {
          X509_CRL_up_ref(cert);
          return sk;
        }
        sk_X509_CRL_free(sk);
      }
    }
  }
  return NULL;
  //return X509_STORE_get1_crls(ctx, nm);
}

static X509* lookup_cert_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name)
{
  X509 *lpX509;
  MX::CSslCertificate *lpCert;
  SIZE_T i, nCount;

  if (lpCertArray == NULL || name == NULL)
    return NULL;
  nCount = lpCertArray->cCertsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpCert = lpCertArray->cCertsList.GetElementAt(i);
    lpX509 = (X509*)(lpCert->GetOpenSSL_X509());
    if (lpX509 != NULL && X509_NAME_cmp(X509_get_subject_name(lpX509), name) == 0 &&
        X509_cmp_time(X509_get_notBefore(lpX509), NULL) < 0 &&
        X509_cmp_time(X509_get_notAfter(lpX509), NULL) > 0)
    {
      return lpX509;
    }
  }
  return NULL;
}

static X509_CRL* lookup_crl_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name)
{
  X509_CRL *lpX509Crl;
  MX::CSslCertificateCrl *lpCertCrl;
  SIZE_T i, nCount;

  if (lpCertArray == NULL || name == NULL)
    return NULL;
  nCount = lpCertArray->cCertCrlsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpCertCrl = lpCertArray->cCertCrlsList.GetElementAt(i);
    lpX509Crl = (X509_CRL*)(lpCertCrl->GetOpenSSL_X509Crl());
    if (lpX509Crl != NULL && X509_NAME_cmp(X509_CRL_get_issuer(lpX509Crl), name) == 0)
      return lpX509Crl;
  }
  return NULL;
}

#ifdef SSLLAYER_PRINT_ERROR
static int DebugPrintSslError(const char *str, size_t len, void *u)
{
  MX::DebugPrint("IpcSslLayer SSL error(%s): %.*s\n", (char*)u, len, str);
  return 1;
}
#endif //SSLLAYER_PRINT_ERROR


//-----------------------------------------------------------

static int circular_buffer_new(BIO *bi);
static int circular_buffer_free(BIO *bi);
static int circular_buffer_read(BIO *bi, char *out, int outl);
static int circular_buffer_write(BIO *bi, const char *in, int inl);
static long circular_buffer_ctrl(BIO *bi, int cmd, long num, void *ptr);
static int circular_buffer_gets(BIO *bi, char *buf, int size);
static int circular_buffer_puts(BIO *bi, const char *str);

static int circular_buffer_new(BIO *bi)
{
  MX::Internals::COpenSSLCircularBufferBIO *lpBuf;

  lpBuf = MX_DEBUG_NEW MX::Internals::COpenSSLCircularBufferBIO();
  if (lpBuf == NULL)
    return 0;
  BIO_set_data(bi, lpBuf);
  BIO_set_init(bi, 1);
  BIO_set_shutdown(bi, 1);
  return 1;
}

static int circular_buffer_free(BIO *bi)
{
  MX::Internals::COpenSSLCircularBufferBIO *lpBuf;

  if (bi == NULL)
    return 0;
  lpBuf = (MX::Internals::COpenSSLCircularBufferBIO*)BIO_get_data(bi);
  if (lpBuf != NULL && BIO_get_shutdown(bi) != 0 && BIO_get_init(bi) != 0)
  {
    delete lpBuf;
    BIO_set_data(bi, NULL);
  }
  return 1;
}

static int circular_buffer_read(BIO *bi, char *out, int outl)
{
  MX::Internals::COpenSSLCircularBufferBIO *lpBuf;
  int ret;

  lpBuf = (MX::Internals::COpenSSLCircularBufferBIO*)BIO_get_data(bi);
  {
    MX::CFastLock cLock(&(lpBuf->nMutex));

    BIO_clear_retry_flags(bi);
    ret = 0;
    if (out != NULL && outl > 0)
      ret = (int)(lpBuf->cBuffer.Read(out, (SIZE_T)outl));
    if (ret <= 0)
    {
      BIO_set_retry_read(bi);
      ret = -1;
    }
  }
  return ret;
}

static int circular_buffer_write(BIO *bi, const char *in, int inl)
{
  MX::Internals::COpenSSLCircularBufferBIO *lpBuf;

  if (in == NULL)
  {
    BIOerr(BIO_F_MEM_WRITE, BIO_R_NULL_PARAMETER);
    return -1;
  }
  if (BIO_test_flags(bi, BIO_FLAGS_MEM_RDONLY))
  {
    BIOerr(BIO_F_MEM_WRITE, BIO_R_WRITE_TO_READ_ONLY_BIO);
    return -1;
  }

  lpBuf = (MX::Internals::COpenSSLCircularBufferBIO*)BIO_get_data(bi);
  if (inl > 0)
  {
    MX::CFastLock cLock(&(lpBuf->nMutex));

    BIO_clear_retry_flags(bi);
    if (FAILED(lpBuf->cBuffer.Write(in, (SIZE_T)inl)))
    {
      BIOerr(BIO_F_MEM_WRITE, ERR_R_MALLOC_FAILURE);
      return -1;
    }
  }
  return inl;
}

static long circular_buffer_ctrl(BIO *bi, int cmd, long num, void *ptr)
{
  MX::Internals::COpenSSLCircularBufferBIO *lpBuf;

  lpBuf = (MX::Internals::COpenSSLCircularBufferBIO*)BIO_get_data(bi);
  {
    MX::CFastLock cLock(&(lpBuf->nMutex));

    switch (cmd)
    {
      case BIO_CTRL_RESET:
        lpBuf->cBuffer.SetBufferSize(0);
        return 1;

      case BIO_CTRL_GET_CLOSE:
        return (long)BIO_get_shutdown(bi);

      case BIO_CTRL_SET_CLOSE:
        BIO_set_shutdown(bi, (int)num);
        return 1;

      case BIO_CTRL_PENDING:
        {
        SIZE_T nAvail = lpBuf->cBuffer.GetAvailableForRead();

        if (nAvail > (SIZE_T)LONG_MAX)
          nAvail = (SIZE_T)LONG_MAX;
        return (long)nAvail;
        }
        break;

      case BIO_CTRL_DUP:
      case BIO_CTRL_FLUSH:
        return 1;
    }
  }
  return 0;
}

static int circular_buffer_gets(BIO *bi, char *buf, int size)
{
  if (buf == NULL)
  {
    BIOerr(BIO_F_BIO_READ, BIO_R_NULL_PARAMETER);
    return -1;
  }
  BIO_clear_retry_flags(bi);
  if (size > 0)
  {
    MX::Internals::COpenSSLCircularBufferBIO *lpBuf;
    SIZE_T i, nToRead, nReaded;
    char *bufEnd, *bufStart;

    lpBuf = (MX::Internals::COpenSSLCircularBufferBIO*)BIO_get_data(bi);
    bufStart = buf;
    bufEnd = buf + (size-1);
    {
      MX::CFastLock cLock(&(lpBuf->nMutex));

      while (bufStart < bufEnd)
      {
        nToRead = (SIZE_T)(bufEnd - bufStart);
        nReaded = lpBuf->cBuffer.Peek(bufStart, nToRead);
        if (nReaded == 0)
          break;
        for (i=0; i<nReaded; i++)
        {
          if (bufStart[i] == '\n')
          {
            nReaded = i+1;
            break;
          }
        }
        bufStart += nReaded;
        lpBuf->cBuffer.AdvanceReadPtr(nReaded);
        if (*(bufStart-1) == '\n')
          break;
      }
      *bufStart = 0;
    }
    return (int)(bufStart - buf);
  }
  return 0;
}

static int circular_buffer_puts(BIO *bi, const char *str)
{
  return circular_buffer_write(bi, str, (int)MX::StrLenA(str));
}

static BIO_METHOD *BIO_circular_buffer_mem()
{
  BIO_METHOD *lpBioMthd;

  lpBioMthd = BIO_meth_new(BIO_TYPE_MEM, "circular memory buffer");
  if (lpBioMthd != NULL)
  {
    BIO_meth_set_write(lpBioMthd, &circular_buffer_write);
    BIO_meth_set_read(lpBioMthd, &circular_buffer_read);
    BIO_meth_set_puts(lpBioMthd, &circular_buffer_puts);
    BIO_meth_set_gets(lpBioMthd, &circular_buffer_gets);
    BIO_meth_set_ctrl(lpBioMthd, &circular_buffer_ctrl);
    BIO_meth_set_create(lpBioMthd, &circular_buffer_new);
    BIO_meth_set_destroy(lpBioMthd, &circular_buffer_free);
  }
  return lpBioMthd;
}

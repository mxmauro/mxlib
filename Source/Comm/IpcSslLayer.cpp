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
#include "..\..\Include\Comm\IpcSslLayer.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\CircularBuffer.h"
#include "..\..\Include\Comm\SslCertificates.h"
#include <intrin.h>
static FORCEINLINE int InterlockedExchangeAdd(_Inout_ _Interlocked_operand_ int volatile *Addend, _In_ int Value)
{
  return (int)_InterlockedExchangeAdd((LONG volatile *)Addend, (LONG)Value);
}
#include "..\OpenSSL\Source\include\internal\bio.h"
#include "..\OpenSSL\Source\crypto\x509\x509_local.h"

//-----------------------------------------------------------

#define FLAG_Disconnected                              0x0001
#define FLAG_HandshakeCompleted                        0x0002
#define FLAG_WantsRead                                 0x0004

//-----------------------------------------------------------

typedef struct {
  MX::CIpcSslLayer *lpLayer;
  LPCWSTR szDescW;
} DEBUGPRINT_DATA, *LPDEBUGPRINT_DATA;

typedef struct tagSSL_LAYER_DATA {
  LONG volatile nFlags;
  BOOL bServerSide;
  //----
  SSL_CTX *lpSslCtx;
  SSL *lpSslSession;
  BIO *lpInBio;
  BIO *lpOutBio;
  MX::CIpc::CPacketList *lpOutgoingPacketsList;
  SIZE_T nSizeUsedInLastPacket;
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

static int DebugPrintSslError(const char *str, size_t len, void *u);

static BIO_METHOD *BIO_circular_buffer_mem();

//-----------------------------------------------------------

namespace MX {

CIpcSslLayer::CIpcSslLayer(_In_opt_ CLoggable *lpLogParent) : CIpc::CLayer(), CLoggable()
{
  SetLogParent(lpLogParent);
  _InterlockedExchange(&nMutex, 0);
  _InterlockedExchange(&hNetworkError, 0);
  lpInternalData = NULL;
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
    MX_DELETE(ssl_data->lpOutgoingPacketsList);
    MX_DELETE(ssl_data->sPeerCert.lpCert);
    MxMemSet(lpInternalData, 0, sizeof(SSL_LAYER_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CIpcSslLayer::Initialize(_In_ BOOL bServerSide, _In_opt_ LPCSTR szHostNameA,
                                 _In_opt_ CSslCertificateArray *lpCheckCertificates,
                                 _In_opt_ CSslCertificate *lpSelfCert, _In_opt_ CEncryptionKey *lpPrivKey)
{
  X509_STORE *lpStore;
  HRESULT hRes;

  if (bServerSide != FALSE && lpSelfCert == NULL)
    return E_POINTER;
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(SSL_LAYER_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MxMemSet(lpInternalData, 0, sizeof(SSL_LAYER_DATA));
  }
  ssl_data->bServerSide = bServerSide;
  //create SSL context
  ERR_clear_error();
  ssl_data->lpSslCtx = Internals::OpenSSL::GetSslContext(bServerSide);
  if (ssl_data->lpSslCtx == NULL)
    return E_OUTOFMEMORY;
  //create session
  ssl_data->lpSslSession = SSL_new(ssl_data->lpSslCtx);
  if (ssl_data->lpSslSession == NULL)
    return E_OUTOFMEMORY;
  //init some stuff
  SSL_set_verify(ssl_data->lpSslSession, SSL_VERIFY_NONE, NULL);
  SSL_set_verify_depth(ssl_data->lpSslSession, 4);
  SSL_set_options(ssl_data->lpSslSession, SSL_OP_NO_COMPRESSION | SSL_OP_LEGACY_SERVER_CONNECT |
                                          SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
  SSL_set_mode(ssl_data->lpSslSession, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                                       SSL_MODE_RELEASE_BUFFERS);
  if (ssl_data->bServerSide == FALSE)
  {
    SSL_set_connect_state(ssl_data->lpSslSession);
    if (szHostNameA != NULL && *szHostNameA != 0)
    {
      ERR_clear_error();
      if (SSL_set_tlsext_host_name(ssl_data->lpSslSession, szHostNameA) <= 0)
      {
        int err = ERR_get_error();
        return (err == ((ERR_LIB_SSL << 24) | (SSL_F_SSL3_CTRL << 12) | SSL_R_SSL3_EXT_INVALID_SERVERNAME))
               ? MX_E_InvalidData : E_OUTOFMEMORY;
      }
    }
  }
  else
  {
    SSL_set_accept_state(ssl_data->lpSslSession);
  }
  //create i/o objects
  ssl_data->lpOutgoingPacketsList = MX_DEBUG_NEW MX::CIpc::CPacketList();
  if (ssl_data->lpOutgoingPacketsList == NULL)
    return E_OUTOFMEMORY;
  ssl_data->lpInBio = BIO_new(BIO_circular_buffer_mem());
  if (ssl_data->lpInBio == NULL)
    return E_OUTOFMEMORY;
  ssl_data->lpOutBio = BIO_new(BIO_circular_buffer_mem());
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
  X509_STORE_set_get_issuer(lpStore, &my_X509_STORE_get1_issuer);
  X509_STORE_set_lookup_certs(lpStore, &my_X509_STORE_get1_certs);
  X509_STORE_set_lookup_crls(lpStore, &my_X509_STORE_get1_crls);
  //setup server/client certificate if provided
  ssl_data->lpCertArray = lpCheckCertificates;
  if (lpSelfCert != NULL)
  {
    if (lpSelfCert->GetX509() == NULL)
      return MX_E_NotReady;
    if (SSL_use_certificate(ssl_data->lpSslSession, lpSelfCert->GetX509()) <= 0)
      return Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    //setup private key if provided
    if (lpPrivKey != NULL)
    {
      TAutoFreePtr<BYTE> aTempBuf;
      TAutoDeletePtr<CSecureBuffer> cBuffer;

      hRes = lpPrivKey->GetPrivateKey(&cBuffer);
      if (FAILED(hRes))
        return hRes;
      ERR_clear_error();
      if (SSL_use_PrivateKey_ASN1(EVP_PKEY_RSA, ssl_data->lpSslSession, cBuffer->GetBuffer(),
                                  (long)(cBuffer->GetLength())) <= 0)
      {
        return Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
      }
      if (SSL_check_private_key(ssl_data->lpSslSession) == 0)
        return MX_E_InvalidData;
    }
  }
  //done
  return S_OK;
}

HRESULT CIpcSslLayer::OnConnect()
{
  CFastLock cLock(&nMutex);
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
  CFastLock cLock(&nMutex);

  if (lpInternalData == NULL)
    return MX_E_NotReady;
  _InterlockedOr(&(ssl_data->nFlags), FLAG_Disconnected);
  //shutdown
  ERR_clear_error();
  SSL_shutdown(ssl_data->lpSslSession);
  //process ssl
  return HandleSsl(FALSE);
}

VOID CIpcSslLayer::OnShutdown()
{
  if (lpInternalData != NULL)
  {
    CIpc::CPacketBase *lpPacket;

    while ((lpPacket = ssl_data->lpOutgoingPacketsList->DequeueFirst()) != NULL)
    {
      FreePacket(lpPacket);
      DecrementOutgoingWrites();
    }
  }
  return;
}

HRESULT CIpcSslLayer::OnReceivedData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
{
  HRESULT hRes;

  if (lpInternalData == NULL)
    return MX_E_NotReady;
  hRes = (HRESULT)__InterlockedRead(&hNetworkError);
  if (FAILED(hRes))
    return hRes;
  if (nDataSize > 0)
  {
    CFastLock cLock(&nMutex);

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
  }
  return hRes;
}

HRESULT CIpcSslLayer::OnSendPacket(_In_ CIpc::CPacketBase *lpPacket)
{
  HRESULT hRes;

  if (lpInternalData == NULL)
  {
    FreePacket(lpPacket);
    return MX_E_NotReady;
  }
  hRes = (HRESULT)__InterlockedRead(&hNetworkError);
  if (SUCCEEDED(hRes))
  {
    CFastLock cLock(&nMutex);

    //DebugPrint("CIpcSslLayer::OnSendMsg %lums\n", ::GetTickCount());
    //add to the outgoing list
    IncrementOutgoingWrites();
    ssl_data->lpOutgoingPacketsList->QueueLast(lpPacket);

    //process ssl
    hRes = HandleSsl(TRUE);

    //done
    if (FAILED(hRes))
      _InterlockedCompareExchange(&hNetworkError, (LONG)hRes, 0);
  }
  else
  {
    FreePacket(lpPacket);
  }
  return hRes;
}

HRESULT CIpcSslLayer::HandleSsl(_In_ BOOL bCanWrite)
{
  SIZE_T nProcessedOutgoingPackets;
  CIpc::CPacketBase *lpAfterWritePacket;
  BOOL bLoop;
  LONG nCurrFlags;
  HRESULT hRes;

restart:
  lpAfterWritePacket = NULL;
  bLoop = TRUE;

  nProcessedOutgoingPackets = 0;
  while (bLoop != FALSE)
  {
    hRes = S_OK;
    bLoop = FALSE;
    nCurrFlags = __InterlockedRead(&(ssl_data->nFlags));
    //process encrypted input
    hRes = ExecSslRead();
    if (FAILED(hRes))
      break;
    if (hRes == S_OK)
    {
      bLoop = TRUE;
      nCurrFlags &= ~FLAG_WantsRead;
      _InterlockedAnd(&(ssl_data->nFlags), ~FLAG_WantsRead);
    }
    //process unencrypted output
    if ((nCurrFlags & FLAG_WantsRead) == 0 && lpAfterWritePacket == NULL)
    {
      hRes = ProcessOutgoingData(&nProcessedOutgoingPackets, &lpAfterWritePacket);
      if (FAILED(hRes))
        break;
      if (hRes == S_OK)
        bLoop = TRUE;
    }
    //send encrypted output
    if (bCanWrite != FALSE)
    {
      hRes = SendEncryptedOutput();
      if (FAILED(hRes))
        break;
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
          break;
        //NOTE: Should we raise a callback?
        bLoop = TRUE;
      }
    }
  }
  if (hRes == S_FALSE)
    hRes = S_OK;
  //count outgoing packets
  while (nProcessedOutgoingPackets > 0)
  {
    DecrementOutgoingWrites();
    nProcessedOutgoingPackets--;
  }
  //process the after write packet if someone appeared
  if (lpAfterWritePacket != NULL)
  {
    if (SUCCEEDED(hRes))
    {
      hRes = SendAfterWritePacketToNextLayer(lpAfterWritePacket);
    }
    else
    {
      FreePacket(lpAfterWritePacket);
    }
    DecrementOutgoingWrites();
    if (SUCCEEDED(hRes))
      goto restart;
  }
  //done
  return hRes;
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
      hRes = SendReadDataToNextLayer(aBuffer, (SIZE_T)r);
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
      break;

    default:
      if (ShouldLog(1) != FALSE)
      {
        DEBUGPRINT_DATA sData = { this, L"ExecSslRead" };
        ERR_print_errors_cb(&DebugPrintSslError, &sData);
      }
      return MX_E_InvalidData;
  }
  //done
  return (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
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
    hRes = SendWriteDataToNextLayer(aBuffer, nToProcess);
    if (FAILED(hRes))
      return hRes;
    bSomethingProcessed = TRUE;
  }
  //done
  return (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpcSslLayer::ProcessOutgoingData(_Inout_ PSIZE_T lpnProcessedPackets,
                                          _Inout_ CIpc::CPacketBase **lplpAfterWritePacket)
{
  BOOL bSomethingWritten;
  int err;

  bSomethingWritten = FALSE;
  do
  {
    CIpc::CPacketBase *lpPacket;
    SIZE_T nAvail;

    lpPacket = ssl_data->lpOutgoingPacketsList->DequeueFirst();
    if (lpPacket == NULL)
    {
      err = 1;
      break;
    }
    if (lpPacket->HasAfterWriteSignalCallback() != FALSE)
    {
      *lplpAfterWritePacket = lpPacket;
      err = 1;
      break;
    }

    if (lpPacket->HasStream() != FALSE)
    {
      CIpc::CPacketBase *lpNewPacket;
      HRESULT hRes;

      hRes = ReadStream(lpPacket, &lpNewPacket);
      if (hRes == MX_E_EndOfFileReached)
      {
        //free the stream
        FreePacket(lpPacket);
        (*lpnProcessedPackets)++;

        err = 1;
        continue;
      }
      else if (FAILED(hRes))
      {
        return hRes;
      }

      //we have a data packet so increment the outgoing packets and assume we are dealing with a normal data packet
      //
      //the packet we be queued in the front if the whole data cannot be processed in this round
      //
      //also requeue the stream
      ssl_data->lpOutgoingPacketsList->QueueFirst(lpPacket);

      IncrementOutgoingWrites();
      lpPacket = lpNewPacket;
    }

    //get available bytes (taking into account send fragmentation)
    nAvail = lpPacket->GetBytesInUse() - ssl_data->nSizeUsedInLastPacket;
    if (nAvail == 0)
    {
      FreePacket(lpPacket);
      (*lpnProcessedPackets)++;

      err = 1;
      break;
    }

    ERR_clear_error();
    err = SSL_write(ssl_data->lpSslSession, lpPacket->GetBuffer() + ssl_data->nSizeUsedInLastPacket, (int)nAvail);
    if (err > 0)
    {
      bSomethingWritten = TRUE;

      if (err == (int)nAvail)
      {
        FreePacket(lpPacket);
        (*lpnProcessedPackets)++;

        ssl_data->nSizeUsedInLastPacket = 0;
      }
      else
      {
        ssl_data->nSizeUsedInLastPacket += (SIZE_T)err;
        //requeue packet for next round
        ssl_data->lpOutgoingPacketsList->QueueFirst(lpPacket);

        //stop
        err = 1;
        break;
      }
    }
    else
    {
      //couldn't send so requeue packet for next round
      ssl_data->lpOutgoingPacketsList->QueueFirst(lpPacket);
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
      if (ShouldLog(1) != FALSE)
      {
        DEBUGPRINT_DATA sData = { this, L"ProcessDataToSend" };
        ERR_print_errors_cb(&DebugPrintSslError, &sData);
      }
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
    {
      X509_free(lpPeer);
      return E_OUTOFMEMORY;
    }
    nLen = i2d_X509(lpPeer, &lpTempBuf[1]);
    X509_free(lpPeer);
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
    if (cert != NULL)
    {
      X509_STORE_CTX_check_issued_fn fnCheckIssued = X509_STORE_CTX_get_check_issued(ctx);

      if (fnCheckIssued(ctx, x, cert) != 0)
      {
        X509_up_ref(cert);
        *issuer = cert;
        return 1;
      }
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
    lpX509 = lpCert->GetX509();
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
    lpX509Crl = lpCertCrl->GetX509Crl();
    if (lpX509Crl != NULL && X509_NAME_cmp(X509_CRL_get_issuer(lpX509Crl), name) == 0)
      return lpX509Crl;
  }
  return NULL;
}

static int DebugPrintSslError(const char *str, size_t len, void *u)
{
  while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    len--;
  ((LPDEBUGPRINT_DATA)u)->lpLayer->Log(L"IpcSslLayer/%s: Error: %.*S\n", ((LPDEBUGPRINT_DATA)u)->szDescW,
                                       (unsigned int)len, str);
  return 1;
}

//-----------------------------------------------------------

static int circular_buffer_create(BIO *bi)
{
  MX::CCircularBuffer *lpBuf = MX_DEBUG_NEW MX::CCircularBuffer();
  if (lpBuf == NULL)
    return 0;
  BIO_set_data(bi, lpBuf);
  BIO_set_init(bi, 1);
  BIO_set_shutdown(bi, 1);
  return 1;
}

static int circular_buffer_destroy(BIO *bi)
{
  MX::CCircularBuffer *lpBuf;

  if (bi == NULL)
    return 0;
  lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  if (lpBuf != NULL && BIO_get_shutdown(bi) != 0 && BIO_get_init(bi) != 0)
  {
    delete lpBuf;
    BIO_set_data(bi, NULL);
  }
  return 1;
}

static int circular_buffer_read_old(BIO *bi, char *out, int outl)
{
  MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer *)BIO_get_data(bi);
  SIZE_T nAvailable;
  int ret;

  BIO_clear_retry_flags(bi);
  nAvailable = lpBuf->GetAvailableForRead();
  ret = (outl > 0 && (SIZE_T)outl > nAvailable) ? (int)nAvailable : outl;
  if (out != NULL && ret > 0)
  {
    ret = (int)(lpBuf->Read(out, (SIZE_T)ret));
  }
  else if (nAvailable == 0)
  {
    ret = -1;
    BIO_set_retry_read(bi);
  }
  return ret;
}

static int circular_buffer_read(BIO *bio, char *data, size_t datal, size_t *readbytes)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = circular_buffer_read_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *readbytes = 0;
    return ret;
  }
  *readbytes = (size_t)ret;
  return 1;
}

static int circular_buffer_write_old(BIO *bi, const char *in, int inl)
{
  MX::CCircularBuffer *lpBuf;

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

  BIO_clear_retry_flags(bi);
  lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  if (inl > 0)
  {
    if (FAILED(lpBuf->Write(in, (SIZE_T)inl)))
    {
      BIOerr(BIO_F_MEM_WRITE, ERR_R_MALLOC_FAILURE);
      return -1;
    }
  }
  return inl;
}

static int circular_buffer_write(BIO *bio, const char *data, size_t datal, size_t *written)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = circular_buffer_write_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *written = 0;
    return ret;
  }
  *written = (size_t)ret;
  return 1;
}

static long circular_buffer_ctrl(BIO *bi, int cmd, long num, void *ptr)
{
  MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  switch (cmd)
  {
    case BIO_CTRL_RESET:
      lpBuf->SetBufferSize(0);
      return 1;

    case BIO_CTRL_GET_CLOSE:
      return (long)BIO_get_shutdown(bi);

    case BIO_CTRL_SET_CLOSE:
      BIO_set_shutdown(bi, (int)num);
      return 1;

    case BIO_CTRL_PENDING:
      {
      SIZE_T nAvail = lpBuf->GetAvailableForRead();

      if (nAvail > (SIZE_T)LONG_MAX)
        nAvail = (SIZE_T)LONG_MAX;
      return (long)nAvail;
      }
      break;

    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      return 1;
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
    MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer *)BIO_get_data(bi);
    SIZE_T i, nToRead, nReaded;

    nToRead = (SIZE_T)size;
    nReaded = lpBuf->Peek(buf, nToRead);
    if (nReaded == 0)
    {
      *buf = 0;
      return 0;
    }
    for (i = 0; i < nReaded; i++)
    {
      if (buf[i] == '\n')
      {
        i++;
        break;
      }
    }
    lpBuf->AdvanceReadPtr(i);
    buf[i] = 0;
    return (int)i;
  }
  if (buf != NULL)
    *buf = '\0';
  return 0;
}

static int circular_buffer_puts(BIO *bi, const char *str)
{
  return circular_buffer_write_old(bi, str, (int)MX::StrLenA(str));
}

static BIO_METHOD *BIO_circular_buffer_mem()
{
  static const BIO_METHOD circular_buffer_mem = {
    BIO_TYPE_MEM,
    (char*)"circular memory buffer",
    &circular_buffer_write,
    &circular_buffer_write_old,
    &circular_buffer_read,
    &circular_buffer_read_old,
    &circular_buffer_puts,
    &circular_buffer_gets,
    &circular_buffer_ctrl,
    &circular_buffer_create,
    &circular_buffer_destroy,
    NULL
  };
  return (BIO_METHOD*)&circular_buffer_mem;
}

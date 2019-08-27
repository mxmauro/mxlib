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
#include <stdlib.h>
#include "..\Include\FnvHash.h"

//-----------------------------------------------------------

// fnv_32a_buf - perform a 32 bit Fowler/Noll/Vo FNV-1a hash on a buffer
//
// input:
//   buf - start of buffer to hash
//   len - length of buffer in octets
//   hval - previous hash value or 0 if first call
//
// returns:
//   32 bit hash as a static hash type
//
// NOTE: To use the recommended 32 bit FNV-1a hash, use FNV1A_32_INIT as the
//       hval arg on the first call to either fnv_32a_buf() or fnv_32a_str().
Fnv32_t fnv_32a_buf(const void *buf, size_t len, Fnv32_t hval)
{
  unsigned char *bp, *be;

  bp = (unsigned char *)buf;
  be = bp + len;
  // FNV-1a hash each octet of the buffer
  while (bp < be) {
    // xor the bottom with the current octet
    hval ^= (Fnv32_t)*bp++;
    // multiply by the 32 bit FNV magic prime mod 2^32
    hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
  }
  return hval;
}

// fnv_64a_buf - perform a 64 bit Fowler/Noll/Vo FNV-1a hash on a buffer
//
// input:
//   buf - start of buffer to hash
//   len - length of buffer in octets
//   hval - previous hash value or 0 if first call
//
// returns:
//   64 bit hash as a static hash type
//
// NOTE: To use the recommended 64 bit FNV-1a hash, use FNV1A_64_INIT as the
//       hval arg on the first call to either fnv_64a_buf() or fnv_64a_str().
Fnv64_t fnv_64a_buf(const void *buf, size_t len, Fnv64_t hval)
{
  unsigned char *bp, *be;

  bp = (unsigned char *)buf;
  be = bp + len;
  // FNV-1a hash each octet of the buffer
  while (bp < be) {
    // xor the bottom with the current octet
    hval ^= (Fnv64_t)*bp++;
    // multiply by the 64 bit FNV magic prime mod 2^64
    hval += (hval << 1) + (hval << 4) + (hval << 5) + (hval << 7) + (hval << 8) + (hval << 40);
  }
  return hval;
}

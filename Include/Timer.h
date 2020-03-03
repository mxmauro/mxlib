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
#ifndef _MX_TIMER_H
#define _MX_TIMER_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

class CTimer : public virtual CBaseMemObj
{
public:
  CTimer();
  CTimer(_In_ CTimer const&);

  CTimer& operator=(_In_ CTimer const&);

  VOID Reset();

  VOID Mark();
  VOID ResetToLastMark();

  DWORD GetElapsedTimeMs() const;
  DWORD GetStartTimeMs() const;
  DWORD GetMarkTimeMs() const;

private:
  ULARGE_INTEGER uliStart, uliMark, uliFrequency;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_TIMER_H

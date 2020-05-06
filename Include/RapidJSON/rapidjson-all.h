#ifndef _RAPIDJSON_INCLUDEALL_H
#define _RAPIDJSON_INCLUDEALL_H

#include <stdexcept>

#define RAPIDJSON_ASSERT(x) if (!(x)) throw std::runtime_error(#x)
#define RAPIDJSON_NOEXCEPT_ASSERT(x)

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "memorybuffer.h"

//------------------------------------------------------------------------------

namespace rapidjson {

__inline const Value* LookupMember(_In_ const Value &parent, _In_z_ LPCSTR szMemberNameA)
{
  const rapidjson::Value::ConstMemberIterator &member = parent.FindMember(szMemberNameA);
  return (member != parent.MemberEnd()) ? &(member->value) : NULL;
}

} //namespace rapidjson

//------------------------------------------------------------------------------

#endif //_RAPIDJSON_INCLUDEALL_H

#ifndef _RAPIDJSON_INCLUDEALL_H
#define _RAPIDJSON_INCLUDEALL_H

#include <stdexcept>

#define RAPIDJSON_ASSERT(x) if (!(x)) throw std::runtime_error(#x)
#define RAPIDJSON_NOEXCEPT_ASSERT(x)
#define RAPIDJSON_DEFAULT_ALLOCATOR MemoryPoolAllocator<CMxLibAllocator>

//------------------------------------------------------------------------------

namespace rapidjson {

class CMxLibAllocator
{
public:
  static const bool kNeedFree = true;

  void* Malloc(size_t size)
    {
    return MX_MALLOC(size);
    };

  void* Realloc(void *originalPtr, size_t originalSize, size_t newSize)
    {
    UNREFERENCED_PARAMETER(originalSize);
    return MX_REALLOC(originalPtr, newSize);
    };

  static void Free(void *ptr)
    {
    MX_FREE(ptr);
    return;
    };
};

} //namespace rapidjson

//------------------------------------------------------------------------------

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

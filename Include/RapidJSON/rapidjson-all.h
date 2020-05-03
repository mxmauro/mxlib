#ifndef _RAPIDJSON_INCLUDEALL_H
#define _RAPIDJSON_INCLUDEALL_H

#include <stdexcept>

#define RAPIDJSON_ASSERT(x) if (x) throw std::runtime_error(#x)
#define RAPIDJSON_NOEXCEPT_ASSERT(x)

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "memorybuffer.h"

#endif //_RAPIDJSON_INCLUDEALL_H

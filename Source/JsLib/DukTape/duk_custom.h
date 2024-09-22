#undef DUK_UNREF
#define DUK_UNREF(x)

//--------------------------------

#undef DUK_USE_MARK_AND_SWEEP

//--------------------------------

#undef DUK_USE_CPP_EXCEPTIONS
#define DUK_USE_CPP_EXCEPTIONS

//--------------------------------

#undef DUK_USE_FATAL_HANDLER
#define DUK_USE_FATAL_HANDLER(udata, msg) throw E_FAIL;

//--------------------------------

#undef DUK_USE_PROVIDE_DEFAULT_ALLOC_FUNCTIONS

//--------------------------------

#define DUK_USE_REGEXP_CANON_WORKAROUND

#define DUK_USE_JSON_STRINGIFY_FASTPATH

//--------------------------------

#undef DUK_USE_VERBOSE_ERRORS
#define DUK_USE_VERBOSE_ERRORS

#undef DUK_USE_AUGMENT_ERROR_THROW
#undef DUK_USE_AUGMENT_ERROR_CREATE
#define DUK_USE_AUGMENT_ERROR_CREATE

//--------------------------------

#undef DUK_MEMCPY
#define DUK_MEMCPY       ::MxMemCopy

#undef DUK_MEMMOVE
#define DUK_MEMMOVE      ::MxMemMove

#undef DUK_MEMCMP
#define DUK_MEMCMP       ::MxMemCompare

#undef DUK_MEMSET
#define DUK_MEMSET       ::MxMemSet

//--------------------------------

#undef DUK_STRLEN
#define DUK_STRLEN       MX::StrLenA

#undef DUK_STRCMP
#define DUK_STRCMP       DukTapeStrCmp
static int DukTapeStrCmp(_In_z_ const char *str1, _In_z_ const char *str2)
{
  return MX::StrCompareA(str1, str2, FALSE);
}

#undef DUK_STRNCMP
#define DUK_STRNCMP      DukTapeStrNCmp
static int DukTapeStrNCmp(_In_reads_or_z_(count) const char *str1,
                          _In_reads_or_z_(count) const char *str2,
                          _In_ size_t const count)
{
  return MX::StrNCompareA(str1, str2, count, FALSE);
}

#undef DUK_SPRINTF
#define DUK_SPRINTF     DukTapeSprintf
static int DukTapeSprintf(_Pre_notnull_ _Always_(_Post_z_) char *const buffer,
                          _In_z_ _Printf_format_string_ const char *format, ...)
{
  va_list argptr;
  int ret;

  va_start(argptr, format);
  ret = vsprintf_s(buffer, 1024, format, argptr);
  va_end(argptr);
  return ret;
}
#undef DUK_SNPRINTF
#define DUK_SNPRINTF     DukTapeSnprintf
static int DukTapeSnprintf(_Out_writes_opt_(bufferSize) _Always_(_Post_z_) char *const buffer,
                           _In_ size_t const bufferSize,
                           _In_z_ _Printf_format_string_ const char *format, ...)
{
  va_list argptr;
  int res;

  va_start(argptr, format);
  res = vsnprintf_s(buffer, bufferSize, _TRUNCATE, format, argptr);
  va_end(argptr);
  return res;
}

#undef DUK_VSNPRINTF
#define DUK_VSNPRINTF    DukTapeVsnprintf
static int DukTapeVsnprintf(_Out_writes_opt_(bufferSize) _Always_(_Post_z_) char *const buffer,
                            _In_ size_t const bufferSize,
                            _In_z_ _Printf_format_string_ const char *format,
                            _In_ va_list argptr)
{
  return vsnprintf_s(buffer, bufferSize, _TRUNCATE, format, argptr);
}

//--------------------------------

#undef DUK_USE_ASSERTIONS
#undef DUK_USE_DEBUG
#ifdef _DEBUG
  #define DUK_USE_ASSERTIONS
  #define DUK_USE_DEBUG
#endif //_DEBUG
#ifdef DUK_USE_DEBUG
  extern LONG nDebugLevel;

  #define DUK_USE_DEBUG
  #undef DUK_USE_DEBUG_LEVEL
  #define DUK_USE_DEBUG_LEVEL 0
  #define DUK_USE_DEBUG_WRITE(level,file,line,func,msg) if (nDebugLevel > (LONG)level) {                   \
    MX::DebugPrint("DukTape: D%ld %s:%d (%s): %s\n", (long)(level), (file), (long) (line), (func), (msg)); \
  }
#endif //DUK_USE_DEBUG

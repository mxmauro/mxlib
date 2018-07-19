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

//--------------------------------

#undef DUK_USE_VERBOSE_ERRORS
#define DUK_USE_VERBOSE_ERRORS

#undef DUK_USE_AUGMENT_ERROR_THROW
#undef DUK_USE_AUGMENT_ERROR_CREATE
#define DUK_USE_AUGMENT_ERROR_CREATE

//--------------------------------

#undef DUK_MEMCPY
#define DUK_MEMCPY       MX::MemCopy

#undef DUK_MEMMOVE
#define DUK_MEMMOVE      MX::MemMove

#undef DUK_MEMCMP
#define DUK_MEMCMP       MX::MemCompare

#undef DUK_MEMSET
#define DUK_MEMSET       MX::MemSet

//--------------------------------

#undef DUK_STRLEN
#define DUK_STRLEN       MX::StrLenA

#undef DUK_STRCMP
#define DUK_STRCMP       DukTapeStrCmp
static __forceinline int DukTapeStrCmp(_In_z_ const char * _Str1, _In_z_ const char * _Str2)
{
  return MX::StrCompareA(_Str1, _Str2, FALSE);
}

#undef DUK_STRNCMP
#define DUK_STRNCMP      DukTapeStrNCmp
static __forceinline int DukTapeStrNCmp(_In_reads_or_z_(_MaxCount) const char * _Str1,
                                        _In_reads_or_z_(_MaxCount) const char * _Str2, _In_ size_t _MaxCount)
{
  return MX::StrNCompareA(_Str1, _Str2, _MaxCount, FALSE);
}

#undef DUK_SPRINTF
#define DUK_SPRINTF     DukTapeSprintf
static __forceinline int DukTapeSprintf(char *buffer, const char *format, ...)
{
  va_list argptr;
  char tempbuf[128];
  int ret;

  va_start(argptr, format);
  ret = mx_vsnprintf(tempbuf, sizeof(tempbuf)/sizeof(tempbuf[0]), format, argptr);
  va_end(argptr);
  if (ret >= 0)
  {
    MX::MemCopy(buffer, tempbuf, ret);
    buffer[ret] = 0;
  }
  else
  {
    buffer[0] = 0;
  }
  return ret;
}
#undef DUK_SNPRINTF
#define DUK_SNPRINTF     mx_sprintf_s
#undef DUK_VSNPRINTF
#define DUK_VSNPRINTF    mx_vsnprintf

//--------------------------------

#undef DUK_USE_ASSERTIONS
#ifdef _DEBUG
  extern LONG nDebugLevel;

  #define DUK_USE_ASSERTIONS

  #define DUK_USE_DEBUG
  #undef DUK_USE_DEBUG_LEVEL
  #define DUK_USE_DEBUG_LEVEL 1
  #define DUK_USE_DEBUG_WRITE(level,file,line,func,msg) if (nDebugLevel > (LONG)level) {                   \
    MX::DebugPrint("DukTape: D%ld %s:%d (%s): %s\n", (long)(level), (file), (long) (line), (func), (msg)); \
  }
#endif //_DEBUG

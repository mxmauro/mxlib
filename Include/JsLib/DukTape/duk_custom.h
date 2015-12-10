#undef DUK_UNREF
#define DUK_UNREF(x)

#undef DUK_ANSI_MALLOC
#undef DUK_ANSI_REALLOC
#undef DUK_ANSI_CALLOC
#undef DUK_ANSI_FREE
#define DUK_ANSI_MALLOC      MX_MALLOC
#define DUK_ANSI_REALLOC     MX_REALLOC
#define DUK_ANSI_CALLOC      DukTapeCalloc
static __forceinline void* DukTapeCalloc(__in size_t _Count, __in size_t _Size)
{
  void *ptr;

  _Count *= _Size;
  ptr = MX_MALLOC(_Count);
  if (ptr != NULL)
    MX::MemSet(ptr, 0, _Count);
  return ptr;
}
#define DUK_ANSI_FREE        MX::MemFree


#undef DUK_MEMCPY
#undef DUK_MEMMOVE
#undef DUK_MEMCMP
#undef DUK_MEMSET
#define DUK_MEMCPY       MX::MemCopy
#define DUK_MEMMOVE      MX::MemMove
#define DUK_MEMCMP       MX::MemCompare
#define DUK_MEMSET       MX::MemSet


#undef DUK_STRLEN
#undef DUK_STRCMP
#undef DUK_STRNCMP
#define DUK_STRLEN       MX::StrLenA
#define DUK_STRCMP       DukTapeStrCmp
static __forceinline int DukTapeStrCmp(_In_z_ const char * _Str1, _In_z_ const char * _Str2)
{
  return MX::StrCompareA(_Str1, _Str2, FALSE);
}
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
  int ret;

  va_start(argptr, format);
  ret = mx_vsnprintf(buffer, 128, format, argptr);
  va_end(argptr);
  return ret;
}
#undef DUK_SNPRINTF
#define DUK_SNPRINTF     mx_sprintf_s
#undef DUK_VSNPRINTF
#define DUK_VSNPRINTF    mx_vsnprintf

//NOTE: Until fixed, I add a copy of duk_debug.h header modified with my custom debug output
#if defined(DUK_OPT_DEBUG) && defined(DUK_COMPILING_DUKTAPE)

#define DUK_DEBUG_H_INCLUDED

#define DUK_DPRINT(...)          MX::DebugPrint(__VA_ARGS__)

#ifdef DUK_USE_DDPRINT
  #undef DUK_DDPRINT
  #define DUK_DDPRINT(...)         MX::DebugPrint(__VA_ARGS__)
#endif &&DUK_USE_DDPRINT

#ifdef DUK_USE_DDDPRINT
  #undef DUK_DDDPRINT
  #define DUK_DDDPRINT(...)        MX::DebugPrint(__VA_ARGS__)
#endif //DUK_USE_DDDPRINT

#define DUK_D(x) x
#define DUK_DD(x) x
#define DUK_DDD(x) x

#define DUK_LEVEL_DEBUG    1
#define DUK_LEVEL_DDEBUG   2
#define DUK_LEVEL_DDDEBUG  3

#define DUK__DEBUG_LOG(lev,...)  mx_duk_debug_log((duk_small_int_t) (lev), DUK_FILE_MACRO, (duk_int_t) DUK_LINE_MACRO, DUK_FUNC_MACRO, __VA_ARGS__);

DUK_LOCAL const char *duk__get_term_1(duk_small_int_t level);
DUK_LOCAL const char *duk__get_term_2(duk_small_int_t level);
DUK_LOCAL const char *duk__get_term_3(duk_small_int_t level);
DUK_LOCAL const char *duk__get_level_string(duk_small_int_t level);

DUK_INTERNAL void mx_duk_debug_log(duk_small_int_t level, const char *file, duk_int_t line, const char *func, const char *fmt, ...)
{
  char buf[1024];
  va_list ap;

  DUK_MEMZERO((void *)buf, 1024);

  va_start(ap, fmt);
  mx_vsnprintf(buf, 1023, fmt, ap);
  va_end(ap);

  MX::DebugPrint("%s[%s] %s:%ld (%s):%s %s%s\n",
                  (const char *)duk__get_term_1(level),
                  (const char *)duk__get_level_string(level),
                  (const char *)file,
                  (long)line,
                  (const char *)func,
                  (const char *)duk__get_term_2(level),
                  (const char *)buf,
                  (const char *)duk__get_term_3(level));
}

#undef DUK_DPRINT
#define DUK_DPRINT(...)          DUK__DEBUG_LOG(DUK_LEVEL_DEBUG, __VA_ARGS__)
#undef DUK_DDPRINT
#define DUK_DDPRINT(...)         DUK__DEBUG_LOG(DUK_LEVEL_DDEBUG, __VA_ARGS__)
#undef DUK_DDDPRINT
#define DUK_DDDPRINT(...)        DUK__DEBUG_LOG(DUK_LEVEL_DDDEBUG, __VA_ARGS__)

struct duk_fixedbuffer
{
  duk_uint8_t *buffer;
  duk_size_t length;
  duk_size_t offset;
  duk_bool_t truncated;
};

DUK_INTERNAL_DECL duk_int_t duk_debug_vsnprintf(char *str, duk_size_t size, const char *format, va_list ap);
DUK_INTERNAL_DECL void duk_debug_format_funcptr(char *buf, duk_size_t buf_size, duk_uint8_t *fptr, duk_size_t fptr_size);

DUK_INTERNAL_DECL void duk_debug_log(duk_small_int_t level, const char *file, duk_int_t line, const char *func, const char *fmt, ...);

DUK_INTERNAL_DECL void duk_fb_put_bytes(duk_fixedbuffer *fb, duk_uint8_t *buffer, duk_size_t length);
DUK_INTERNAL_DECL void duk_fb_put_byte(duk_fixedbuffer *fb, duk_uint8_t x);
DUK_INTERNAL_DECL void duk_fb_put_cstring(duk_fixedbuffer *fb, const char *x);
DUK_INTERNAL_DECL void duk_fb_sprintf(duk_fixedbuffer *fb, const char *fmt, ...);
DUK_INTERNAL_DECL void duk_fb_put_funcptr(duk_fixedbuffer *fb, duk_uint8_t *fptr, duk_size_t fptr_size);
DUK_INTERNAL_DECL duk_bool_t duk_fb_is_full(duk_fixedbuffer *fb);

#endif //DUK_OPT_DEBUG && DUK_COMPILING_DUKTAPE

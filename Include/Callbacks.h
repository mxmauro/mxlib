/*
 * Original code by Elbert Mai (http://www.mauroleggieri.com.ar) published at
 * http://www.codeproject.com/Articles/136799/Lightweight-Generic-C-Callbacks-or-Yet-Another-Del
 *
 * Copyright (C) 2010. All rights reserved.
 *
 * IMPORTANT: The original CodeProject license is located at the bottom of this file to make code more readable.
 *
 * Code slightly modified and adapted to Mauro Leggieri's library.
 **/
#ifndef _MX_CALLBACKS_H
#define _MX_CALLBACKS_H

#include "Defines.h"

//-----------------------------------------------------------

#define MX_BIND_CALLBACK(lpFunction)                       \
     (MX::GetCallbackFactory(lpFunction).Bind<lpFunction>())

#define MX_BIND_MEMBER_CALLBACK(lpMemberFunction, lpInstance) \
     (MX::GetCallbackFactory(lpMemberFunction).Bind<lpMemberFunction>(lpInstance))

//-----------------------------------------------------------

namespace MX {

template<typename FuncSignature>
class Callback;

class NullCallback {};

#define CB_TEMPL_GEN_JOIN_PCOUNT(a, b)   a##b

#define CALLBACK_TEMPLATE_GEN(params_count)                                                         \
template<typename R CBTEMPLGEN_PARAMS_TYPENAME>                                                     \
class Callback<R (CBTEMPLGEN_PARAMS)>                                                               \
{                                                                                                   \
public:                                                                                             \
  static const int Arity = params_count;                                                            \
  typedef R ReturnType;                                                                             \
  CBTEMPLGEN_PARAMS_TYPEDEF                                                                         \
                                                                                                    \
  Callback()                                                                                        \
    {                                                                                               \
    lpObj = NULL;                                                                                   \
    lpFunc = NULL;                                                                                  \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
  Callback(__in NullCallback)                                                                       \
    {                                                                                               \
    lpObj = NULL;                                                                                   \
    lpFunc = NULL;                                                                                  \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
  Callback(__in const Callback& rhs)                                                                \
    {                                                                                               \
    lpObj = rhs.lpObj;                                                                              \
    lpFunc = rhs.lpFunc;                                                                            \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
  Callback& operator=(__in const Callback& rhs)                                                     \
    {                                                                                               \
    lpObj = rhs.lpObj;                                                                              \
    lpFunc = rhs.lpFunc;                                                                            \
    return *this;                                                                                   \
    };                                                                                              \
                                                                                                    \
  Callback& operator=(__in NullCallback)                                                            \
    {                                                                                               \
    lpObj = NULL;                                                                                   \
    lpFunc = NULL;                                                                                  \
    return *this;                                                                                   \
    };                                                                                              \
                                                                                                    \
  ~Callback()                                                                                       \
    { }                                                                                             \
                                                                                                    \
  inline R operator()(CBTEMPLGEN_PARAMS_DEF) const                                                  \
    {                                                                                               \
    return (*lpFunc)(lpObj CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA);                                   \
    };                                                                                              \
                                                                                                    \
  inline bool operator!() const                                                                     \
    {                                                                                               \
    return (lpObj == NULL && lpFunc == NULL);                                                       \
    };                                                                                              \
                                                                                                    \
  inline bool operator==(__in const Callback& rhs)                                                  \
    {                                                                                               \
    return (lpObj == rhs.lpObj || lpFunc == rhs.lpFunc);                                            \
    };                                                                                              \
                                                                                                    \
  inline bool operator==(__in NullCallback)                                                         \
    {                                                                                               \
    return (lpObj == NULL && lpFunc == NULL);                                                       \
    };                                                                                              \
                                                                                                    \
  inline operator bool() const                                                                      \
    {                                                                                               \
    return (lpObj != NULL || lpFunc != NULL);                                                       \
    };                                                                                              \
                                                                                                    \
  void serialize(__in void *p)                                                                      \
    {                                                                                               \
    *((void**)p) = const_cast<void*>(lpObj);                                                        \
    *((FuncType*)((char*)p+sizeof(void*))) = lpFunc;                                                \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
  void deserialize(__in void *p)                                                                    \
    {                                                                                               \
    lpObj = *((void**)p);                                                                           \
    lpFunc = *((FuncType*)((char*)p+sizeof(void*)));                                                \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
  static size_t serialization_buffer_size()                                                         \
    {                                                                                               \
    return 2 * sizeof(void*);                                                                       \
    };                                                                                              \
                                                                                                    \
private:                                                                                            \
  typedef R (*FuncType)(const void* CBTEMPLGEN_PARAMS_WITH_COMMA);                                  \
  Callback(__in const void* _lpObj, __in FuncType _lpFunc)                                          \
    {                                                                                               \
    lpFunc = _lpFunc;                                                                               \
    lpObj = _lpObj;                                                                                 \
    return;                                                                                         \
    };                                                                                              \
                                                                                                    \
private:                                                                                            \
  const void* lpObj;                                                                                \
  FuncType lpFunc;                                                                                  \
                                                                                                    \
  template<typename FR CBTEMPLGEN_PARAMS_TYPENAME_FRIEND>                                           \
  friend class CB_TEMPL_GEN_JOIN_PCOUNT(FreeCallbackFactory, params_count);                         \
  template<typename FR, class FT CBTEMPLGEN_PARAMS_TYPENAME_FRIEND>                                 \
  friend class CB_TEMPL_GEN_JOIN_PCOUNT(MemberCallbackFactory, params_count);                       \
  template<typename FR, class FT CBTEMPLGEN_PARAMS_TYPENAME_FRIEND>                                 \
  friend class CB_TEMPL_GEN_JOIN_PCOUNT(ConstMemberCallbackFactory, params_count);                  \
};                                                                                                  \
                                                                                                    \
template<typename R CBTEMPLGEN_PARAMS_TYPENAME>                                                     \
void operator==(const Callback<R (CBTEMPLGEN_PARAMS)>&, const Callback<R (CBTEMPLGEN_PARAMS)>&);    \
template<typename R CBTEMPLGEN_PARAMS_TYPENAME>                                                     \
void operator!=(const Callback<R (CBTEMPLGEN_PARAMS)>&, const Callback<R (CBTEMPLGEN_PARAMS)>&);    \
                                                                                                    \
template<typename R CBTEMPLGEN_PARAMS_TYPENAME>                                                     \
class CB_TEMPL_GEN_JOIN_PCOUNT(FreeCallbackFactory, params_count)                                   \
{                                                                                                   \
private:                                                                                            \
  template<R (*Func)(CBTEMPLGEN_PARAMS)>                                                            \
  static R Wrapper(const void* CBTEMPLGEN_PARAMS_DEF_WITH_COMMA)                                    \
    {                                                                                               \
    return (*Func)(CBTEMPLGEN_PARAMS_DEF_USE);                                                      \
    };                                                                                              \
                                                                                                    \
public:                                                                                             \
  template<R (*Func)(CBTEMPLGEN_PARAMS)>                                                            \
  inline static Callback<R (CBTEMPLGEN_PARAMS)> Bind()                                              \
    {                                                                                               \
    return Callback<R (CBTEMPLGEN_PARAMS)>                                                          \
                   (NULL, &CB_TEMPL_GEN_JOIN_PCOUNT(FreeCallbackFactory, params_count)::            \
                   Wrapper<Func>);                                                                  \
    };                                                                                              \
};                                                                                                  \
                                                                                                    \
template<typename R CBTEMPLGEN_PARAMS_TYPENAME>                                                     \
inline CB_TEMPL_GEN_JOIN_PCOUNT(FreeCallbackFactory, params_count)<R CBTEMPLGEN_PARAMS_WITH_COMMA>  \
GetCallbackFactory(R (*)(CBTEMPLGEN_PARAMS))                                                        \
{                                                                                                   \
  return CB_TEMPL_GEN_JOIN_PCOUNT(FreeCallbackFactory, params_count)                                \
                                 <R CBTEMPLGEN_PARAMS_WITH_COMMA>();                                \
}                                                                                                   \
                                                                                                    \
template<typename R, class T CBTEMPLGEN_PARAMS_TYPENAME>                                            \
class CB_TEMPL_GEN_JOIN_PCOUNT(MemberCallbackFactory, params_count)                                 \
{                                                                                                   \
private:                                                                                            \
  template<R (T::*Func)(CBTEMPLGEN_PARAMS)>                                                         \
  static R Wrapper(const void* o CBTEMPLGEN_PARAMS_DEF_WITH_COMMA)                                  \
    {                                                                                               \
    T* lpObj = const_cast<T*>(static_cast<const T*>(o));                                            \
    return (lpObj->*Func)(CBTEMPLGEN_PARAMS_DEF_USE);                                               \
    };                                                                                              \
                                                                                                    \
public:                                                                                             \
  template<R (T::*Func)(CBTEMPLGEN_PARAMS)>                                                         \
  inline static Callback<R (CBTEMPLGEN_PARAMS)> Bind(T* o)                                          \
    {                                                                                               \
    return Callback<R (CBTEMPLGEN_PARAMS)>(static_cast<const void*>(o),                             \
                    &CB_TEMPL_GEN_JOIN_PCOUNT(MemberCallbackFactory, params_count)::                \
                    Wrapper<Func>);                                                                 \
    };                                                                                              \
};                                                                                                  \
                                                                                                    \
template<typename R, class T CBTEMPLGEN_PARAMS_TYPENAME>                                            \
inline CB_TEMPL_GEN_JOIN_PCOUNT(MemberCallbackFactory, params_count)                                \
       <R, T CBTEMPLGEN_PARAMS_WITH_COMMA>                                                          \
GetCallbackFactory(R (T::*)(CBTEMPLGEN_PARAMS))                                                     \
{                                                                                                   \
  return CB_TEMPL_GEN_JOIN_PCOUNT(MemberCallbackFactory, params_count)                              \
                                 <R, T CBTEMPLGEN_PARAMS_WITH_COMMA>();                             \
}                                                                                                   \
                                                                                                    \
template<typename R, class T CBTEMPLGEN_PARAMS_TYPENAME>                                            \
class CB_TEMPL_GEN_JOIN_PCOUNT(ConstMemberCallbackFactory, params_count)                            \
{                                                                                                   \
private:                                                                                            \
  template<R (T::*Func)() const>                                                                    \
  static R Wrapper(const void* o CBTEMPLGEN_PARAMS_DEF_WITH_COMMA)                                  \
    {                                                                                               \
    const T* lpObj = static_cast<const T*>(o);                                                      \
    return (lpObj->*Func)(CBTEMPLGEN_PARAMS_DEF_USE);                                               \
    };                                                                                              \
                                                                                                    \
public:                                                                                             \
  template<R (T::*Func)(CBTEMPLGEN_PARAMS) const>                                                   \
  inline static Callback<R (CBTEMPLGEN_PARAMS)> Bind(const T* o)                                    \
    {                                                                                               \
    return Callback<R (CBTEMPLGEN_PARAMS)>(static_cast<const void*>(o),                             \
                    &CB_TEMPL_GEN_JOIN_PCOUNT(ConstMemberCallbackFactory, params_count)::           \
                    Wrapper<Func>);                                                                 \
    };                                                                                              \
};                                                                                                  \
                                                                                                    \
template<typename R, class T CBTEMPLGEN_PARAMS_TYPENAME>                                            \
inline CB_TEMPL_GEN_JOIN_PCOUNT(ConstMemberCallbackFactory, params_count)                           \
       <R, T CBTEMPLGEN_PARAMS_WITH_COMMA>                                                          \
GetCallbackFactory(R (T::*)(CBTEMPLGEN_PARAMS) const)                                               \
{                                                                                                   \
  return CB_TEMPL_GEN_JOIN_PCOUNT(ConstMemberCallbackFactory, params_count)                         \
                                 <R, T CBTEMPLGEN_PARAMS_WITH_COMMA>();                             \
}

//------------------------

#define CBTEMPLGEN_PARAMS_TYPENAME
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#define CBTEMPLGEN_PARAMS
#define CBTEMPLGEN_PARAMS_WITH_COMMA
#define CBTEMPLGEN_PARAMS_DEF
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#define CBTEMPLGEN_PARAMS_DEF_USE
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#define CBTEMPLGEN_PARAMS_TYPEDEF
CALLBACK_TEMPLATE_GEN(0)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1
#define CBTEMPLGEN_PARAMS                    P1
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1
#define CBTEMPLGEN_PARAMS_DEF                P1 a1
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1
#define CBTEMPLGEN_PARAMS_DEF_USE            a1
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type;
CALLBACK_TEMPLATE_GEN(1)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2
#define CBTEMPLGEN_PARAMS                    P1, P2
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type;
CALLBACK_TEMPLATE_GEN(2)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3
#define CBTEMPLGEN_PARAMS                    P1, P2, P3
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type;
CALLBACK_TEMPLATE_GEN(3)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3, typename P4
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3, typename FP4
#define CBTEMPLGEN_PARAMS                    P1, P2, P3, P4
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3, P4
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3, P4 a4
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3, P4 a4
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3, a4
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3, a4
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type; \
                                             typedef P4 Param4Type;
CALLBACK_TEMPLATE_GEN(4)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3, typename P4, typename P5
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3, typename FP4, typename FP5
#define CBTEMPLGEN_PARAMS                    P1, P2, P3, P4, P5
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3, P4, P5
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3, P4 a4, P5 a5
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3, P4 a4, P5 a5
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3, a4, a5
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3, a4, a5
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type; \
                                             typedef P4 Param4Type; typedef P5 Param5Type;
CALLBACK_TEMPLATE_GEN(5)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3, typename P4, typename P5 \
                                             , typename P6
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3, typename FP4, typename FP5 \
                                             , typename FP6
#define CBTEMPLGEN_PARAMS                    P1, P2, P3, P4, P5, P6
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3, P4, P5, P6
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3, a4, a5, a6
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3, a4, a5, a6
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type; \
                                             typedef P4 Param4Type; typedef P5 Param5Type; typedef P6 Param6Type;
CALLBACK_TEMPLATE_GEN(6)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3, typename P4, typename P5 \
                                             , typename P6, typename P7
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3, typename FP4, typename FP5 \
                                             , typename FP6, typename FP7
#define CBTEMPLGEN_PARAMS                    P1, P2, P3, P4, P5, P6, P7
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3, P4, P5, P6, P7
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3, a4, a5, a6, a7
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3, a4, a5, a6, a7
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type; \
                                             typedef P4 Param4Type; typedef P5 Param5Type; typedef P6 Param6Type; \
                                             typedef P7 Param7Type;
CALLBACK_TEMPLATE_GEN(7)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//--------

#define CBTEMPLGEN_PARAMS_TYPENAME           , typename P1, typename P2, typename P3, typename P4, typename P5 \
                                             , typename P6, typename P7, typename P8
#define CBTEMPLGEN_PARAMS_TYPENAME_FRIEND    , typename FP1, typename FP2, typename FP3, typename FP4, typename FP5 \
                                             , typename FP6, typename FP7, typename FP8
#define CBTEMPLGEN_PARAMS                    P1, P2, P3, P4, P5, P6, P7, P8
#define CBTEMPLGEN_PARAMS_WITH_COMMA         , P1, P2, P3, P4, P5, P6, P7, P8
#define CBTEMPLGEN_PARAMS_DEF                P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8
#define CBTEMPLGEN_PARAMS_DEF_WITH_COMMA     , P1 a1, P2 a2, P3 a3, P4 a4, P5 a5, P6 a6, P7 a7, P8 a8
#define CBTEMPLGEN_PARAMS_DEF_USE            a1, a2, a3, a4, a5, a6, a7, a8
#define CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA , a1, a2, a3, a4, a5, a6, a7, a8
#define CBTEMPLGEN_PARAMS_TYPEDEF            typedef P1 Param1Type; typedef P2 Param2Type; typedef P3 Param3Type; \
                                             typedef P4 Param4Type; typedef P5 Param5Type; typedef P6 Param6Type; \
                                             typedef P7 Param7Type; typedef P8 Param8Type;
CALLBACK_TEMPLATE_GEN(8)
#undef CBTEMPLGEN_PARAMS_TYPEDEF
#undef CBTEMPLGEN_PARAMS_DEF_USE_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF_USE
#undef CBTEMPLGEN_PARAMS_DEF_WITH_COMMA
#undef CBTEMPLGEN_PARAMS_DEF
#undef CBTEMPLGEN_PARAMS_WITH_COMMA
#undef CBTEMPLGEN_PARAMS
#undef CBTEMPLGEN_PARAMS_TYPENAME_FRIEND
#undef CBTEMPLGEN_PARAMS_TYPENAME

//------------------------

#undef CALLBACK_TEMPLATE_GEN
#undef CB_TEMPL_GEN_JOIN_PCOUNT

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CALLBACKS_H

/*
 * The Code Project Open License (CPOL) 1.02
 * Preamble
 *
 * This License governs Your use of the Work. This License is intended to allow
 * developers to use the Source Code and Executable Files provided as part of
 * the Work in any application in any form.
 *
 * The main points subject to the terms of the License are:
 *   * Source Code and Executable Files can be used in commercial applications;
 *   * Source Code and Executable Files can be redistributed; and
 *   * Source Code can be modified to create derivative works.
 *   * No claim of suitability, guarantee, or any warranty whatsoever is
 *     provided. The software is provided "as-is".
 *   * The Article accompanying the Work may not be distributed or republished
 *     without the Author's consent
 * This License is entered between You, the individual or other entity reading
 * or otherwise making use of the Work licensed pursuant to this License and the
 * individual or other entity which offers the Work under the terms of this
 * License ("Author").
 *
 * License
 *
 * THE WORK (AS DEFINED BELOW) IS PROVIDED UNDER THE TERMS OF THIS CODE PROJECT
 * OPEN LICENSE ("LICENSE"). THE WORK IS PROTECTED BY COPYRIGHT AND/OR OTHER
 * APPLICABLE LAW. ANY USE OF THE WORK OTHER THAN AS AUTHORIZED UNDER THIS
 * LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED HEREIN, YOU ACCEPT AND AGREE TO
 * BE BOUND BY THE TERMS OF THIS LICENSE. THE AUTHOR GRANTS YOU THE RIGHTS
 * CONTAINED HEREIN IN CONSIDERATION OF YOUR ACCEPTANCE OF SUCH TERMS AND
 * CONDITIONS. IF YOU DO NOT AGREE TO ACCEPT AND BE BOUND BY THE TERMS OF THIS
 * LICENSE, YOU CANNOT MAKE ANY USE OF THE WORK.
 *
 *   1. Definitions.
 *
 *     1. "Articles" means, collectively, all articles written by Author which
 *        describes how the Source Code and Executable Files for the Work may be
 *        used by a user.
 *
 *     2. "Author" means the individual or entity that offers the Work under the
 *        terms of this License.
 *
 *     3. "Derivative Work" means a work based upon the Work or upon the Work
 *        and other pre-existing works.
 *
 *     4. "Executable Files" refer to the executables, binary files,
 *        configuration and any required data files included in the Work.
 *
 *     5. "Publisher" means the provider of the website, magazine, CD-ROM, DVD
 *        or other medium from or by which the Work is obtained by You.
 *
 *     6. "Source Code" refers to the collection of source code and
 *        configuration files used to create the Executable Files.
 *
 *     7. "Standard Version" refers to such a Work if it has not been modified,
 *        or has been modified in accordance with the consent of the Author,
 *        such consent being in the full discretion of the Author.
 *
 *     8. "Work" refers to the collection of files distributed by the Publisher,
 *        including the Source Code, Executable Files, binaries, data files,
 *        documentation, whitepapers and the Articles.
 *
 *     9. "You" is you, an individual or entity wishing to use the Work and
 *        exercise your rights under this License.
 *
 *   2. Fair Use/Fair Use Rights. Nothing in this License is intended to reduce,
 *      limit, or restrict any rights arising from fair use, fair dealing, first
 *      sale or other limitations on the exclusive rights of the copyright owner
 *      under copyright law or other applicable laws.
 *
 *   3. License Grant. Subject to the terms and conditions of this License, the
 *      Author hereby grants You a worldwide, royalty-free, non-exclusive,
 *      perpetual (for the duration of the applicable copyright) license to
 *      exercise the rights in the Work as stated below:
 *
 *     1. You may use the standard version of the Source Code or Executable
 *        Files in Your own applications.
 *
 *     2. You may apply bug fixes, portability fixes and other modifications
 *        obtained from the Public Domain or from the Author. A Work modified in
 *        such a way shall still be considered the standard version and will be
 *        subject to this License.
 *
 *     3. You may otherwise modify Your copy of this Work (excluding the
 *        Articles) in any way to create a Derivative Work, provided that You
 *        insert a prominent notice in each changed file stating how, when and
 *        where You changed that file.
 *
 *     4. You may distribute the standard version of the Executable Files and
 *        Source Code or Derivative Work in aggregate with other (possibly
 *        commercial) programs as part of a larger (possibly commercial)
 *        software distribution.
 *
 *     5. The Articles discussing the Work published in any form by the author
 *        may not be distributed or republished without the Author's consent.
 *        The author retains copyright to any such Articles. You may use the
 *        Executable Files and Source Code pursuant to this License but you may
 *        not repost or republish or otherwise distribute or make available the
 *        Articles, without the prior written consent of the Author.  Any
 *        subroutines or modules supplied by You and linked into the Source Code
 *        or Executable Files of this Work shall not be considered part of this
 *        Work and will not be subject to the terms of this License.
 *
 *   4. Patent License. Subject to the terms and conditions of this License,
 *      each Author hereby grants to You a perpetual, worldwide, non-exclusive,
 *      no-charge, royalty-free, irrevocable (except as stated in this section)
 *      patent license to make, have made, use, import, and otherwise transfer
 *      the Work.
 *
 *   5. Restrictions. The license granted in Section 3 above is expressly made
 *      subject to and limited by the following restrictions:
 *
 *     1. You agree not to remove any of the original copyright, patent,
 *        trademark, and attribution notices and associated disclaimers that may
 *        appear in the Source Code or Executable Files.
 *
 *     2. You agree not to advertise or in any way imply that this Work is a
 *        product of Your own.
 *
 *     3. The name of the Author may not be used to endorse or promote products
 *        derived from the Work without the prior written consent of the Author.
 *
 *     4. You agree not to sell, lease, or rent any part of the Work. This does
 *        not restrict you from including the Work or any part of the Work
 *        inside a larger software distribution that itself is being sold. The
 *        Work by itself, though, cannot be sold, leased or rented.
 *
 *     5. You may distribute the Executable Files and Source Code only under the
 *        terms of this License, and You must include a copy of, or the Uniform
 *        Resource Identifier for, this License with every copy of the
 *        Executable Files or Source Code You distribute and ensure that anyone
 *        receiving such Executable Files and Source Code agrees that the terms
 *        of this License apply to such Executable Files and/or Source Code. You
 *        may not offer or impose any terms on the Work that alter or restrict
 *        the terms of this License or the recipients' exercise of the rights
 *        granted hereunder. You may not sublicense the Work. You must keep
 *        intact all notices that refer to this License and to the disclaimer of
 *        warranties. You may not distribute the Executable Files or Source Code
 *        with any technological measures that control access or use of the Work
 *        in a manner inconsistent with the terms of this License.
 *
 *     6. You agree not to use the Work for illegal, immoral or improper
 *        purposes, or on pages containing illegal, immoral or improper
 *        material. The Work is subject to applicable export laws. You agree to
 *        comply with all such laws and regulations that may apply to the Work
 *        after Your receipt of the Work.
 *
 *   6. Representations, Warranties and Disclaimer. THIS WORK IS PROVIDED "AS
 *      IS", "WHERE IS" AND "AS AVAILABLE", WITHOUT ANY EXPRESS OR IMPLIED
 *      WARRANTIES OR CONDITIONS OR GUARANTEES. YOU, THE USER, ASSUME ALL RISK
 *      IN ITS USE, INCLUDING COPYRIGHT INFRINGEMENT, PATENT INFRINGEMENT,
 *      SUITABILITY, ETC. AUTHOR EXPRESSLY DISCLAIMS ALL EXPRESS, IMPLIED OR
 *      STATUTORY WARRANTIES OR CONDITIONS, INCLUDING WITHOUT LIMITATION,
 *      WARRANTIES OR CONDITIONS OF MERCHANTABILITY, MERCHANTABLE QUALITY OR
 *      FITNESS FOR A PARTICULAR PURPOSE, OR ANY WARRANTY OF TITLE OR
 *      NON-INFRINGEMENT, OR THAT THE WORK (OR ANY PORTION THEREOF) IS CORRECT,
 *      USEFUL, BUG-FREE OR FREE OF VIRUSES. YOU MUST PASS THIS DISCLAIMER ON
 *      WHENEVER YOU DISTRIBUTE THE WORK OR DERIVATIVE WORKS.
 *
 *   7. Indemnity. You agree to defend, indemnify and hold harmless the Author
 *      and the Publisher from and against any claims, suits, losses, damages,
 *      liabilities, costs, and expenses (including reasonable legal or
 *      attorneys’ fees) resulting from or relating to any use of the Work by
 *      You.
 *
 *   8. Limitation on Liability. EXCEPT TO THE EXTENT REQUIRED BY APPLICABLE
 *      LAW, IN NO EVENT WILL THE AUTHOR OR THE PUBLISHER BE LIABLE TO YOU ON
 *      ANY LEGAL THEORY FOR ANY SPECIAL, INCIDENTAL, CONSEQUENTIAL, PUNITIVE
 *      OR EXEMPLARY DAMAGES ARISING OUT OF THIS LICENSE OR THE USE OF THE WORK
 *      OR OTHERWISE, EVEN IF THE AUTHOR OR THE PUBLISHER HAS BEEN ADVISED OF
 *      THE POSSIBILITY OF SUCH DAMAGES.
 *
 *   9. Termination.
 *
 *     1. This License and the rights granted hereunder will terminate
 *        automatically upon any breach by You of any term of this License.
 *        Individuals or entities who have received Derivative Works from You
 *        under this License, however, will not have their licenses terminated
 *        provided such individuals or entities remain in full compliance with
 *        those licenses. Sections 1, 2, 6, 7, 8, 9, 10 and 11 will survive any
 *        termination of this License.
 *
 *     2. If You bring a copyright, trademark, patent or any other infringement
 *        claim against any contributor over infringements You claim are made by
 *        the Work, your License from such contributor to the Work ends
 *        automatically.
 *
 *     3. Subject to the above terms and conditions, this License is perpetual
 *        (for the duration of the applicable copyright in the Work).
 *        Notwithstanding the above, the Author reserves the right to release
 *        the Work under different license terms or to stop distributing the
 *        Work at any time; provided, however that any such election will not
 *        serve to withdraw this License (or any other license that has been, or
 *        is required to be, granted under the terms of this License), and this
 *        License will continue in full force and effect unless terminated as
 *        stated above.
 *
 *   10. Publisher. The parties hereby confirm that the Publisher shall not,
 *       under any circumstances, be responsible for and shall not have any
 *       liability in respect of the subject matter of this License. The
 *       Publisher makes no warranty whatsoever in connection with the Work and
 *       shall not be liable to You or any party on any legal theory for any
 *       damages whatsoever, including without limitation any general, special,
 *       incidental or consequential damages arising in connection to this
 *       license. The Publisher reserves the right to cease making the Work
 *       available to You at any time without notice.
 *
 *   11. Miscellaneous
 *
 *     1. This License shall be governed by the laws of the location of the head
 *        office of the Author or if the Author is an individual, the laws of
 *        location of the principal place of residence of the Author.
 *
 *     2. If any provision of this License is invalid or unenforceable under
 *        applicable law, it shall not affect the validity or enforceability of
 *        the remainder of the terms of this License, and without further action
 *        by the parties to this License, such provision shall be reformed to
 *        the minimum extent necessary to make such provision valid and
 *        enforceable.
 *
 *     3. No term or provision of this License shall be deemed waived and no
 *        breach consented to unless such waiver or consent shall be in writing
 *        and signed by the party to be charged with such waiver or consent.
 *
 *     4. This License constitutes the entire agreement between the parties with
 *        respect to the Work licensed herein. There are no understandings,
 *        agreements or representations with respect to the Work not specified
 *        herein. The Author shall not be bound by any additional provisions
 *        that may appear in any communication from You. This License may not be
 *        modified without the mutual written agreement of the Author and You.
 */

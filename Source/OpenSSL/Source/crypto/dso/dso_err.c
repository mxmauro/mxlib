/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/err.h>
#include "internal/dsoerr.h"

#ifndef OPENSSL_NO_ERR

static const ERR_STRING_DATA DSO_str_reasons[] = {
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_CTRL_FAILED), "control command failed"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_DSO_ALREADY_LOADED), "dso already loaded"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_EMPTY_FILE_STRUCTURE),
    "empty file structure"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_FAILURE), "failure"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_FILENAME_TOO_BIG), "filename too big"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_FINISH_FAILED),
    "cleanup method function failed"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_INCORRECT_FILE_SYNTAX),
    "incorrect file syntax"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_LOAD_FAILED),
    "could not load the shared library"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_NAME_TRANSLATION_FAILED),
    "name translation failed"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_NO_FILENAME), "no filename"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_NULL_HANDLE),
    "a null shared library handle was used"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_SET_FILENAME_FAILED),
    "set filename failed"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_STACK_ERROR),
    "the meth_data stack is corrupt"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_SYM_FAILURE),
    "could not bind to the requested symbol name"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_UNLOAD_FAILED),
    "could not unload the shared library"},
    {ERR_PACK(ERR_LIB_DSO, 0, DSO_R_UNSUPPORTED),
    "functionality not supported"},
    {0, NULL}
};

#endif

int ERR_load_DSO_strings(void)
{
#ifndef OPENSSL_NO_ERR
    if (ERR_func_error_string(DSO_str_reasons[0].error) == NULL)
        ERR_load_strings_const(DSO_str_reasons);
#endif
    return 1;
}

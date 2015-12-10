#ifndef _MX_CONFIG_H_
#define _MX_CONFIG_H_

typedef struct sockaddr_storage {
  short   ss_family;
  char    __ss_pad1[6];
  __int64 __ss_align;
  char    __ss_pad2[112];
} SOCKADDR_STORAGE, *PSOCKADDR_STORAGE;

typedef struct timeval {
  long tv_sec;
  long tv_usec;
} timeval;

#endif //_MX_CONFIG_H_

#ifdef MAC_ENDIAN_H
#include "mac_endian.h"
#elif defined(PEBBLE_ENDIAN_H)
#include "pebble_endian.h"
#else
#ifndef _BSD_SOURCE
#define _BSD_SOURCE 
#endif
#include <endian.h>
#endif


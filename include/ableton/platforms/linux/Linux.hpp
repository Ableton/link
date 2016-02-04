// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <byteswap.h>

#ifndef ntohll
#define ntohll(x)       bswap_64(x)
#endif

#ifndef htonll
#define htonll(x)       bswap_64(x)
#endif

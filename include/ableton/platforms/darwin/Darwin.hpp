// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

// ntohll and htonll are not defined in 10.7 SDK, so we provide a compatibility macro here

#ifndef ntohll
#define ntohll(x)       __DARWIN_OSSwapInt64(x)
#endif

#ifndef htonll
#define htonll(x)       __DARWIN_OSSwapInt64(x)
#endif

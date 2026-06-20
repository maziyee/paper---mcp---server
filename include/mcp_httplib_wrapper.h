#pragma once

// MinGW-w64 compatibility: some newer Windows APIs are not yet available
// in MinGW headers. Undefine features that require them.
#ifdef __MINGW32__
#ifdef CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO
#undef CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO
#endif
#endif

#include <httplib.h>

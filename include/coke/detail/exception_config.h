#ifndef COKE_DETAIL_EXCEPTION_CONFIG_H
#define COKE_DETAIL_EXCEPTION_CONFIG_H

#include <system_error>

#if defined(__GNUC__) || defined(__clang__)
#   ifndef __EXCEPTIONS
#       define COKE_NO_EXCEPTIONS
#   endif
#endif

#ifdef COKE_NO_EXCEPTIONS
#define coke_try if (true)
#define coke_catch(E) if (false)
#else
#define coke_try try
#define coke_catch(E) catch(E)
#endif

namespace coke::detail {

void throw_system_error(std::errc);

}

#endif // COKE_DETAIL_EXCEPTION_CONFIG_H

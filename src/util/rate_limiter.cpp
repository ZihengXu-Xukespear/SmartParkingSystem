// =============================================================================
//  SmartParkingSystem - rate_limiter.cpp
// =============================================================================
#include "rate_limiter.h"

#include <algorithm>

namespace sps::util {

// TokenBucketLimiter is fully header-defined; this TU exists so the linker
// always has a strong definition for any out-of-line helpers we add in the
// future (e.g. metrics export). Today it is intentionally empty.
namespace {
[[maybe_unused]] int rate_limiter_anchor = 0;
}  // namespace

}  // namespace sps::util
// =============================================================================
//  SmartParkingSystem - cache.cpp
//  Force-instantiate the template members used by the rest of the system so
//  that the linker keeps them. This is the single .cpp file that owns the
//  vtables for LruCache<std::string, std::string> and friends.
// =============================================================================
#include "cache.h"

namespace sps::util {

// The LruCache template is normally header-only, but we force one common
// instantiation here so that the executable always links in the destructor
// and move-assignment paths even when the relevant call sites live in a
// different translation unit.
template class LruCache<std::string, std::string>;
template class LruCache<int, std::string>;
template class LruCache<long, std::string>;
template class LruCache<std::string, int>;
template class LruCache<std::string, long>;
template class LruCache<std::string, double>;

}  // namespace sps::util
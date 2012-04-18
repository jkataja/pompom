/**
 * Common typedefs and macros.
 *
 * @author jkataja
 */

#pragma once

#include <boost/cstdint.hpp>

#define L(x) ((int)x)
#define R(x) ((int)x+1)

namespace pompom {

// Common typedefs for different sizes of integers 
typedef boost::int64_t int64;
typedef boost::uint64_t uint64;
typedef boost::int32_t int32;
typedef boost::uint32_t uint32;
typedef boost::int16_t int16;
typedef boost::uint16_t uint16;
typedef boost::uint8_t uint8;

} // namespace

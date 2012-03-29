/**
 * Compression and decompression procedures.
 *
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <boost/cstdint.hpp>

#include "pompomdefs.hpp"

namespace pompom {

// id for error messages
static const char SELF[] = "pompom";

// Number of symbols in alphabet without escape and EOS
static const uint16 Alpha = 255;

// Code for escape symbol
static const uint16 Escape = 256;

// Code for end of stream symbol
static const uint16 EOS = 257;

// Compressed file magic header
static const char Magia[] = "pim";

// Model order
static const uint8 OrderMin = 0;
static const uint8 OrderDefault = 3;
static const uint8 OrderMax = 8;

// Model memory limits 
static const uint16 LimitMin = 8;
static const uint16 LimitDefault = 32;
static const uint16 LimitMax = 2048;

// Number of bits in a code value 
static const uint8 CodeValueBits = 16;

// Largest code value
static const uint64 TopValue = (((uint64) 1 << CodeValueBits) - 1);

long decompress(std::istream&, std::ostream&, std::ostream&);

long compress(std::istream&, std::ostream&, std::ostream&, const uint8, const uint16);

} // namespace

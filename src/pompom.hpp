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
static const int Alpha = 255;

// Code for escape symbol
static const int Escape = (Alpha + 1);

// Code for end of stream symbol
static const int EOS = (Alpha + 2);

// Compressed file magic header
static const char Magia[] = "pim";

// Bootstrap buffer length limits
static const int BootMin = 1;
static const int BootDefault = 32;
static const int BootMax = 255;

// Model order limits
static const int OrderMin = 1;
static const int OrderDefault = 3;
static const int OrderMax = 6;

// Model memory limits 
static const int LimitMin = 8;
static const int LimitDefault = 32;
static const int LimitMax = 2048;

// Default for max n bytes
static const int CountDefault = 0;

// Number of bits in a code value 
static const int CodeValueBits = 32;

// Largest code value in range
static const uint64 TopValue = (((uint64) 1 << CodeValueBits) - 1);

// Maximum frequency for encoding
static const uint64 MaxFrequency = (((uint64) 1 << 16) - 1);

// Point after first quarter in range
static const uint64 FirstQuarter = (TopValue/4+1);

// Point after first half in range
static const uint64 Half = (2*FirstQuarter);

// Point after third quarter in range
static const uint64 ThirdQuarter = (3*FirstQuarter);

long decompress(std::istream&, std::ostream&, std::ostream&);

long compress(std::istream&, std::ostream&, std::ostream&, 
	const int, const int, const long, const bool, const int);

} // namespace

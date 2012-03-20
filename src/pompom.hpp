/**
 * Compression and decompression procedures.
 *
 * @author jkataja
 */

#pragma once

#include <iostream>

#define L(x) ((int)x)
#define R(x) ((int)x+1)

namespace pompom {

// id for error messages
static const char SELF[] = "pompom";

// Number of symbols in alphabet without escape and EOS
static const unsigned short Alpha = 255;

// Code for escape symbol
static const unsigned short Escape = 256;

// Code for end of stream symbol
static const unsigned short EOS = 257;

// Compressed file magic header
static const char Magia[] = "pim";

// Fixed length prediction order
// TODO allow for variable order
static const unsigned short Order = 3;

// Model memory limits 
static const unsigned short LimitMin = 8;
static const unsigned short LimitDefault = 32;
static const unsigned short LimitMax = 2048;

long decompress(std::istream&, std::ostream&, std::ostream&);

long compress(std::istream&, std::ostream&, std::ostream&, const std::string&);

} // namespace

/**
 * Arithmetic encoder.
 * 
 * Based on Witten, I.H., Neal, R. and Cleary, J.G. (1987)
 * Arithmetic coding for data compression,'' Comm ACM: 30(6): 520-540; June;
 * 
 * Based on Mark Nelson (1991)
 * Arithmetic Coding + Statistical Modeling = Data Compression
 * Dr. Dobb's Journal February, 1991 
 *
 * Modified to use 64 bit variables.
 * 
 * @see ftp://ftp.cpsc.ucalgary.ca/pub/projects/ar.cod/
 * @see http://www.dogma.net/markn/articles/arith/part1.htm
 * @author jkataja
 */

#pragma once

#include <iostream>

#include "pompomdefs.hpp"

namespace pompom {

class encoder {
public:
	// Encode a symbol.
	inline void encode(const uint16, const uint32[]);

	// Length of output byte
	const uint64 len() const;

	// Write bit buffer and closing fluff
	void finish();

	encoder(std::ostream&);
	~encoder();
private:
	encoder(const encoder&);
	const encoder& operator=(const encoder&);

	std::ostream& out;
	
	static const uint32 WriteBufSize = 32768;
	char * buf;
	uint32 p;
	uint8 bitp;
	uint8 byte;
	uint64 outlen;

	// High end of the current code region
	uint64 high;

	// Low end of the current code region
	uint64 low;

	// Number of byte to follow next
	uint64 byte_to_follow;

	inline void bit_plus_follow(const bool);
	inline void bit_write(const bool);
	inline void flush_bits();
	inline void flush();
};

encoder::encoder(std::ostream& proxy)
	: out(proxy), p(0), bitp(0), byte(0), outlen(0),
	  high(TopValue), low(0), byte_to_follow(0) 
{
	buf = new char[WriteBufSize];
	memset(buf, 0, sizeof(char) * WriteBufSize);
}

encoder::~encoder() {
	delete [] buf;
}

void encoder::encode(const uint16 c, const uint32 * dist) {
#ifndef UNSAFE
	if (c > EOS) {
		throw std::range_error("c not in code range");
	}
#endif

	// Size of the current code region
	uint64 range = (uint64) (high - low) + 1;
	// Narrow the code region  to that allocated to this symbol
	uint32 totalrange = dist[ R(EOS) ];
	high = low + ((range * dist[ R(c)] ) / totalrange) - 1;
	low = low + ((range * dist[ L(c)] ) / totalrange);

#ifdef DEBUG
	std::cerr << "encode\t" << range 
		<< "\t< " << dist[ L(c) ] << " , " << dist[ R(c) ] << " > " 
		<< "\t < " << low << " , " << high << " > ";
	if (c == 256)
		std::cerr << "\t " << c << " ESC" << std::endl;
	else if (c >= 0x20 && c <= '~')
		std::cerr << "\t " << c << " '" << (char)c << "'" << std::endl;
	else
		std::cerr << "\t " << c << std::endl;
#endif

	// Loop to output bits
	while (true) {
		// Output matching high bit 
		if ((high & (1 << (CodeValueBits - 1))) 
				== (low & (1 << (CodeValueBits - 1)))) {
			bit_plus_follow(high >> (CodeValueBits - 1));
		}
		// Output an opposite bit
		else if ((low & FirstQuarter) && !(high & FirstQuarter)) {
			// later if in middle half
			++byte_to_follow;
			// subtract offset to middle
			low &= (FirstQuarter - 1);
			high |= FirstQuarter;
		}
		// Otherwise exit loop
		else
			break;

		// Scale up code range
		low = ((low << 1) & TopValue);
		high = (((high << 1) | 1) & TopValue);

#ifdef DEBUG
		std::cerr << "\t\t\t\t\t" 
			<< " < " << low << " , " << high << " > "
			<< std::endl;
#endif
	
	}
}

void encoder::finish() {
	// Output two byte that select the quarter that the current
	// code range contains
	++byte_to_follow;
	bit_plus_follow(low >= FirstQuarter);
	// Pad output byte to to 8 byte
	if (bitp != 0)
		byte <<= (8 - bitp); 
	flush_bits();
	flush();
	// Pad the output to CodeValueBits length
	// FIXME Amount of fluff after end of code
	for (int i = 0 ; i < (CodeValueBits >> 3) ; ++i) {
		out << (char)0;
		++outlen;
	}
}

// Output byte plus following opposite bits.
void encoder::bit_plus_follow(const bool bit) {
	// Output bit
	bit_write(bit);
	// Output byte_to_follow opposite bits. Sets byte_to_follow to zero.
	while (byte_to_follow > 0) {
		bit_write(!bit);
		--byte_to_follow;
	}
}

void encoder::flush_bits() {
#ifndef UNSAFE
	assert (bitp == 8);
#endif
	buf[p++] = byte;
	byte = bitp = 0;
}

const uint64 encoder::len() const {
	return outlen;
}

void encoder::flush() {
	if (p == 0)
		return;
	out.write(buf, p);
	outlen += p;
	p = 0;
}

void encoder::bit_write(const bool bit) {
	byte = ((byte << 1) | (bit ? 1 : 0));
	if (++bitp == 8)
		flush_bits();
	if (p == WriteBufSize)
		flush();
}

} // namespace

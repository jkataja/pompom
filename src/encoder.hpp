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

	// Length of output bitss
	const uint64 len() const;

	// Write pending bits
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
	uint8 bits;
	uint64 outlen;

	// High end of the current code region
	uint64 high;

	// Low end of the current code region
	uint64 low;

	// Number of bits to follow next
	uint64 bits_to_follow;

	inline void bit_plus_follow(const bool);
	inline void bit_write(const bool);
	inline void flush_bits();
	inline void flush();
};

encoder::encoder(std::ostream& proxy)
	: out(proxy), p(0), bitp(0), bits(0), outlen(0),
	  high(TopValue), low(0), bits_to_follow(0) 
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
	high = low + (range * dist[ R(c) ]) / dist[ R(EOS) ] - 1;
	low = low + (range * dist[ L(c) ]) / dist[ R(EOS) ];

#ifdef DEBUG
	std::cerr << "encode\t" << range 
		<< "\t< " << dist[ L(c) ] << " , " << dist[ R(c) ] << " > " 
		<< "\t < " << low << " , " << high << " > " 
		<< "\t " << c << " '" << (char)c << "'" << std::endl;
#endif

	// Loop to output bits
	while (true) {
		// Output 0 if in low half
		if (high < Half) {
			bit_plus_follow(false);
		}
		// Output 1 if in high half
		else if (low >= Half) {
			bit_plus_follow(true);
			// Subtract offset to top
			low -= Half;
			high -= Half;
		}
		// Output an opposite bit
		else if (low >= FirstQuarter && high < ThirdQuarter) {
			// later if in middle half
			++bits_to_follow;
			// Subtract offset to middle
			low -= FirstQuarter;
			high -= FirstQuarter;
		}
		// Otherwise exit loop
		else
			break;
		// Scale up code range
		low = (low << 1);
		high = ((high << 1) + 1);
	}
}

void encoder::finish() {
	// Output two bits that select the quarter that the current
	// code range contains
	++bits_to_follow;
	if (low < FirstQuarter)
		bit_plus_follow(false);
	else
		bit_plus_follow(true);
	// Pad output byte to to 8 bits
	while (++bitp != 8)
		bits <<= 1;
	flush_bits();
	flush();

	// FIXME should it close output?
}

// Output bits plus following opposite bits.
void encoder::bit_plus_follow(const bool bit) {
	// Output bit
	bit_write(bit);
	// Output bits_to_follow opposite bits. Set bits_to_follow to zero
	while (bits_to_follow > 0) {
		bit_write(!bit);
		bits_to_follow -= 1;
	}
}

void encoder::flush_bits() {
#ifndef UNSAFE
	assert (bitp == 8);
#endif
	buf[p++] = bits;
	bits = bitp = 0;
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
	bits = ((bits << 1) + (bit ? 1 : 0));
	if (++bitp == 8)
		flush_bits();
	if (p == WriteBufSize)
		flush();
}

} // namespace

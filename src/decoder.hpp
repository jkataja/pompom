/**
 * Arithmetic decoder.
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

class decoder {
public:
	// Encoder symbol using distribution
	inline const uint16 decode(const uint32[], const uint64[]);

	// End of data reached
	inline const bool eof();

	decoder(std::istream&);
	~decoder();
private:
	decoder(const decoder&);
	const decoder& operator=(const decoder&);

	bool eofreached;
	std::istream& in;

	// Low end of the current code region
	uint64 low;

	// High end of the current code region
	uint64 high;

	// Code word that is currently being decoded
	uint64 value;

	// Bit output
	inline const bool bit_read();
	uint16 bitp;
	uint8 byte;
};

const uint16 decoder::decode(const uint32 dist[], 
		const uint64 dist_check[]) 
{
	if (eof())
		return EOS;

	// Symbol to be decoded
	uint16 c = Escape;
	// Size of the current code region
	uint64 range = (uint64) (high - low) + 1;
	// Frequency for value in range
	uint32 v_range = dist[ R(EOS) ];
	uint32 freq = (uint32) ((((value - low) + 1) * v_range - 1) / range);

	// Then find symbol
	bool c_found = false;
	/*
	for (int p = 0 ; (!c_found && p < 4) ; ++p) {
		int off = (p << 6);
		uint64 it = 0xFFFFFFFFFFFFFFFFULL;
		uint64 nodes = (dist_check[p]);
		while (!c_found && ((nodes = ((nodes & it))) != 0)) {
			int b = __builtin_clzll(nodes); // leading zeros
			uint64 c_mask = (1ULL << (63-b));
			it ^= c_mask; // mark visited
			c = off + b;
			std::cerr << "freq:" << freq  << "|" << dist[R(c)]<< " b:" << b << " c:" << c << std::endl;
			if (dist[ R(c) ] > freq) {
				c_found = true;
				break;
			}
		}
	}
	*/
	for (c = 0; c <= Alpha ; ++c) {
		if ((dist_check[ (c >> 6) ] & (1 - (63-(c & 0x3F)))) 
				&& dist[ R(c) ] > freq) {
			c_found = true;
			break;
		}
	}
	// Escape and EOS not in node_check
	// Don't consume input after EOS
	if (!c_found && freq >= dist[ R(Escape) ])
		return EOS;

#ifndef UNSAFE
	assert((c != Escape) || (dist[L(Escape)] != dist[R(Escape)]));//XXX
#endif

	// Narrow the code region to that allotted to this symbol.
	high = low + (range * dist[ R(c) ]) / v_range - 1;
	low = low + (range * dist[ L(c) ]) / v_range;

#ifdef DEBUG
	std::cerr << "decode\t" << range 
		<< "\t< " << dist[ L(c) ] << " , " << dist[ R(c) ] << " > " 
		<< "\t < " << low << " , " << high << " > " ;
	if (c == 256)
		std::cerr << "\t " << c << " ESC" << std::endl;
	else if (c >= 0x20 && c <= '~')
		std::cerr << "\t " << c << " '" << (char)c << "'" << std::endl;
	else
		std::cerr << "\t " << c << std::endl << std::flush;
#endif

	// Consume bits
	while (true) {
		// Matching most significant bit, shift out
		if ((high & (1 << (CodeValueBits - 1))) 
				== (low & (1 << (CodeValueBits - 1)))) {
			// Nothing
		}
		// Underflow 
		else if ((low & FirstQuarter) && !(high & FirstQuarter)) {
			// Flip second most significant
			value ^= FirstQuarter;
			// Clear 2 upper bits
			low &= (FirstQuarter - 1);
			// Set second most significant
			high |= FirstQuarter;
		}
		// Otherwise exit loop.
		else
			break;

		// Scale up code range
		low = ((low << 1) & TopValue);
		high = (((high << 1) | 1) & TopValue);
		value = (((value << 1) | (bit_read() ? 1 : 0)) & TopValue);

#ifdef DEBUG
		std::cerr << "\t\t\t\t\t" 
			<< " < " << low << " , " << high << " > "
			<< std::endl;
#endif
	}

	return c;
}

const bool decoder::eof() {
	return (eofreached || in.eof());
}

const bool decoder::bit_read() {
	// Read next byte on byte boundary 
	if (bitp == 0) {
		byte = in.get();
		bitp = 8;
	}
	return ((byte & (1 << --bitp)) >= 1);
}

decoder::decoder(std::istream& proxy)
	: eofreached(false), in(proxy), low(0), high(TopValue), value(0),
	  bitp(0), byte(0)
{
	// Initial code range
	for (int i = 0 ; (i < CodeValueBits >> 3) ; ++i)
		value = (value << 8) | in.get();
}

decoder::~decoder() {
}

} // namespace

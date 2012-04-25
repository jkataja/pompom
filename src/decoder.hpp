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
	inline const uint16 decode(const uint32[]);

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

const uint16 decoder::decode(const uint32 dist[]) {
	if (eof())
		return EOS;

	// Symbol to be decoded
	uint16 c;
	// Size of the current code region
	uint64 range = (uint64) (high - low) + 1;
	// Frequency for value in range
	uint32 freq = (uint32) ((((value - low) + 1) 
		* dist[ R(EOS) ] - 1) / range);

	// Then find symbol
	for (c = 0; c <= EOS + 1 ; ++c)
		if (dist[ R(c) ] > freq)
			break;

	// Don't consume input after EOS
	if (c == EOS) {
		return c;
	}

	// Narrow the code region to that allotted to this symbol.
	high = low + (range * dist[ R(c) ]) / dist[ R(EOS) ] - 1;
	low = low + (range * dist[ L(c) ]) / dist[ R(EOS) ];

#ifdef DEBUG
	std::cerr << "decode\t" << range 
		<< "\t< " << dist[ L(c) ] << " , " << dist[ R(c) ] << " > " 
		<< "\t < " << low << " , " << high << " > " ;
	if (c == 256)
		std::cerr << "\t " << c << " ESC" << std::endl;
	else if (c >= 0x20 && c <= '~')
		std::cerr << "\t " << c << " '" << (char)c << "'" << std::endl;
	else
		std::cerr << "\t " << c << std::endl;
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

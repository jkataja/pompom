#include <iostream>
#include <cmath>
#include <stdexcept>

#include "encoder.hpp"
#include "pompom.hpp"

using namespace std;

namespace pompom {

void encoder::encode(const uint16 c, const uint32 dist[]) {

#ifndef UNSAFE
	if (c > EOS) {
		throw range_error("symbol not in code range");
	}
#endif

	buf[p++] = (c >> 8); buf[p++] = (c & 0xFF);
	if (p == WriteBufSize) {
		out.write(buf, p);
		p = 0;
	}

#ifdef DEBUG
	float h = log( (float)(dist[ R(c) ] - dist[ L(c) ]) / dist[ R(EOS) ] ) 
			/ log(2);
	h *= -1;

	if (c >= 0x20 && c <= 0x7e) // ' ' .. '~'
		cerr << "encode ('" << ((char)c) << "'";
	else if (c == Escape)
		cerr << "encode (ESC";
	else if (c == EOS)
		cerr << "encode (EOS";
	else
		cerr << "encode (" << c;

	hsum += h;
	cerr << ")\t" << (dist[ R(c) ] - dist[ L(c) ]) 
		<< "/" << (dist[ R(EOS) ])
		<< "\th=" << h << endl << flush;
#endif
}

const long encoder::len() {
	return (hsum/8)+1;
}

void encoder::finish() {
	// TODO Write pending 
	if (p > 0) {
		out.write(buf, p);
		p = 0;
	}
}

encoder::encoder(ostream& proxy) : out(proxy), p(0) {
}

encoder::~encoder() {
}

} // namespace

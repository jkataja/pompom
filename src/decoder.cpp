#include <iostream>

#include "decoder.hpp"
#include "pompom.hpp"

using namespace std;

namespace pompom {

const int decoder::decode(const uint32[]) {
	int c = 0;

	if (eof())
		return EOS;

	char b = 0;
	for (int i = 0 ; (i<2 && (in.get(b) >= 0)) ; ++i) {
		c = ((c << 8) | (b & 0xFF));
	}

#ifdef DEBUG
	if (c >= 0x20 && c <= 0x7e) // ' ' .. '~'
		cerr << "decode -> " << ((char)c) << endl << flush;
	else if (c == Escape)
		cerr << "decode -> Escape" << endl << flush;
	else if (c == EOS)
		cerr << "decode -> EOS" << endl << flush;
	else
		cerr << "decode -> " << c << endl << flush;
#endif

	return c;
}

const bool decoder::eof() {
	return (eofreached || in.eof());
}

decoder::decoder(istream& proxy) : eofreached(false), in(proxy) {
}

decoder::~decoder() {
}

} // namespace

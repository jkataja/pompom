#include <sstream>
#include <iomanip>
#include <memory>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/crc.hpp>

#include "pompom.hpp"
#include "model.hpp"
#include "decoder.hpp"
#include "encoder.hpp"

using std::string;
using std::istream;
using std::ostream;
using std::endl;
using std::flush;
using std::unique_ptr;
using std::fixed;
using std::setprecision;
using std::range_error;
using boost::crc_32_type;
using boost::str;
using boost::format;

namespace pompom {

long decompress(istream& in, ostream& out, ostream& err) {

	// Magic header: 0-terminated string
	char filemagic[ sizeof(Magia) ];
	in.getline(filemagic, sizeof(Magia), (char)0);
	if (strncmp(filemagic, Magia, sizeof(Magia)) != 0) {
		err << SELF << ": no magic" << endl << flush;
		return -1;
	}

	// Model order: 1 byte
	uint8 order = in.get();

	// Model memory limit: 2 bytes
	uint16 limit = ((in.get() << 8) | in.get());

	decoder dec(in);
	unique_ptr<model> m( model::instance(order, limit) );
	uint32 dist[ R(EOS) + 1 ];

	// Read data: terminated by EOS symbol
	crc_32_type crc;
	uint64 len = 0;
	uint16 c = 0;
	while (!dec.eof()) {
		// Seek character range
		for (int ord = m->Order ; ord >= -1 ; --ord) {
			m->dist(ord, dist);
			// Symbol c has frequency in context
			if ((c = dec.decode(dist)) != Escape)
				break;
		} 
#ifndef UNSAFE
		if (c == Escape) {
			throw range_error("seek character range leaked escape");
		}
#endif
		if (c == EOS) {
			break;
		}
	
		// Output
		out << (char)c;

		// Update model
		m->update(c);
		crc.process_byte(c);
		++len;
	}
	if (dec.eof()) {
		err << SELF << ": unexpected end of compressed data" << endl;
		return -1;
	}

	// CRC check: 4 bytes
	uint32 v = 0;
	char b = 0;
	for (int i = 0 ; (i<4 && (in.get(b) >= 0)) ; ++i) {
		v = ((v << 8) | (b & 0xFF));
	}
	if (v != crc.checksum()) {
		err << SELF << ": checksum does not match" << endl;
		return -1;
	}

	return len;
}

long compress(istream& in, ostream& out,
		ostream& err, const uint8 order, const uint16 limit) {

	unique_ptr<model> m( model::instance(order, limit) );
	uint32 dist[ R(EOS) + 1 ];

	// Out magic, order and memory limit
	out << Magia << (char)0x00;
	out << (char)(order & 0xFF);
	out << (char)(limit >> 8) << (char)(limit & 0xFF);

	// Use boost CRC even when hardware intrisics would be available.
	// Just to be sure encoder/decoder use same CRC algorithm.
	crc_32_type crc;
	
	// Write data: terminated by EOS symbol
	encoder enc(out);
	uint64 len = 0;
	char b;
	while (in.get(b)) {
		int c = (0xFF & b);
		// Seek character range
		for (int ord = m->Order ; ord >= -1 ; --ord) {
			m->dist(ord, dist);
			// Symbol c has frequency in context
			if (dist[ L(c) ] != dist[ R(c) ])
				break;
			// Output escape when symbol c has zero frequency
			enc.encode(Escape, dist); 
		} 
		
		// Output
#ifndef UNSAFE
		if (dist[ L(c) ] == dist[ R(c) ]) {
			throw range_error(
				str ( format("zero frequency for symbol %1%") % (int)c ) 
			);
		}
#endif
		enc.encode(c, dist);

		// Update model
		m->update(c);
		crc.process_byte(c);
		++len;
	}
	// Escape to -1 level, output EOS
	for (int ord = m->Order ; ord >= 0 ; --ord) {
		m->dist(ord, dist);
		enc.encode(Escape, dist); 
	} 
	m->dist(-1, dist);
#ifndef UNSAFE
	if (dist[ L(EOS) ] == dist[ R(EOS) ]) {
		throw range_error("zero frequency for EOS");
	}
#endif
	enc.encode(EOS, dist);

	// Write pending output 
	enc.finish();
	
	// Write checksum: 4 bytes
	uint32 v = crc.checksum();
	out << (char)(v >> 24) << (char)((v >> 16) & 0xFF) 
		<< (char)((v >> 8) & 0xFF) << (char)(v & 0xFF);

	uint64 outlen = enc.len();
	double bpc = ((outlen / (double)len) * 8.0); // XXX
	
	err << SELF << ": in " << len << " -> out " << outlen << " at " 
		<< fixed << setprecision(3) << bpc << " bpc" 
		<< endl << flush;
	
	return len;
}

} // namespace

#include <sstream>
#include <iomanip>
#include <memory>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <boost/crc.hpp>
#include <boost/format.hpp>

#include "pompom.hpp"
#include "model.hpp"
#include "decoder.hpp"
#include "encoder.hpp"

using std::string;
using std::istream;
using std::ostream;
using std::endl;
using std::flush;
using std::auto_ptr;
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

	// Model memory limit: 2 bytes
	unsigned short limit = ((in.get() << 8) | in.get());

	decoder dec(in);
	auto_ptr<model> m( model::instance(limit) );
	int dist[ R(EOS) + 1];
	//memset(dist, 0, sizeof(dist));

	// Read data: terminated by EOS symbol
	crc_32_type crc;
	long len = 0;
	int c = 0;
	while (!dec.eof()) {
		// Seek character range
		for (int ord = Order ; ord >= -1 ; --ord) {
			m->dist(ord, dist);
			// Symbol c has frequency in context
			if ((c = dec.decode(dist)) != Escape)
				break;
		} 
		if (c == Escape) {
			throw range_error("character range leaked escape from");
		}
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
	if (in.eof()) {
		err << SELF << ": unexpected end of compressed data" << endl;
		return -1;
	}

	// CRC check: 4 bytes
	unsigned int v = 0;
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
		ostream& err, const string& model_args) {

	// Model memory limit
	unsigned short limit = 0;
	if ((limit = atol(model_args.c_str())) == 0) {
		err << SELF << ": memory limit '" << model_args 
			<< "' is not an integer" << endl;
		return -1;
	}

	// Out magic and memory limit
	out << Magia << (char)0x00;
	out << (char)(limit >> 8) << (char)(limit & 0xFF);

	encoder enc(out);
	auto_ptr<model> m( model::instance(limit) );
	int dist[ R(EOS) + 1];
	//memset(dist, 0, sizeof(dist));

	// Write data: terminated by EOS symbol
	crc_32_type crc;
	long len = 0;
	char ch;
	while (in.get(ch)) {
		int c = (0xFF & ch);
		// Seek character range
		for (int ord = Order ; ord >= -1 ; --ord) {
			m->dist(ord, dist);
			// Symbol c has frequency in context
			if (dist[ L(c) ] != dist[ R(c) ])
				break;
			// Output escape when symbol c has zero frequency
			enc.encode(Escape, dist); 
		} 
		
		// Output
		if (dist[ L(c) ] == dist[ R(c) ]) {
			throw range_error(
				str ( format("zero frequency for symbol %1%") % (int)c ) 
			);
		}
		enc.encode(c, dist);

		// Update model
		m->update(c);
		crc.process_byte(c);
		++len;
	}
	// Escape to -1 level, output EOS
	for (int ord = Order ; ord >= 0 ; --ord) {
		m->dist(ord, dist);
		enc.encode(Escape, dist); 
	} 
	m->dist(-1, dist);
	if (dist[ L(EOS) ] == dist[ R(EOS) ]) {
		throw range_error("zero frequency for EOS");
	}
	enc.encode(EOS, dist);

	// Write pending output 
	enc.finish();
	
	// Write checksum: 4 bytes
	unsigned int v = crc.checksum();
	out << (char)(v >> 24) << (char)((v >> 16) & 0xFF) 
		<< (char)((v >> 8) & 0xFF) << (char)(v & 0xFF);

	long outlen = enc.len();
	double bpc = ((outlen / (double)len) * 8.0);
	
	err << SELF << ": in " << len << " -> out " << outlen << " at " 
		<< fixed << setprecision(2) << bpc << " bpc" 
		<< endl << flush;
	
	return len;
}

} // namespace

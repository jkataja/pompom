#include <iomanip>
#include <cstring>
#include <boost/format.hpp>
#include <boost/crc.hpp>

#include "pompom.hpp"
#include "model.hpp"
#include "decoder.hpp"
#include "encoder.hpp"

namespace pompom {

long decompress(std::istream& in, std::ostream& out, std::ostream& err) {

	// Magic header: 0-terminated std::string
	char filemagic[ sizeof(Magia) ];
	in.getline(filemagic, sizeof(Magia), (char)0);
	if (strncmp(filemagic, Magia, sizeof(Magia)) != 0) {
		err << SELF << ": no magic" << std::endl << std::flush;
		return -1;
	}

	// Model order: 1 byte
	uint8 order = in.get();

	// Model memory limit: 2 bytes
	uint16 limit = ((in.get() << 8) | in.get());

	// Model bootstrap buffer length: 1 byte
	uint8 bootsize = in.get();

	// Model local adaptation length: 1 byte
	uint8 adaptsize = in.get();

	decoder dec(in);
	std::unique_ptr<model> m( model::instance(order, limit, 
			(bootsize == 0), bootsize, (adaptsize > 0), adaptsize ) );

	uint32 dist[ R(EOS) + 1 ];

	// Exclusion mask for chars which appeared in a higher order
	uint64 x_mask[4];

	// Read data: terminated by EOS symbol
	boost::crc_32_type crc;
	uint64 len = 0;
	uint16 c = 0;
	while (!dec.eof()) {
		memset(x_mask, 0xFF, sizeof(long) * 4);
		// Seek character range
		for (int ord = m->order ; ord >= -1 ; --ord) {
			m->dist(ord, dist, x_mask);
			// Symbol c has frequency in context
			if ((c = dec.decode(dist)) != Escape)
				break;
		} 
#ifndef UNSAFE
		if (c == Escape) {
			throw std::range_error("seek character range leaked escape");
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
		err << SELF << ": unexpected end of compressed data" << std::endl;
		return -1;
	}

	// CRC check: 4 bytes at EOF
	// FIXME Instead of until EOF; fixed amount of fluff after end of code
	uint32 v = 0;
	int b;
	while ((b = in.get()) != -1)
		v = ((v << 8) | (b & 0xFF));
	if (v != crc.checksum()) {
		err << SELF << ": checksum does not match"
#ifdef VERBOSE
			<< ": " << std::hex 
			<< "out:" << crc.checksum() << " file:" << v << std::dec 
#endif
			<< std::endl;
		return -1;
	}

	return len;
}

long compress(std::istream& in, std::ostream& out, std::ostream& err, 
		const int order, const int limit, const long maxlen, 
		const bool reset, const int bootsize,
		const bool adapt, const int adaptsize ) 
{
	// Magic
	out << Magia << (char)0x00;

	// Model order: 1 byte
	out << (char)(order & 0xFF);

	// Model memory limit: 2 bytes
	out << (char)(limit >> 8) << (char)(limit & 0xFF);

	// Model bootstrap buffer length: 1 byte
	out << (char)((reset ? 0 : bootsize) & 0xFF);

	// Model local adaptation length: 1 byte
	out << (char)((adapt ? adaptsize : 0) & 0xFF);

	// Use boost CRC even when hardware intrisics would be available.
	// Just to be sure encoder/decoder use same CRC algorithm.
	boost::crc_32_type crc;
	
	std::unique_ptr<model> m( model::instance(order, limit, 
			reset, bootsize, adapt, adaptsize ) );

	uint32 dist[ R(EOS) + 1 ];

	// Exclusion mask for chars which appeared in a higher order
	uint64 x_mask[4];

	// Write data: terminated by EOS symbol
	encoder enc(out);
	uint64 len = 0;
	char b;
	while (in.get(b)) {
		int c = (0xFF & b);
		memset(x_mask, 0xFF, sizeof(long) * 4);
		// Seek character range
		for (int ord = m->order ; ord >= -1 ; --ord) {
			m->dist(ord, dist, x_mask);
			// Symbol c has frequency in context
			if (dist[ L(c) ] != dist[ R(c) ])
				break;
			// Output escape when symbol c has zero frequency
			enc.encode(Escape, dist); 
		} 
		
		// Output
#ifndef UNSAFE
		if (dist[ L(c) ] == dist[ R(c) ]) {
			throw std::range_error(
				boost::str ( boost::format("zero frequency for symbol %1%") % (int)c ) 
			);
		}
#endif
		enc.encode(c, dist);

		// Update model
		m->update(c);
		crc.process_byte(c);

		// Process only prefix amount of bytes
		if (++len == maxlen)
			break;
	}
	// Escape to -1 level, output EOS
	memset(x_mask, 0xFF, sizeof(long) * 4);
	for (int ord = m->order ; ord >= 0 ; --ord) {
		m->dist(ord, dist, x_mask);
		enc.encode(Escape, dist); 
	} 
	m->dist(-1, dist, x_mask);
#ifndef UNSAFE
	if (dist[ L(EOS) ] == dist[ R(EOS) ]) {
		throw std::range_error("zero frequency for EOS");
	}
#endif
	enc.encode(EOS, dist);

	// Write pending output 
	enc.finish();
	
	// Write checksum: 4 bytes
	uint32 v = crc.checksum();
	out << (char)(v >> 24) << (char)((v >> 16) & 0xFF) 
		<< (char)((v >> 8) & 0xFF) << (char)(v & 0xFF);

	// Length: magic + order + limit + bootsize + adapt + code + crc
	uint64 outlen = sizeof(Magia) + 1 + 2 + 1 + 1 + enc.len() + 4 ; 
	double bpc = ((outlen / (double)len) * 8.0);
	
	err << SELF << ": in " << len << " -> out " << outlen << " at " 
		<< std::fixed << std::setprecision(3) << bpc << " bpc" 
		<< std::endl << std::flush;
	
	return len;
}

} // namespace

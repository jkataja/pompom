/**
 * Dummy encoder for testing. 
 *
 * @author jkataja
 */

#pragma once

#include <iostream>

#include "pompomdefs.hpp"

namespace pompom {

class encoder {
public:
	// Encoder symbol using distribution
	void encode(const uint16, const uint32[]);

	// Length of output bytes
	const long len();

	// Write pending bits
	void finish();

	encoder(std::ostream&);
	~encoder();
private:
	encoder(const encoder&);
	const encoder& operator=(const encoder&);

	std::ostream& out;
	double hsum;
	
	const static uint32 WriteBufSize = 32768;
	char buf[WriteBufSize];
	uint32 p;
};

} // namespace

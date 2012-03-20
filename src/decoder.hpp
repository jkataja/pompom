/**
 * Dummy decoder for testing. 
 *
 * @author jkataja
 */

#pragma once

#include <iostream>

namespace pompom {

class decoder {
public:
	// Encoder symbol using distribution
	const int decode(const int[]);

	// End of data reached
	const bool eof();

	decoder(std::istream&);
	~decoder();
private:
	decoder(const decoder&);
	const decoder& operator=(const decoder&);
	bool eofreached;
	std::istream& in;
};

} // namespace

/**
 * Dummy encoder for testing. 
 *
 * @author jkataja
 */

#pragma once

#include <iostream>

namespace pompom {

	class encoder {
	public:
		// Encoder symbol using distribution
		void encode(const int, const int[]);

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
	};

}

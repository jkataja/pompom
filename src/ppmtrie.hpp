/**
 * Context frequency logging trie for PPM compression. Uses 8 bit alphabet.
 *
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <stdexcept>

#include "pompom.hpp"
#include "pompomdefs.hpp"

namespace pompom {

class ppmtrienode {
public:
	// add to frequency count
	void seen(); 
	// number of children set in context
	int escape_freq();
	ppmtrienode();
	~ppmtrienode();
private:
	ppmtrienode(const ppmtrienode& old);
	const ppmtrienode& operator=(const ppmtrienode& old);

	// base node index: 24b
	uint8 basepage;
	uint16 base;

	// bit vector of 32 bit ints: successor states
	uint32 child[8];
	
	// frequency of context: 16b
	uint16 freq;
	
};

inline
void ppmtrienode::seen() {
#ifndef HAPPY_GO_LUCKY
	if (freq ==  TopValue) {
		throw std::range_error("context frequency at max, rescale necessary");
	}
#endif 
	++freq;
}

inline
int ppmtrienode::escape_freq() {
	return __builtin_popcount(child[0]) 
		+ __builtin_popcount(child[1])
		+ __builtin_popcount(child[2]) 
		+ __builtin_popcount(child[3])
		+ __builtin_popcount(child[4])
		+ __builtin_popcount(child[5])
		+ __builtin_popcount(child[6])
		+ __builtin_popcount(child[7]);
}

class ppmtrie {
public:
	ppmtrie(const uint16);
	~ppmtrie();
private:
	ppmtrie();
	ppmtrie(const ppmtrie& old);
	const ppmtrie& operator=(const ppmtrie& old);
	
};

inline
ppmtrie::ppmtrie() {
}

} // namespace

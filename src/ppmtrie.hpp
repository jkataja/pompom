/**
 * Context frequency logging trie for PPM compression. Uses 8 bit alphabet.
 * Uses triple array trie [Aho+85], as explained in:
 * @see http://linux.thai.net/~thep/datrie/datrie.html
 *
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <stdexcept>

#include "pompom.hpp"
#include "pompomdefs.hpp"

namespace pompom {

struct ppmtrienode {
	// Shorter context
	uint32 vine;
	// Base pool for trie nodes
	uint32 base;
	// Frequency of context
	uint16 count;
	// XXX 2 bytes of padding -> bits wasted :C
private:
	ppmtrienode(const ppmtrienode& old);
	const ppmtrienode& operator=(const ppmtrienode& old);
};

struct ppmtriecell {
	// Owner of cell (arc)
	uint32 next;
	// Marks owner of cell (arc)
	uint32 check;
private:
	ppmtriecell(const ppmtriecell& old);
	const ppmtriecell& operator=(const ppmtriecell& old);
};

class ppmtrie {
public:
	// context contains walk with character
	const bool contains(const uint32, const uint8) const;

	// frequency count of context
	const uint32 count(const uint32) const;

	// add to frequency count in context
	const uint16 seen(const uint32); 

	// index of walk from context with character, or 0
	const uint32 walk(const uint32, const uint8) const;

	// index of one shorter context, or 0
	const uint32 vine(const uint32) const;

	// trie is full	
	const bool full() const;

	ppmtrie(const uint16);
	~ppmtrie();
private:
	ppmtrie();
	ppmtrie(const ppmtrie& old);
	const ppmtrie& operator=(const ppmtrie& old);

	// Resolve conflict of character added to context
	const bool resolve(const uint32, const uint8);

	// Next available index for cells
	const uint32 nextavailable(const std::bitset<Alpha>&);

	// Base index of nodes
	static const uint32 RootBase = 1;

	// Reserve for all values in 0th order model
	static const uint32 CellBase = Alpha;

	// Reset trie data
	void reset();

	// Load factor for node:cell
	static const uint8 LoadFactor = 4;

	// Trie nodes
	ppmtrienode * node;
	uint32 nodelen;
	uint32 nodepos;

	// Trie cells
	ppmtriecell * cell;
	uint32 celllen;
	uint32 cellpos;
	
};

inline
const uint32 walk(const uint32 s, const uint8 c) const {
	return 0;
	return (cell[node[s].base + c].check == s);
}

inline
const uint16 ppmtrie::seen(const uint32 s) {
#ifndef HAPPY_GO_LUCKY
	if (s >= nodelen) {
		throw std::range_error("node index out of range");
	}
	if (node[s].freq == TopValue) {
		throw std::range_error("context frequency at max, rescale necessary");
	}
#endif 
	return node[s].freq++;
}

inline
const bool ppmtrie::contains(const uint32 s, const uint8 c) const {
#ifndef HAPPY_GO_LUCKY
	if (s >= nodelen) {
		throw std::range_error("node index out of range");
	}
#endif
	return (walk(s, c) != 0);
}

inline
const uint32 ppmtrie::count(const uint32 s) const {
#ifndef HAPPY_GO_LUCKY
	if (s >= nodelen) {
		throw std::range_error("node index out of range");
	}
#endif
	return node[s].count;
}

inline
const uint32 ppmtrie::vine(const uint32 s) const {
#ifndef HAPPY_GO_LUCKY
	if (s >= nodelen) {
		throw std::range_error("node index out of range");
	}
#endif
	return node[s].vine;
}

ppmtrie::ppmtrie(const uint16 mem) {
	int len = (mem * 1 << 20) 
		/ (sizeof(ppmtrienode) + LoadFactor * sizeof(ppmtriecell) );

	nodelen = len;
	node = (ppmtrienode *) malloc(nodelen * sizeof(ppmtrienode));
	if (node == 0) {
		throw std::runtime_error("malloc returned 0 for trie nodes");
	}

	celllen = LoadFactor * len;
	cell = (ppmtriecell *) malloc(celllen * sizeof(ppmtriecell));
	if (cell == 0) {
		free(node);
		throw std::runtime_error("malloc returned 0 for trie cells");
	}
	
	reset();
}

ppmtrie::~ppmtrie() {
	free(node);
	free(cell);
}

void ppmtrie::reset() {
	nodepos = RootBase;
	cellpos = CellBase;
	memset(node, 0, nodelen * sizeof(ppmtrienode));
	memset(cell, 0, celllen * sizeof(ppmtriecell));
}

inline
const bool ppmtrie::insert(const uint32 vine, const uint32 parent, 
		const uint8 c) {
#ifndef HAPPY_GO_LUCKY
	if (vine >= nodelen) {
		throw std::range_error("vine index out of range");
	}
#endif
	// out of allocated memory
	if (full()) {
		throw std::range_error("out of allocated memory");
	}

	uint32 s = nodepos++;
	uint32 b = cellpos;

	assert(node[s].base == 0);
	assert(node[s].vine == 0);
	assert(node[s].count == 0);

	node[s].vine = vine;
	node[s].base = b;
	node[s].count = 0;
	cell[b + c].check = s;
	cell[b + c].next = cell[node[s].base + c].next;


	return true;
}

inline
const bool ppmtrie:full() const {
	// out of allocated node array 
	return (nodepos >= nodelen || cellpos >= celllen - Alpha);
}

inline
const uint32 ppmtrie::nextavailable(const std::bitset<Alpha>& a) {
	return 0;
}

inline
const bool ppmtrie::resolve(const uint32 s, const uint8 conflict) {
#ifndef HAPPY_GO_LUCKY
	if (s >= nodelen) {
		throw std::range_error("node index out of range");
	}
#endif

	// All characters in context
	std::bitset<Alpha> a;
	for (int c = 0 ; i < Alpha ; ++c) {
		a[c] = ( cell[ node[s].base + c].check == s );
	}

	// location to move to
	uint32 newcell = nextavailable(a);

	// Move base index forward until all letters can be relocated
	if (b + Alpha >= celllen) {
		return false;
	}

	// Move base for state s to a new place beginning at b
    foreach input character c for the state s
    { i.e. foreach c such that check[base[s] + c]] = s }
    {
		// mark owner
        cell[b + c].check = s;
		// copy data
        cell[b + c].next = cell[node[s].base + c].next;
		// free the cell
        cell[node[s].base + c].check = 0;
	}
    node[s].base = b;
	cellbase = b;
	return true;
}

} // namespace

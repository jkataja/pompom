/**
 * Prediction by Partial Matching model. Uses count of symbols in context
 * for escape frequency (symbols in context is the frequency of escape).
 * Update adds to symbol counts in only the contexts used in
 * compression and not in lower order contexts ("update exclusion").
 *
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <tr1/unordered_map>

#include "pompom.hpp"

namespace pompom {

class model {
public:
	// Returns new instance, checking model_args
	static model * instance(const uint8, const uint16);
	
	// Give running totals of the symbols in context
	void dist(const int16, uint32[]);

	// Clean polluted entries
	// @see http://research.microsoft.com/en-us/um/people/darkok/papers/DCC-ppm-cleaning.pdf 
	void clean();

	// Rescale when largest frequency has met limit
	void rescale();

	// Increase symbol counts
	void update(const uint16);

	// Prediction order
	const uint8 Order;

	~model();
private:
	model(const uint8, const uint16);
	model();
	model(const model& old);
	const model& operator=(const model& old);

	// Data context
	std::deque<int> context;

	// Visited nodes
	std::vector<int> visit;

	// Context+Following letters (4 characters) -> frequency
	std::tr1::unordered_map<uint32,uint32> nodecnt;
};

inline
void model::dist(const int16 ord, uint32 dist[]) {
	// Count of symbols which have frequency, used as escape frequency
	uint32 syms = 0; 
	// Cumulative frequency of symbols
	uint32 run = 0; 
	// Store previous value since R(c) == L(c+1)
	uint32 last = 0; 

	// -1th order
	// Give 1 frequency to symbols which have no frequency in 0th order
	if (ord == -1) { 
		for (int c = 0 ; c <= EOS ; ++c) {
			/*
			if (dist[ R(c) ] == last)
				++run; 
			*/
			run += (dist[ R(c) ] == last);
			last = dist[ R(c) ];
			dist[ R(c) ] = run;
		}
		return;
	}

	// Zero cumulative sums, no f for any symbol
	if (ord == Order)
		memset(dist, 0, sizeof(int) * (R(EOS) + 1));

	// Just escapes before we have any context
	if ((int)context.size() < ord) {
		dist[ R(Escape) ] = dist[ R(EOS) ] = 1;
		return;
	}

	// seek existing context (maximum order of 3)
	uint32 t = 0;
	for (int i = ord ; i > 0 ; --i) {
		int c = context[i];
		t = (t << 8) | c;
	}

	// TODO trie: have no context - add 0 counts

	// seek successor states from node (following letters)
	for (int c = 0 ; c <= Alpha ; ++c) {
		// Only add if symbol had 0 frequency in higher order
		if (dist[ R(c) ] == last) {
			// Frequency of following context
			int freq = nodecnt[(t << 8) | c];
			// Update cumulative frequency
			run += freq;
			// Count of symbols in context
			syms += (freq > 0);
		}
		
		last = dist[ R(c) ];
		dist[ R(c) ] = run;
	}
	// Symbols in context, zero frequency for EOS
	dist[ R(EOS) ] = dist[ R(Escape) ] = run + (syms > 0 ? syms : 1); 

	visit.push_back(t);
}

} // namespace

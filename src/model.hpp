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

#include "pompomdefs.hpp"

namespace pompom {

class model {
public:
	// Returns new instance, checking model_args
	static model * instance(const uint16);

	// Give running totals of the symbols in context
	void dist(const int16, uint32[]);

	// Clean polluted entries
	// @see http://research.microsoft.com/en-us/um/people/darkok/papers/DCC-ppm-cleaning.pdf 
	void clean();

	// Rescale when largest frequency has met limit
	void rescale();

	// Increase symbol counts
	void update(const uint16);

	~model();
private:
	model(const uint16);
	model();
	model(const model& old);
	const model& operator=(const model& old);

	// Data context
	std::deque<int> context;

	// Visited nodes
	std::vector<int> visit;

	std::tr1::unordered_map<uint32,uint32> nodecnt;
};

} // namespace

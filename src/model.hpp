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
#include <boost/format.hpp>

#include "pompom.hpp"
#include "cuckoo.hpp"

using boost::str;
using boost::format;

namespace pompom {

class model {
public:
	// Returns new instance after checking model args
	static model * instance(const uint8, const uint16);
	
	// Give running totals of the symbols in context
	void dist(const int16, uint32 *);

	// Rescale when largest frequency has met limit
	void rescale();

	// Increase symbol counts
	void update(const uint16);

	// Prediction order
	const uint8 Order;

	// Memory limit in MiB
	const uint16 Limit;

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

	// Length+Context (0-7 characters; uint64) -> Frequency (uint16)
	cuckoo * contextfreq;
};

inline
void model::dist(const int16 ord, uint32 * dist) {
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

	// Start from existing context
	uint64 t = (ord + 1) << 8;
	for (int i = ord ; i > 0 ; --i) {
		int c = context[i];
		t = (t << 8L) | c;
	}

	// seek successor states from node (following letters)
	for (int c = 0 ; c <= Alpha ; ++c) {
		// Only add if symbol had 0 frequency in higher order
		if (dist[ R(c) ] == last) {
			// Frequency of following context
			int freq = contextfreq->count((t << 8L) | c);
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

inline
model * model::instance(const uint8 order, const uint16 limit) {
	if (order < OrderMin || order > OrderMax) {
		std::string err = str( format("accepted order is %1%-%2%") 
				% (int)OrderMin % (int)OrderMax );
		throw std::range_error(err);
	}
	if (limit < LimitMin || limit > LimitMax) {
		std::string err = str( format("accepted limit is %1%-%2% MiB") 
				% LimitMin % LimitMax );
		throw std::range_error(err);
	}
	return new model(order, limit);
}

inline
model::model(const uint8 order, const uint16 limit) 
	: Order(order), Limit(limit), contextfreq(0)
{
	visit.reserve(Order);
	contextfreq = new cuckoo(limit);
}

inline
model::~model() {
	delete contextfreq;
}

inline
void model::update(const uint16 c) { 
#ifndef HAPPY_GO_LUCKY
	if (c > Alpha) {
		throw std::range_error("update character out of range");
	}
#endif
	// Check if maximum frequency would be met, rescale if necessary
	bool outscale = false;
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 t = ((*it) << 8L) | c;
		outscale = (outscale || contextfreq->count(t) >= TopValue - 1);
	}
	if (outscale)
		rescale();

	// Update frequency of c from visited nodes
	// Don't update lower order contexts ("update exclusion")
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 t = ((*it) << 8L) | c;
		contextfreq->seen(t);
	}
	visit.clear();

	// Update text context
	if (context.size() == Order)
		context.pop_back();
	context.push_front(c);

}

inline
void model::rescale() {
	// Rescale all entries
	contextfreq->rescale();
}

} // namespace

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
	inline void dist(const int16, uint32 *);

	// Rescale when largest frequency has met limit
	void rescale();

	// Increase symbol counts
	inline void update(const uint16);

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
	std::vector<uint64> visit;

	// Length+Context (0-7 characters; uint64) -> Frequency (uint16)
	cuckoo * contextfreq;

	// Buffer length used to bootstrap model when hash is reset
	static const uint32 History = 2048;

	// Bootstrap context frequencies using recent text
	void bootstrap();
};

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

	// Existing context in 64b int
	uint64 parent = 0;
	for (int i = ord - 1 ; i >= 0 ; --i) 
		parent |= (0xFFULL & context[i]) << (i << 3); // context chars

	// First bit always set
	// Length (+1 for following): 2 bytes
	// Context char: 6 bytes
	// Following char: 1 byte
	uint64 keybase = ((0x81ULL + ord) << 56) | (parent << 8); 

	// Length of context
	parent |= ((0x80ULL + ord) << 56); 

	// Following letters in parent context
	const uint64 * vec = contextfreq->get_follower_vec(parent);

	// No symbols in context, assign 1/1 to escape
	if (vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0) {
		memset(dist, 0, sizeof(int) * (R(EOS) + 1));
		dist[ R(EOS) ] = dist[ R(Escape) ] = 1;
		visit.push_back(keybase);
		return;
	}

	// Add counts for successor chars from context
	int p = 0;
	uint64 cmask = (1ULL << 63);
	uint64 cfollow = vec[p++];
	
	for (int c = 0 ; c <= Alpha ; ++c) {
#ifdef DEBUG
		assert (contextfreq->has_follower(parent,c) 
				== ((cmask & cfollow) != 0) > 0));
		assert (contextfreq->has_follower(parent,c) 
				== (contextfreq->count(keybase | c) > 0));
#endif

		// Only add if symbol had 0 frequency in higher order
		if (dist[ R(c) ] == last && (cmask & cfollow) > 0) {
			// Frequency of following context
			int freq = contextfreq->count(keybase | c);
			// Update cumulative frequency
			run += freq;
			// Count of symbols in context
			syms += (freq > 0);
		}
	
		last = dist[ R(c) ];
		dist[ R(c) ] = run;

		// Following char bit mask
		cmask >>= 1;
		if (cmask == 0) {
			cmask = (1ULL << 63);
			cfollow = vec[p++];
		}
	}
	// Escape frequency is symbols in context
	// Zero frequency for EOS
	dist[ R(EOS) ] = dist[ R(Escape) ] = run + (syms > 0 ? syms : 1); 

	visit.push_back(keybase);
}

model * model::instance(const uint8 order, const uint16 limit) {
	if (order < OrderMin || order > OrderMax) {
		std::string err = str( format("accepted range for order is %1%-%2%") 
				% (int)OrderMin % (int)OrderMax );
		throw std::range_error(err);
	}
	if (limit < LimitMin || limit > LimitMax) {
		std::string err = str( format("accepted range for memory limit is %1%-%2% (in MiB)") 
				% LimitMin % LimitMax );
		throw std::range_error(err);
	}
	return new model(order, limit);
}

model::model(const uint8 order, const uint16 limit) 
	: Order(order), Limit(limit), contextfreq(0)
{
	visit.reserve(Order);
	contextfreq = new cuckoo(limit);
}

model::~model() {
	delete contextfreq;
}

void model::update(const uint16 c) { 
#ifndef UNSAFE
	if (c > Alpha) {
		throw std::range_error("update character out of range");
	}
#endif
	// Check if maximum frequency would be met, rescale if necessary
	bool outscale = false;
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 t = (*it) | c;
		outscale = (outscale || contextfreq->count(t) >= TopValue - 1);
	}
	if (outscale)
		rescale();

	// Update frequency of c from visited nodes
	// Don't update lower order contexts ("update exclusion")
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 key = (*it) | c;
		contextfreq->seen(key);
	}
	visit.clear();

	// Instead of rehashing, clear context data when preset size is full
	if (contextfreq->full()) {
		contextfreq->reset();

#ifdef BOOTSTRAP
		// Bootstrap based on most recent text
		if (context.size() == History)
			bootstrap();
#endif
	}

	// Update text context
	if (context.size() == History)
		context.pop_back();
	context.push_front(c);

}

void model::bootstrap() {
#ifdef VERBOSE
	std::cerr << "bootstrap" << std::endl;
#endif

#ifndef UNSAFE
	assert (context.size() == History);
#endif

	// Key length markers (first byte)
	uint64 len[Order];
	for (int ord = 0 ; ord <= Order ; ++ord) {
			len[ord] = ((0x81ULL + ord) << 56);
	}

	// Key mask for characters (max 7 bytes)
	uint64 mask[Order];
	uint64 lastmask = 0;
	for (int ord = 0 ; ord <= Order ; ++ord) {
		lastmask = mask[ord] = (lastmask << 8 | 0xFF);
	}

	// Fill first (circular buffer)
	uint64 text = 0;
	for (int i = History - 8  ; i < History ; --i) {
		uint8 c = context[i];
		text = (text << 8 | c);
	}

	// History buffer
	for (int i = History - 1 ; i >= 0 ; --i) {
		uint8 c = context[i];
		text = ((text << 8) | c);

		// Mark context lengths 0..Order as visited
		for (int ord = 0 ; ord <= Order ; ++ord) {
			uint64 key = (len[ord] | (mask[ord] & text));
			contextfreq->seen(key);
		}
	}
}

void model::rescale() {
	// Rescale all entries
	contextfreq->rescale();
}

} // namespace

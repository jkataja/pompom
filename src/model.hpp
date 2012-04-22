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
	static model * instance(const int, const int, const bool, const int);
	
	// Give running totals of the symbols in context
	inline void dist(const int16, uint32 *, uint64 *);

	// Rescale when largest frequency has met limit
	void rescale();

	// Increase symbol counts
	inline void update(const uint16);

	// Prediction order
	const uint8 order;

	// Memory limit in MiB
	const uint16 limit;

	~model();
private:
	model(const uint8, const uint16, const bool, const uint8);
	model();
	model(const model& old);
	const model& operator=(const model& old);

	// Data context
	std::deque<int> context;

	// Visited nodes
	std::vector<uint64> visit;

	// Length+Context (0-7 characters; uint64) -> Frequency (uint16)
	cuckoo * contextfreq;

	// Call bootstrap on reset
	bool do_bootstrap;

	// Buffer length, used in text context and model bootstrap 
	const uint32 history;

	// Bootstrap context frequencies using recent text
	void bootstrap();
};

void model::dist(const int16 ord, uint32 * dist, uint64 * x_mask) {
	// Count of symbols which have frequency, used as escape frequency
	uint32 syms = 0; 
	// Cumulative frequency of symbols
	uint32 run = 0; 

	int p = 0;
	uint64 c_mask = (1ULL << 63);

	// -1th order
	// Give 1 frequency to symbols which have no frequency in higher order
	if (ord == -1) { 
		for (int c = 0 ; c <= EOS ; ++c) {
			run += ((c_mask & x_mask[p]) > 0);
			dist[ R(c) ] = run;
			c_mask >>= 1;
			if (c_mask == 0) {
				c_mask = (1ULL << 63);
				++p;
			}
		}
		return;
	}

	// Zero cumulative sums, no f for any symbol
	if (ord == order)
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
	const uint64 * follow_vec = contextfreq->get_follower_vec(parent);

	// No symbols in context, assign 1/1 to escape
	if (follow_vec[0] == 0 && follow_vec[1] == 0 
			&& follow_vec[2] == 0 && follow_vec[3] == 0) {
		memset(dist, 0, sizeof(int) * (R(EOS) + 1));
		dist[ R(EOS) ] = dist[ R(Escape) ] = 1;
		visit.push_back(keybase);
		return;
	}

	// Add counts for successor chars from context
	for (int c = 0 ; c <= Alpha ; ++c) {
		// Only add if symbol had 0 frequency in higher order
		if (((x_mask[p] & follow_vec[p]) & c_mask) > 0) {
			// Frequency of following context
			int freq = contextfreq->count(keybase | c);
			// Update cumulative frequency
			run += freq;
			// Count of symbols in context
			syms += (freq > 0);
			// Mark visited
			if (freq > 0)
				x_mask[p] ^= c_mask;
		}
	
		dist[ R(c) ] = run;

		// Following char bit mask
		c_mask >>= 1;
		if (c_mask == 0) {
			c_mask = (1ULL << 63);
			++p;
		}
	}
	// Escape frequency is symbols in context
	// Zero frequency for EOS
	dist[ R(EOS) ] = dist[ R(Escape) ] = run + (syms > 0 ? syms : 1); 

	visit.push_back(keybase);
}

model * model::instance(const int ord, const int lim, 
		const bool reset, const int bootsiz) 
{
	if (bootsiz < BootMin || bootsiz > BootMax) {
		std::string err = str( format("accepted range for bootstrap buffer is %1%-%2%") 
				% (int)BootMin % (int)BootMax );
		throw std::range_error(err);
	}
	if (ord < OrderMin || ord > OrderMax) {
		std::string err = str( format("accepted range for ord is %1%-%2%") 
				% (int)OrderMin % (int)OrderMax );
		throw std::range_error(err);
	}
	if (lim < LimitMin || lim > LimitMax) {
		std::string err = str( format("accepted range for memory lim is %1%-%2% (in MiB)") 
				% LimitMin % LimitMax );
		throw std::range_error(err);
	}
	return new model(ord, lim, reset, bootsiz);
}

model::model(const uint8 ord, const uint16 lim, const bool reset,
		const uint8 bootsiz) 
	: order(ord), limit(lim), contextfreq(0), do_bootstrap(!reset),
	  history(do_bootstrap ? (bootsiz << 10) : ord)
{
	visit.reserve(order);
	contextfreq = new cuckoo(lim);
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
		outscale = (outscale || contextfreq->count(t) >= MaxFrequency - 1);
	}
	if (outscale)
		rescale();

	// Update frequency of c from visited nodes
	// Don't update lower order contexts ("update exclusion")
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 key = ((*it) | c);
		contextfreq->seen(key);
	}
	visit.clear();

	// Instead of rehashing, clear context data when preset size is full
	if (contextfreq->full()) {
		contextfreq->reset();

		// Bootstrap based on most recent text
		if (do_bootstrap && context.size() == history)
			bootstrap();
	}

	// Update text context
	if (context.size() == history)
		context.pop_back();
	context.push_front(c);

}

void model::bootstrap() {
#ifdef VERBOSE
	std::cerr << "bootstrap" << std::endl;
#endif

#ifndef UNSAFE
	assert (context.size() == history);
#endif

	// Circular buffer
	uint64 tailtext = 0;
	for (int i = order ; i >= 0 ; --i) {
		uint8 c = context[i];
		tailtext = ((tailtext << 8) | c);
	}
	
	// Key mask for characters (max 7 bytes)
	uint64 mask = 0xFF;
	for (int ord = 0 ; ord <= order ; ++ord) {

		uint64 text = tailtext;
		
		// Key length marker
		uint64 len = ((0x81ULL + ord) << 56);

		// History buffer
		for (int i = history - 1 ; i >= 0 ; --i) {
			uint8 c = context[i];
			text = ((text << 8) | c);
	
			// Mark context as visited
			// Insertion fails if history if too large to fit in memory
			// Disable bootstrap
			// TODO ex. cut history size in half, until minimal cap
			uint64 key = (len | (mask & text));
			if (!contextfreq->seen(key)) {
				contextfreq->reset();
				do_bootstrap = false;
#ifdef VERBOSE
				std::cerr << "history is too large to fit in memory, bootstrap disabled" << std::endl;
#endif
				return;
			}
		}

		mask = ((mask << 8) | 0xFF);
	}

}

void model::rescale() {
	// Rescale all entries
	contextfreq->rescale();
}

} // namespace

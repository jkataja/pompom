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

namespace pompom {

class model {
public:
	// Returns new instance after checking model args
	static model * instance(const int, const int, const bool, const int, const bool, const int);
	
	// Give running totals of the symbols in context
	inline void dist(const int16, uint32 *, uint64 *, uint64 *);

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
	model(const uint8, const uint16, const uint8, const uint8);
	model();
	model(const model& old);
	const model& operator=(const model& old);

	// Options range check
	static void opt_check(const char *, const int, const int, const int);

	// Data context
	std::deque<int> context;

	// Visited nodes
	std::vector<uint64> visit;

	// Length+Context (0-7 characters; uint64) -> Frequency (uint16)
	cuckoo * contextfreq;

	// Call bootstrap on reset
	bool lets_bootstrap;

	// Faster local adaptation with escape frequency count
	bool lets_esc_rescale;

	// Local adaptability threshold value for escaped frequency
	const uint64 adaptcount;

	// Buffer length, used in text context and model bootstrap 
	const uint32 history;

	// Bootstrap context frequencies using recent text
	void bootstrap();

	// Maximum cumulative frequency met
	bool outscale;

	// Cumulative frequency from last run
	uint64 last_run;

	// Cumulative frequency from last iteration of dist
	uint64 lastest_run;

	// Sum of escaped cumulative frequency
	uint64 sum_esc;
};

void model::dist(const int16 ord, uint32 * dist, uint64 * x_mask,
		uint64 * dist_check) 
{
	// Count of symbols which have frequency, used as escape frequency
	uint32 syms = 0; 
	// Cumulative frequency of symbols
	uint32 run = 0; 
	// 256 bits: symbols set in dist
	memset(dist_check, 0, sizeof(uint64) * 4);

	// -1th order
	// Give 1 frequency to symbols which have no frequency in higher order
	if (ord == -1) { 
		for (int c = 0 ; c <= Alpha ; ++c) {
			int p = (c >> 6);
			int b = (c & 0x3F);
			uint64 c_mask = (1ULL << (63-b));
			int freq = ((c_mask & x_mask[ p ]) > 0);
			dist[ L(c) ] = run;
			dist[ R(c) ] = (run += freq);
			dist_check[ p ] ^= c_mask;
		}
		dist[ L(EOS) ] = run;
		dist[ R(EOS) ] = ++run;

		return;
	}

	// Zero cumulative sums and exclusion mask
	if (ord == order) {
		memset(dist, 0, sizeof(int) * (R(EOS) + 1));
		memset(x_mask, 0xFF, sizeof(uint64) * 4);
	}
		

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

	// Add counts for successor chars from context
	for (int p = 0 ; p < 4 ; ++p) {
		int c_off = (p << 6);
		uint64 it = 0xFFFFFFFFFFFFFFFFULL;
		uint64 nodes = (x_mask[p] & follow_vec[p]);
		while ((nodes = ((nodes & it))) != 0) {
			int b = __builtin_clzll(nodes); // leading zeros
			int c = (c_off + b);
			uint64 c_mask = (1ULL << (63-b));
			it ^= c_mask; // mark visited
			// Frequency of following context
			int freq = contextfreq->count(keybase | c);
			// freq may be zero after shift-right at rescale()
			if (freq == 0)
				continue;
			// Update cumulative frequency
			dist[ L(c) ] = run;
			dist[ R(c) ] = (run += ((freq << 1) - 1));
			// Count of symbols in context
			++syms;
			// Mark visited in exclusion mask
			x_mask[p] ^= c_mask;
			// Mark appearing in dist
			dist_check[p] |= c_mask;
		}
	
	}
	// Escape frequency is symbols in context
	// Zero frequency for EOS
	dist[ L(Escape) ] = run;
	dist[ R(Escape) ] = dist[ R(EOS) ] = run + (syms > 0 ? syms : 1); 

	// Rescale forced by on encoder numerical limit
	outscale = (outscale || (dist[ R(Escape) ] > CoderRescale));

	last_run += run;
	lastest_run = run;

	visit.push_back(keybase);
}

void model::opt_check(const char * desc, const int val, 
		const int min, const int max) 
{
	if (val < min || val > max) {
		std::string err = boost::str( 
				boost::format("accepted range for %1% is [%2%,%3%]") 
						% desc % min % max );
		throw std::range_error(err);
	}
}

model * model::instance(const int ord, const int lim, 
		const bool reset, const int bootsize,
		const bool adapt, const int adaptsize) 
{
	opt_check("order", ord, OrderMin, OrderMax);
	opt_check("limit", lim, LimitMin, LimitMax);
	if (!reset)
		opt_check("bootstrap buffer", bootsize, BootMin, BootMax);
	if (adapt)
		opt_check("adapt", adaptsize, AdaptMin, AdaptMax);
	return new model(ord, lim, (reset ? 0 : bootsize), 
			(adapt ? adaptsize : 0));
}

model::model(const uint8 ord, const uint16 lim, const uint8 bootsize, 
		const uint8 adaptsize) 
	: order(ord), 
	  limit(lim), 
	  contextfreq(0), 
	  lets_bootstrap(bootsize > 0),
	  lets_esc_rescale(adaptsize > 0), 
	  adaptcount((1 << adaptsize) - 1), 
	  history(lets_bootstrap ? (bootsize << 10) : ord), 
	  outscale(false), 
	  last_run(0), 
	  lastest_run(0), 
	  sum_esc(0)
{
#ifdef VERBOSE
	std::cerr << "model order:" << (int)order << " limit:" << (int)limit 
		<< " bootstrap:" << lets_bootstrap << " bootsize:" << (int)bootsize
		<< " adapt:" << lets_esc_rescale << " adaptsize:" << (int)adaptsize 
		<< std::endl;
#endif
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

	// Rescale when past escaped frequency count threshold
	if (lets_esc_rescale) {
		sum_esc += (last_run - lastest_run);
		if (!outscale && (sum_esc >= adaptcount)) { 
#ifdef VERBOSE
			std::cerr << "escape frequency rescale sum_esc:" << sum_esc 
				<< " adaptcount:" << adaptcount << std::endl;
#endif
			sum_esc = 0;
			outscale = true;
		}
		last_run = lastest_run = 0;
	}

	// Check if maximum frequency would be met
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 key = ((*it) | c);
		outscale = (outscale || contextfreq->count(key) >= MaxFrequency);
	}
	// Rescale before updates
	if (outscale) {
		rescale();
		sum_esc = 0;
		outscale = false;
	}

	// Update frequency of c from visited nodes
	// Don't update lower order contexts ("update exclusion")
	for (auto it = visit.begin() ; it != visit.end() ; it++ ) {
		uint64 key = ((*it) | c);
		contextfreq->seen(key);
	}
	visit.clear();

	// Instead of rehashing, clear context data when preset size is full
	if (contextfreq->full()) {
		sum_esc = 0;

		contextfreq->reset();

		// Bootstrap based on most recent text
		if (lets_bootstrap && context.size() == history)
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
				lets_bootstrap = false;
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

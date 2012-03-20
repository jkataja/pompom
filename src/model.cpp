#include <stdexcept>
#include <cstring>
#include <vector>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include "pompom.hpp"
#include "model.hpp"

using std::range_error;
using std::string;
using std::vector;
using boost::str;
using boost::format;

namespace pompom {

model * model::instance(const int limit) {
	if (limit < 8 || limit > 2048) {
		string err = str( format("accepted limit is %1%-%2% MiB") 
				% LimitMin % LimitMax );
		throw range_error(err);
	}
	return new model(limit);
}

model::model(const int limit) {
	visit.reserve(Order);

	// initialize trie
}

model::~model() {
	// TODO delete trie
}

void model::dist(const int ord, int dist[]) {
	// Count of symbols seen for escape frequency
	int syms = 0; 
	// Cumulative sum
	int run = 0; 
	// Store previous value since R(c) == L(c+1)
	int last = 0; 

	// -1th order
	// Give 1 frequency to symbols which have no frequency in 0th order
	if (ord == -1) { 
		for (int c = 0 ; c <= EOS ; ++c) {
			if (dist[ R(c) ] == last)
				++run; 
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
		// XXX 
		if (dist[ R(c) ] == last) {
			// frequency of successor
			int f = nodecnt[(t << 8) | c];
			if (f > 0) {
				run += f;
				// Count of symbols in context
				++syms;
			}
		}

		last = dist[ R(c) ];
		dist[ R(c) ] = run;
	}
	// Symbols in context, zero frequency for EOS
	dist[ R(EOS) ] = dist[ R(Escape) ] = run + (syms > 0 ? syms : 1); 

	visit.push_back(t);
}

void model::update(const int c) { 
	if (c < 0 || c > Alpha) {
		throw range_error("update character out of range");
	}
	// Check if maximum frequency would be met, rescale if necessary
	bool outscale = false;
	BOOST_FOREACH ( uint32 node, visit ) {
		outscale = (outscale || nodecnt[(node << 8) | c] >= (1 << 15));
	}
	if (outscale) {
		rescale();
	}

	// Update frequency of c from visited nodes
	// Don't update lower order contexts ("update exclusion")
	BOOST_FOREACH ( uint32 node, visit ) {
		++nodecnt[(node << 8) | c];
	}
	visit.clear();

	// Update text context
	if (context.size() == Order)
		context.pop_back();
	context.push_front(c);

}

void model::clean() {
	// TODO clean 'polluted entries'
}

void model::rescale() {
	// Rescale all entries
	for (auto it = nodecnt.begin() ; it != nodecnt.end(); ++it) {
		it->second >>= 1;
		if (it->second < 1) 
			it->second = 1;
	}
	// TODO call clean sometimes
}

} // namespace

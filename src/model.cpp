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
	// TODO initialize datree

	visit.reserve(Order);
}

model::~model() {
	// TODO delete datree
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
	if (context.size() < ord) {
		dist[ R(Escape) ] = dist[ R(EOS) ] = 1;
		return;
	}

	// TODO seek existing context
	for (int c = 0 ; c <= Alpha ; ++c) {
		// TODO c is in context
		// only if symbol had 0 frequency in higher order
		// if (dist[ L(c) ] == dist[ R(c) ])
		// run += node.count(c)
		// Escape frequency is count of symbols in context
		if (dist[ R(c) ] != run)
			++syms; 

		last = dist[ R(c) ];
		dist[ R(c) ] = run;
	}
	// Symbols in context, zero frequency for EOS
	dist[ R(EOS) ] = dist[ R(Escape) ] = run + (syms > 0 ? syms : 1); 

	//visit.insert(node);
}

void model::update(const int c) { 
	if (c < 0 || c > Alpha) {
		throw range_error("update character out of range");
	}
	// Max f met
	BOOST_FOREACH ( int node, visit ) {
		// TODO check if need to rescale
	}
	// Don't update lower order contexts ("update exclusion")
	BOOST_FOREACH ( int node, visit ) {
		// TODO Add to symbol count in contexts used in compression
	}
	visit.clear();
	// Text context
	if (context.size() == Order)
		context.pop_back();
	context.push_front(c);	
}

void model::clean() {
	// TODO clean 'polluted entries'
}

void model::rescale() {
	// TODO rescale all entries
	// TODO call clean sometimes
}

} // namespace

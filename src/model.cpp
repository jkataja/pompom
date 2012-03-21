#include <stdexcept>
#include <cstring>
#include <vector>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include "model.hpp"
#include "pompom.hpp"

using std::range_error;
using std::string;
using std::vector;
using boost::str;
using boost::format;

namespace pompom {

model * model::instance(const uint16 limit) {
	if (limit < 8 || limit > 2048) {
		string err = str( format("accepted limit is %1%-%2% MiB") 
				% LimitMin % LimitMax );
		throw range_error(err);
	}
	return new model(limit);
}

model::model(const uint16 limit) {
	visit.reserve(Order);

	// initialize trie
}

model::~model() {
	// TODO delete trie
}

void model::update(const uint16 c) { 
#ifndef HAPPY_GO_LUCKY
	if (c > Alpha) {
		throw range_error("update character out of range");
	}
#endif
	// Check if maximum frequency would be met, rescale if necessary
	bool outscale = false;
	BOOST_FOREACH ( uint32 node, visit ) {
		outscale = (outscale || nodecnt[(node << 8) | c] >= TopValue - 1);
	}
	if (outscale)
		rescale();

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

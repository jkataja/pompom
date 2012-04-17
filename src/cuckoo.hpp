/**
 * Context frequency logging cuckoo hash for PPM compression. 
 *
 * @see http://www.it-c.dk/people/pagh/papers/cuckoo-jour.pdf
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <stdexcept>
#include <bitset>

#include "pompom.hpp"
#include "pompomdefs.hpp"

namespace pompom {

class cuckoo {
public:
	// Frequency of context
	const uint16 count(const uint64) const;

	// Context is contained in cuckoo hash
	const bool contains(const uint64) const;

	// Size allocated for hash data is full	
	const bool full() const;

	// Reset array contents
	void reset();

	// Rescale all value entries
	void rescale();

	// Insert new context
	const bool insert(uint64);

	// Increase frequency of context
	const bool seen(const uint64);

	// Hashing functions
	const uint32 h1(const uint64) const;
	const uint32 h2(const uint64) const;

	cuckoo(const size_t);
	~cuckoo();
private:
	cuckoo();
	cuckoo(const cuckoo& old);
	const cuckoo& operator=(const cuckoo& old);
	
	// Maximum number of repetitions
	static const uint32 MaxLoop = 10000;

	// Output tree for debugging
	void print_r() const;

	// Recent insert(..) resulted in terminated loop
	bool maxloop_terminated;

	// Context in 64 bit int
	uint64 * keys;

	// Context frequency count
	uint16 * values;

	// Length of allocated keys and values 
	size_t len;
};

cuckoo::cuckoo(const size_t mem) {
	len = (mem * 1 << 20) / (sizeof(uint64) + sizeof(uint16) );

	keys = (uint64 *) malloc(len * sizeof(uint64));
	if (keys == 0) {
		throw std::runtime_error("couldn't allocate cuckoo keys");
	}

	values = (uint16 *) malloc(len * sizeof(uint16));
	if (values == 0) {
		free(keys);
		throw std::runtime_error("couldn't allocate cuckoo values");
	}
	
	reset();
}

cuckoo::~cuckoo() {
	free(keys);
	free(values);
}

void cuckoo::reset() {
	memset(keys, 0, len * sizeof(keys));
	memset(values, 0, len * sizeof(values));
	maxloop_terminated = false;
}

inline
const uint16 cuckoo::count(const uint64 key) const {
	uint32 a = h1(key);
	uint32 b = h2(key);
	if (keys[a] == key)
		return values[a];
	else if (keys[b] == key)
		return values[b];
	return 0;
}

inline
const bool cuckoo::contains(const uint64 key) const {
	return (keys[h1(key)] == key || keys[h2(key)] == key);
}

inline
const bool cuckoo::insert(uint64 key) {
	if (contains(key))
		return true;

	// out of allocated memory
	if (full()) {
		//throw std::runtime_error("out of allocated memory");
		return false;
	}

	// loop n times
	uint32 pos = h1(key);
	uint16 value = 0;
	for (size_t n = 0 ; n < MaxLoop ; ++n) {
		// found an empty nest
		if (keys[pos] == 0) { 
			keys[pos] = key; 
			values[pos] = value;
			return true;
		}

		// find new home nest
		std::swap(key, keys[pos]);
		std::swap(value, values[pos]);
		if (pos == h1(key)) 
			pos = h2(key);
		else 
			pos = h1(key);
	}

	maxloop_terminated = true;
	//rehash(); insert(x)
#ifndef HAPPY_GO_LUCKY
	print_r();
#endif
	return false;
}

inline
const bool cuckoo::seen(const uint64 key) {
	if (!contains(key))
		if (!insert(key))
			return false;

	uint32 a = h1(key);
	uint32 b = h2(key);
	if (keys[a] == key)
		++values[a];
	else // keys[b] == key
		++values[b];
	
	return true;
}

inline
const bool cuckoo::full() const {
	return !maxloop_terminated;
}

void cuckoo::rescale() {
	for (size_t i = 0 ; i < len ; ++i) {
		if (values[i] == 0)
			continue;
		values[i] >>= 1;
		if (values[i] == 0)
			values[i] = 0;

	}
}

void cuckoo::print_r() const {
	int filled = 0;
	for (size_t i = 0 ; i<len ; ++i) {
		if (keys[i] != 0)
			++filled;

	}
	std::cerr << "cuckoo hash terminated, fill rate " << std::setprecision(3) << ((double)filled/len) << std::endl; 
}

inline
const uint32 cuckoo::h1(const uint64 key) const {
	return key % len;
}

inline
const uint32 cuckoo::h2(const uint64 key) const {
	return (key + 1) % len;
}



} // namespace

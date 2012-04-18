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
	INLINE_CANDIDATE const uint16 count(const uint64) const;

	// Context is contained in cuckoo hash
	INLINE_CANDIDATE const bool contains(const uint64) const;

	// Size allocated for hash data is full	
	INLINE_CANDIDATE const bool full() const;

	// Reset array contents
	void reset();

	// Rescale all value entries
	void rescale();

	// Insert new context
	INLINE_CANDIDATE const bool insert(uint64);

	// Increase frequency of context
	INLINE_CANDIDATE const bool seen(const uint64);

	// Hashing functions
	INLINE_CANDIDATE const uint32 h1(const uint64) const;
	INLINE_CANDIDATE const uint32 h2(const uint64) const;

	cuckoo(const size_t);
	~cuckoo();

private:
	cuckoo();
	cuckoo(const cuckoo& old);
	const cuckoo& operator=(const cuckoo& old);
	
	// Maximum number of repetitions
	static const uint32 MaxLoop = 10000;

	// Recent insert(..) resulted in terminated loop
	bool maxloop_terminated;

	// Context in 64 bit int
	uint64 * keys;

	// Context frequency count
	uint16 * values;

	// Length of allocated keys and values 
	size_t len;

	// Output tree (for debugging)
	void print_r() const;

	// Output context key to string (for debugging)
	const std::string key_str(uint64 key) const;
};

cuckoo::cuckoo(const size_t mem) {
	len = (mem * 1 << 20) / (sizeof(uint64) + sizeof(uint16) );

	keys = (uint64 *) malloc(len * sizeof(uint64));
	if (!keys) {
		throw std::runtime_error("couldn't allocate cuckoo keys");
	}

	values = (uint16 *) malloc(len * sizeof(uint16));
	if (!values) {
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
	memset(keys, 0, len * sizeof(uint64));
	memset(values, 0, len * sizeof(uint16));
	maxloop_terminated = false;
}

const uint16 cuckoo::count(const uint64 key) const {
	uint32 a = h1(key);
	if (keys[a] == key)
		return values[a];
	uint32 b = h2(key);
	if (keys[b] == key)
		return values[b];
	return 0;
}

const bool cuckoo::contains(const uint64 key) const {
	return (keys[h1(key)] == key || keys[h2(key)] == key);
}

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
#ifndef UNSAFE
	print_r();
#endif
	return false;
}

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

const bool cuckoo::full() const {
	return maxloop_terminated;
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

const std::string cuckoo::key_str(uint64 key) const {
	std::string s("'");
	int ord = (key >> 56) - 1;
	for (int i = ord ; i >= 0 ; --i) {
		char c = ((key >> (i << 3)) & 0xFF); // characters in context
		s += (c >= ' ' && c <= '~' ? c : '_');
	}
	return s.append("'");
}

void cuckoo::print_r() const {
	int filled = 0;
	for (size_t p = 0 ; p<len ; ++p) {
		if (keys[p] == 0)
			continue;
		std::cerr << key_str(keys[p]) << "\t-> " << values[p]  << "\t" << h1(keys[p]) << "\t" << h2(keys[p]) << std::endl;
		++filled;
	}
	std::cerr << "fill rate " << std::fixed << std::setprecision(3) << ((double)filled/len) << std::endl; 
}

const uint32 cuckoo::h1(const uint64 key) const {
	// fnv-1a
	// @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	static const uint64 FNV_prime = 1099511628211L;
	static const uint64 FNV_offset_basis = 14695981039346656037L;

	uint64 hash = FNV_offset_basis;
	for (int i = 0 ; i < 8 ; ++i) { // -funrolled
		hash = (hash ^ ((key >> (i << 3)) & 0xFF)) * FNV_prime;
	}
	return hash % len;
}

const uint32 cuckoo::h2(const uint64 key) const {
	// Jenkins one-at-a-time
	// @see https://en.wikipedia.org/wiki/Jenkins_hash_function
	uint32_t hash, i;
	for (hash = i = 0 ; i < 8 ; ++i) { // -funrolled
		hash += ((key >> (i << 3)) & 0xFF);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash % len;
}

} // namespace

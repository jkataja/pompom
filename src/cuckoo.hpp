/**
 * Context frequency keeping cuckoo hash for PPM compression. 
 *
 * @see http://www.it-c.dk/people/pagh/papers/cuckoo-jour.pdf
 * @author jkataja
 */

#pragma once

#include <iostream>
#include <stdexcept>

#include "pompom.hpp"
#include "pompomdefs.hpp"

namespace pompom {

class cuckoo {
public:
	// Frequency of context
	inline const uint16 count(const uint64) const;

	// Context is contained in cuckoo hash
	inline const bool contains(const uint64) const;

	// Size allocated for hash data is full	
	inline const bool full() const;

	// Reset array contents
	void reset();

	// Rescale all value entries
	void rescale();

	// Insert new context
	inline const bool insert(uint64);

	// Increase frequency of context
	inline const bool seen(const uint64);

	// Hashing functions
	// CRC32c hardware intrisics in i5/i7 or later
	// FNV-1a in software
	// h1 and h2 differ in taking different byte order 56781234 vs 12345678
	// From test run appears two hashes give fill of approx. 50%
	inline const uint32 h1(const uint64) const;
	inline const uint32 h2(const uint64) const;

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

	// Contexts in 64 bit int (1 byte of length, 7 bytes of context)
	uint64 * keys;

	// Context frequency count
	uint16 * values;

	// Length of allocated keys and values 
	size_t len;

	// Count of filled contexts (used for fill rate)
	const uint32 filled() const;

	// Output context key to string (for debugging)
	const std::string key_str(uint64 key) const;

	// Constants for hashing functions
	static const uint64 FNV_prime = 1099511628211L;
	static const uint64 FNV_offset_basis = 14695981039346656037L;

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
		// found an empty bucket
		if (keys[pos] == 0) { 
			keys[pos] = key; 
			values[pos] = value;
			return true;
		}

		// kick a can down the road
		std::swap(key, keys[pos]);
		std::swap(value, values[pos]);
		if (pos == h1(key)) 
			pos = h2(key);
		else 
			pos = h1(key);
	}

	maxloop_terminated = true;
	//rehash(); insert(x)

#ifdef DEBUG
	uint32 fill = filled();
	float rate = (float)fill/len * 100;
	std::cerr << "reset with load factor " << std::fixed 
			<< std::setprecision(3) << rate << "% "
			<< fill<< "/"<< len << std::endl;
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
	else 
#ifndef UNSAFE
	if (keys[b] == key)
#endif
		++values[b];
#ifndef UNSAFE
	else
		throw new std::runtime_error("no context key match with h1 or h2");
#endif
	
	return true;
}

const bool cuckoo::full() const {
	return maxloop_terminated;
}

void cuckoo::rescale() {
#ifdef DEBUG
	std::cerr << "rescale" << std::endl; 
#endif
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

const uint32 cuckoo::filled() const {
	int filled = 0;
	for (size_t p = 0 ; p<len ; ++p) {
		if (keys[p] == 0)
			continue;
		++filled;
	}
	return filled;
}

const uint32 cuckoo::h1(const uint64 key) const {
#ifdef BUILTIN_CRC
	const uint32 * p = ((const uint32 *) &key);
	uint32 crc = CRCInit;
	crc = __builtin_ia32_crc32si(crc, *(p + 1));
	crc = __builtin_ia32_crc32si(crc, *(p));
	return (crc % len);
#else
	return 0;
#endif
}

const uint32 cuckoo::h2(const uint64 key) const {
#ifdef BUILTIN_CRC
	const uint32 * p = ((const uint32 *) &key);
	uint32 crc = CRCInit;
	crc = __builtin_ia32_crc32si(crc, *(p));
	crc = __builtin_ia32_crc32si(crc, *(p + 1));
	return (crc % len);
#else
	return 1;
#endif
}

} // namespace

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

	// Bit vector with followers
	inline const uint64 * get_follower_vec(const uint64);
	
	// Test if context has follower
	inline const bool has_follower(const uint64, const uint8);

	// Hashing functions
	//
	// Hardware implementation:
	// CRC32c hardware intrisics in i5/i7 or later
	// h1 and h2 differ in taking different byte order 56781234 vs 12345678
	//
	// Software implementation:
	// FNV-1a and Jenkins one-at-a-time
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

	// Key for 0th order context
	static const uint64 RootKey = (1ULL << 63);

	// Recent insert(key) resulted in terminated loop or full bit vectors
	bool is_full;

	// Contexts in 64 bit int (1 byte of length, 7 bytes of context)
	uint64 * keys;

	// Context frequency count
	uint16 * values;

	// Without keeping bit vector of following contexts, the
	// count function took majority of all running time of program.
	// Index in follower_vecs for state.
	uint32 * followers;

	// Check bits for follower in state
	uint64 * follower_vecs;
	uint32 follower_vecs_at;
	uint32 follower_vecs_len;

	// Keep previous follower index for faster access 
	// (no need for hash function calls)
	uint64 follower_lastkey;
	uint32 follower_lastidx;

	// Index of follower bit vector of context
	inline const uint32 follower_idx(const uint64);

	// Length of allocated keys and values 
	size_t len;

	// Set bit of follower in context
	inline const bool set_follower(const uint64, const uint8);

	// Count of filled contexts (used for fill rate)
	const uint32 filled() const;

	// Output verbose output to stderr when filled
	const void filled_verbose() const;

	// Output context key to string (for debugging)
	const std::string key_str(uint64) const;

	// Key of parent context
	inline const uint64 parent_key(const uint64) const;

	// Output following set characters in context
	const void print_set(const uint64 key);

	// Bit vector offset for character
	inline const uint32 off(const uint32, const uint8) const;

	// Bit vector mask for character
	inline const uint64 mask(const uint8) const;

	// Constants for hashing functions
	static const uint64 FNV_prime = 1099511628211ULL;
	static const uint64 FNV_offset_basis = 14695981039346656037ULL;

	// CRC32 checksum initial value
	static const uint32 CRCInit = 0xFFFFFFFFU;

	// Starting index of followers (0 -> not found)
	static const uint32 FollowersBase = 1;

};

cuckoo::cuckoo(const size_t mem) {
	len = (mem * 1 << 20) / 
		(sizeof(uint64) // keys
		+ sizeof(uint16)  // values
		+ sizeof(uint32) // followers bitvector index
		+ (((Alpha >> 6) * sizeof(uint64)) >> 1)); // bitvector

	// 64bit context key
	keys = (uint64 *) malloc(len * sizeof(uint64));
	if (!keys) {
		throw std::runtime_error("couldn't allocate cuckoo keys");
	}

	// 16bit context count value
	values = (uint16 *) malloc(len * sizeof(uint16));
	if (!values) {
		free(keys);
		throw std::runtime_error("couldn't allocate cuckoo values");
	}

	// 32bit index to followers bitvector
	followers = (uint32 *) malloc(len * sizeof(uint32));
	if (!followers) {
		free(keys);
		free(values);
		throw std::runtime_error("couldn't allocate cuckoo followers");
	}

	// 256bit bit vectors for followers
	// Since two hash functions give load factor of ~ 50%,
	// is enough to have half as many slots for follower bit vectors
	follower_vecs_len = (len >> 1);
	follower_vecs = (uint64 *) malloc(follower_vecs_len 
			* (Alpha >> 6) * sizeof(uint64));
	if (!follower_vecs) {
		free(keys);
		free(values);
		free(followers);
		throw std::runtime_error("couldn't allocate cuckoo follower vectors");
	}

	reset();
}

cuckoo::~cuckoo() {
	free(keys);
	free(values);
	free(followers);
	free(follower_vecs);
}

void cuckoo::reset() {
	memset(keys, 0, len * sizeof(uint64));
	memset(values, 0, len * sizeof(uint16));
	memset(followers, 0, len * sizeof(uint32));
	memset(follower_vecs, 0, follower_vecs_len * (Alpha >> 6) 
			* sizeof(uint64)); 
	follower_vecs_at = FollowersBase;
	follower_lastkey = 0;
	follower_lastidx = 0;
	is_full = false;

	// 0th order
	seen(RootKey);
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

const uint32 cuckoo::follower_idx(const uint64 key) {
	if (key == follower_lastkey)
		return follower_lastidx;

	uint32 a = h1(key);
	if (keys[a] == key) {
		follower_lastkey = key;
		return follower_lastidx = followers[a];
	}
	uint32 b = h2(key);
	if (keys[b] == key) {
		follower_lastkey = key;
		return follower_lastidx = followers[b];
	}
	return 0;
}

const bool cuckoo::contains(const uint64 key) const {
	return (keys[h1(key)] == key || keys[h2(key)] == key);
}

const bool cuckoo::insert(uint64 key) {
	if (contains(key))
		return true;

	// Out of allocated memory
	if (full()) {
		return false;
	}

	// No more space for follower bit vectors
	if (follower_vecs_at >= follower_vecs_len - 1) {
		is_full = true;
#ifdef VERBOSE
		filled_verbose();
#endif
		return false;
	}

	// Loop at most MaxLoop times
	uint32 pos = h1(key);
	uint16 value = 0;
	uint32 follower = follower_vecs_at;
	++follower_vecs_at;

	for (size_t n = 0 ; n < MaxLoop ; ++n) {
		// Found an empty bucket
		if (keys[pos] == 0) { 
			keys[pos] = key; 
			values[pos] = value;
			followers[pos] = follower;
			return true;
		}

		// Kick a can down the road
		std::swap(key, keys[pos]);
		std::swap(value, values[pos]);
		std::swap(follower, followers[pos]);
		if (pos == h1(key)) 
			pos = h2(key);
		else 
			pos = h1(key);
	}

	// maxloop terminated marker
	is_full = true; 

#ifdef VERBOSE
	filled_verbose();
#endif

	return false;
}

const void cuckoo::filled_verbose() const {
	uint32 fill = filled();
	float rate = (float)fill/len * 100;
	std::cerr << "reset with load factor " << std::fixed 
			<< std::setprecision(3) << rate << "% "
			<< fill<< "/"<< len << " follower vectors "
			<< follower_vecs_at << "/" << follower_vecs_len
			<< std::endl;
}

const bool cuckoo::seen(const uint64 key) {
	if (!contains(key))
		if (!insert(key))
			return false;

	// 0th order context
	if (key == RootKey) 
		return true;

	uint32 a = h1(key);
	if (keys[a] == key)
		++values[a];
	else
		++values[h2(key)];

	// Set bit for this node in parent context bit vector
	set_follower(parent_key(key), (key & 0xFF));
	
	return true;
}

const uint64 * cuckoo::get_follower_vec(const uint64 key) {
	// index at 0 is empty
	uint32 p = follower_idx(key);
#ifdef DEBUG
	assert (p != 0);
#endif
	return (follower_vecs + off(p,0));
}

const bool cuckoo::has_follower(const uint64 key, const uint8 c) {
	uint32 p = follower_idx(key);
	if (p == 0)
		return false;
	return (mask(c) & follower_vecs[off(p,c)]);
}

const bool cuckoo::set_follower(const uint64 key, const uint8 c) {
	uint32 p = follower_idx(key);
	if (p == 0)
		return false;
	follower_vecs[off(p,c)] |= mask(c);
#ifdef DEBUG
	assert(has_follower(key,c));
#endif
	return true;
}

const bool cuckoo::full() const {
	return is_full;
}

void cuckoo::rescale() {
#ifdef VERBOSE
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
	int ord = (key >> 56) & 0x7F;
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
	// FNV-1a
	// @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	uint64 hash = FNV_offset_basis;
	for (int i = 0 ; i < 8 ; ++i) { // -funrolled
		hash = (hash ^ ((key >> (i << 3)) & 0xFF)) * FNV_prime;
	}
	return (hash % len);
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
	return (hash % len);
#endif
}

const uint32 cuckoo::off(const uint32 p, const uint8 c) const {
	const uint32 off = ((Alpha >> 6) * p + (c >> 6));
#ifdef DEBUG
	assert(off < follower_vecs_len);
#endif
	return off;
}

const uint64 cuckoo::mask(const uint8 c) const {
	return (1ULL << (0x3F - (c & 0x3F)));
}

const uint64 cuckoo::parent_key(const uint64 key) const {
	return (((0xFF00000000000000ULL & key) - (1ULL << 56)) 
		| ((0x00FFFFFFFFFFFFFFULL & key) >> 8));
}

const void cuckoo::print_set(const uint64 key) {
	uint32 p = follower_idx(key);
	if (p == 0)
		return;
	for (int c = 0 ; c <= Alpha ; ++c) {
		if (mask(c) & follower_vecs[off(p,c)])
			std::cerr << c << " ";
	}
	std::cerr << std::endl;
}

} // namespace

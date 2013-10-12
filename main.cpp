#include <stdint.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <time.h>
#include <sstream>
#include <limits>
#include <unistd.h>
#include <math.h>
#define ssefunc __attribute__ ((__target__ ("sse4.1")))
#define always_inline __attribute__ ((always_inline))
#pragma GCC target ("sse4.1")
#ifndef __MMX__
	#define __MMX__
#endif
#ifndef __SSE__
	#define __SSE__
#endif
#ifndef __SSE2__
	#define __SSE2__
#endif
#ifndef __SSE3__
	#define __SSE3__
#endif
#ifndef __SSSE3__
	#define __SSSE3__
#endif
#ifndef __SSE4_1__
	#define __SSE4_1__
#endif
#include <x86intrin.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef int32_t sint32;
typedef uint32_t uint32;
typedef uint64_t uint64;
#define m128 __m128i __attribute__ ((aligned (16))) 

#define LOCAL


/// @see http://fierz.ch/strategy2.htm
/// @see http://senseis.xmp.net/?UCT
/// @see http://www.mcts.ai/?q=mcts
// http://sseplus.sourceforge.net/fntable.html

template<typename T>
T min(const T& a, const T& b)
{
	return (a <= b) ? a : b;
}

template<typename T>
T max(const T& a, const T& b)
{
	return (a >= b) ? a : b;
}

template<typename T>
int sgn(T val)
{
	return (T(0) < val) - (val < T(0));
}

void print128(std::ostream& out, uint8* data)
{
	out.fill('0');
	out << std::hex;
	for(int i = 15; i >= 0; --i) {
		out.width(2);
		out << int(data[i]);
	}
	out.fill(' ');
	out << std::dec;
}

std::ostream& operator<<(std::ostream& out, __m128i in) ssefunc;
inline std::ostream& operator<<(std::ostream& out, __m128i in)
{
	// Avoid http://gcc.gnu.org/bugzilla/show_bug.cgi?id=35414
	uint8 data[16] __attribute__ ((aligned (16)));
	_mm_store_si128((m128*)(data), in);
	print128(out, data);
	return out;
}

class BoardPoint;
class BoardMask;
class Board;

//
//   B O A R D   P O I N T
//

class BoardPoint {
public:
	BoardPoint(): _index(0) { }
	BoardPoint(int index): _index(index) { }
	BoardPoint(int horizontal, int vertical): _index(horizontal | (vertical << 4)) { }
	~BoardPoint() { }
	
	int number() const { return _index; }
	int horizontal() const { return _index & 0x0f; }
	BoardPoint& horizontal(int value) { _index = value | (_index & 0xf0); return *this; }
	int vertical() const { return _index >> 4; }
	BoardPoint& vertical(int value) { _index = (value << 4) | (_index & 0x0f); return *this; }
	
	BoardPoint left() const { return BoardPoint(_index - 1); }
	BoardPoint right() const { return BoardPoint(_index + 1); }
	BoardPoint up() const { return BoardPoint(_index + 16); }
	BoardPoint down() const { return BoardPoint(_index - 16); }
	
protected:
	int _index;
};

std::ostream& operator<<(std::ostream& out, const BoardPoint& point)
{
	char hor[2] = {'A' + char(point.horizontal()), 0x00};
	out << hor << 1 + point.vertical();
	return out;
}

std::istream& operator>>(std::istream& in, BoardPoint& point)
{
	point.horizontal(in.get() - 'A');
	int vertical;
	in >> vertical;
	point.vertical(vertical - 1);
	return in;
}

//
//   B O A R D   M A S K
//

class BoardMask {
public:
	BoardMask() ssefunc;
	BoardMask(const BoardMask& other) ssefunc;
	BoardMask(const BoardPoint& point) ssefunc;
	~BoardMask() ssefunc { }
	BoardMask& operator=(const BoardMask& other) ssefunc;
	bool operator==(const BoardMask& other) const ssefunc;
	bool operator!=(const BoardMask& other) const ssefunc { return !operator==(other); }
	BoardMask& operator&=(const BoardMask& other) ssefunc;
	BoardMask& operator|=(const BoardMask& other) ssefunc;
	BoardMask& operator-=(const BoardMask& other) ssefunc;
	BoardMask operator&(const BoardMask& other) const ssefunc;
	BoardMask operator|(const BoardMask& other) const ssefunc;
	BoardMask operator-(const BoardMask& other) const ssefunc;
	BoardMask operator~() const ssefunc;
	BoardMask expanded() const ssefunc;
	BoardMask connected(const BoardMask& seed) const ssefunc;
	BoardMask rotated() const ssefunc;
	BoardMask& invert() ssefunc { return operator=(operator~()); }
	BoardMask& expand() ssefunc { return operator=(expanded()); }
	BoardMask& rotate() ssefunc { return operator=(rotated()); }
	uint popcount() const ssefunc;
	uint countBridges() const ssefunc;
	BoardMask& clear() ssefunc;
	BoardMask& set(const BoardPoint& point) ssefunc;
	BoardMask& clear(const BoardPoint& point) ssefunc;
	bool isSet(const BoardPoint& point) const ssefunc;
	bool isEmpty() const ssefunc;
	BoardPoint firstPoint() const ssefunc;
	
	
	std::string toMoves() const;
	static BoardMask fromMoves(const std::string& moves);
	
protected:
	friend class PointIterator;
	friend std::ostream& operator<<(std::ostream& out, const BoardMask& boardMask) ssefunc;
	friend std::ostream& operator<<(std::ostream& out, const Board& board);
	m128 bits[2];
	
	static BoardPoint planePoint(int plane, int index);
};

std::ostream& operator<<(std::ostream& out, const BoardMask& boardMask) ssefunc;

inline BoardMask::BoardMask()
{
	clear();
}

inline BoardMask::BoardMask(const BoardMask& other)
{
	operator=(other);
}

inline BoardMask::BoardMask(const BoardPoint& point)
{
	clear();
	set(point);
}

inline BoardMask& BoardMask::operator=(const BoardMask& other)
{
	bits[0] = other.bits[0];
	bits[1] = other.bits[1];
	return *this;
}

inline bool BoardMask::operator==(const BoardMask& other) const
{
	m128 vectCompare = _mm_cmpeq_epi8(bits[0], other.bits[0]);
	vectCompare = _mm_and_si128(vectCompare, _mm_cmpeq_epi8(bits[1], other.bits[1]));
	return _mm_movemask_epi8(vectCompare) == 0xffff;
}

BoardMask BoardMask::expanded() const
{
	BoardMask result;
	m128 mask = _mm_set1_epi16(0x7fff);
	result.bits[0]  = bits[0];
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_epi16(bits[0], 1));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_srli_epi16(bits[0], 1));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_si128(bits[0], 2));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_srli_si128(bits[0], 2));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_si128(bits[1], 14));
	result.bits[0] = _mm_and_si128(result.bits[0], mask);
	mask = _mm_xor_si128(mask, _mm_slli_si128(mask, 14));
	result.bits[1] = bits[1];
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_si128(bits[0], 14));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_slli_epi16(bits[1], 1));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_epi16(bits[1], 1));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_slli_si128(bits[1], 2));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_si128(bits[1], 2));
	result.bits[1] = _mm_and_si128(result.bits[1], mask);
	return result;
}

/// NOTE: This function is by far the bottleneck of the application
BoardMask BoardMask::connected(const BoardMask& seed) const
{
	// Unpack to registers
	m128 s0 = seed.bits[0];
	m128 s1 = seed.bits[1];
	m128 m0 = bits[0];
	m128 m1 = bits[1];
	m128 p0;
	m128 p1;
	
	// Make sure the seed is not outside the mask
	s0 = _mm_and_si128(s0, m0);
	s1 = _mm_and_si128(s1, m1);
	
	// Loop until no more connections
	do {
		
		// Fully extend in the right direction
		s0 = _mm_or_si128(s0, _mm_andnot_si128(_mm_add_epi16(s0, m0), m0));
		s1 = _mm_or_si128(s1, _mm_andnot_si128(_mm_add_epi16(s1, m1), m1));
		
		// Expansion round in other directions, with fixed-point check
		p0 = s0;
		p1 = s1;
		s0 = _mm_or_si128(s0, _mm_srli_epi16(p0, 1));
		s1 = _mm_or_si128(s1, _mm_srli_epi16(p1, 1));
		s0 = _mm_or_si128(s0, _mm_slli_si128(p0, 2));
		s1 = _mm_or_si128(s1, _mm_slli_si128(p1, 2));
		s0 = _mm_or_si128(s0, _mm_srli_si128(p0, 2));
		s1 = _mm_or_si128(s1, _mm_srli_si128(p1, 2));
		s0 = _mm_or_si128(s0, _mm_slli_si128(p1, 14));
		s1 = _mm_or_si128(s1, _mm_srli_si128(p0, 14));
		s0 = _mm_and_si128(s0, m0);
		s1 = _mm_and_si128(s1, m1);
		
		// Compare s and p
	} while(_mm_movemask_epi8(_mm_and_si128(_mm_cmpeq_epi8(p0, s0), _mm_cmpeq_epi8(p1, s1))) != 0xffff);
	
	// Pack into boardmask
	BoardMask result;
	result.bits[0] = s0;
	result.bits[1] = s1;
	return result;
}

BoardMask BoardMask::rotated() const
{
	// This is here because the code is so fast and neat, it is not really useful
	m128 a = bits[0];
	m128 b = bits[1];
	m128 x = _mm_setzero_si128();
	m128 y = _mm_setzero_si128();
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 0);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 1);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 2);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 3);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 4);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 5);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 6);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	x = _mm_insert_epi16(x, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 7);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 0);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 1);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 2);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 3);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 4);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 5);
	a = _mm_slli_epi16(a, 1); b = _mm_slli_epi16(b, 1);
	y = _mm_insert_epi16(y, _mm_movemask_epi8(_mm_packs_epi16(a, b)), 6);
	BoardMask bm;
	bm.bits[0] = x;
	bm.bits[1] = y;
	return bm;
}

uint BoardMask::popcount() const
{
	m128 a = bits[0];
	m128 b = bits[1];
	
	// Replace all the bytes in a and b are by their popcount
	const m128 mask0 = _mm_set1_epi8(0x55); // 0101 0101
	a = _mm_sub_epi8(a, _mm_and_si128(_mm_srli_epi64(a, 1), mask0));
	b = _mm_sub_epi8(b, _mm_and_si128(_mm_srli_epi64(b, 1), mask0));
	const m128 mask1 = _mm_set1_epi8(0x33); // 0011 0011
	a = _mm_add_epi8(_mm_and_si128(a, mask1), _mm_and_si128(_mm_srli_epi64(a, 2), mask1));
	b = _mm_add_epi8(_mm_and_si128(b, mask1), _mm_and_si128(_mm_srli_epi64(b, 2), mask1));
	const m128 mask2 = _mm_set1_epi8(0x0f); // 0000 1111
	a = _mm_and_si128(_mm_add_epi8(a, _mm_srli_epi64(a, 4)), mask2);
	b = _mm_and_si128(_mm_add_epi8(b, _mm_srli_epi64(b, 4)), mask2);
	
	// Sum the bytes
	a = _mm_add_epi8(a, b);
	a = _mm_add_epi8(a, _mm_srli_si128(a, 1));
	a = _mm_add_epi8(a, _mm_srli_si128(a, 2));
	a = _mm_add_epi8(a, _mm_srli_si128(a, 4));
	a = _mm_add_epi8(a, _mm_srli_si128(a, 8));
	
	// Return the result
	return _mm_cvtsi128_si32(a) & 0xff;
}

uint BoardMask::countBridges() const
{
	m128 a = bits[0];
	m128 b = bits[1];
	
	// Horizontal bridges
	m128 h0 = _mm_and_si128(a, _mm_slli_epi16(a, 1));
	m128 h1 = _mm_and_si128(b, _mm_slli_epi16(b, 1));
	
	// Vertical bridges
	m128 v0 = _mm_and_si128(a, _mm_slli_si128(a, 2));
	m128 v1 = _mm_and_si128(b, _mm_or_si128(_mm_slli_si128(b, 2), _mm_srli_si128(a, 14)));
	
	// Count them
	BoardMask result;
	result.bits[0] = _mm_or_si128(h0, v0);
	result.bits[1] = _mm_or_si128(h1, v1);
	return result.popcount();
}

inline BoardMask& BoardMask::operator&=(const BoardMask& other)
{
	bits[0] = _mm_and_si128(bits[0], other.bits[0]);
	bits[1] = _mm_and_si128(bits[1], other.bits[1]);
	return *this;
}

inline BoardMask& BoardMask::operator|=(const BoardMask& other)
{
	bits[0] = _mm_or_si128(bits[0], other.bits[0]);
	bits[1] = _mm_or_si128(bits[1], other.bits[1]);
	return *this;
}

inline BoardMask& BoardMask::operator-=(const BoardMask& other)
{
	bits[0] = _mm_andnot_si128(other.bits[0], bits[0]);
	bits[1] = _mm_andnot_si128(other.bits[1], bits[1]);
	return *this;
}

inline BoardMask BoardMask::operator&(const BoardMask& other) const
{
	return BoardMask(*this).operator&=(other);
}

inline BoardMask BoardMask::operator|(const BoardMask& other) const
{
	return BoardMask(*this).operator|=(other);
}

inline BoardMask BoardMask::operator-(const BoardMask& other) const
{
	return BoardMask(*this).operator-=(other);
}

inline BoardMask BoardMask::operator~() const
{
	BoardMask result;
	m128 mask = _mm_set1_epi16(0x7fff);
	result.bits[0] = _mm_andnot_si128(bits[0], mask);
	mask = _mm_xor_si128(mask, _mm_slli_si128(mask, 14));
	result.bits[1] = _mm_andnot_si128(bits[1], mask);
	return result;
}

inline BoardMask& BoardMask::clear()
{
	bits[0] = _mm_setzero_si128();
	bits[1] = _mm_setzero_si128();
	return *this;
}

inline BoardMask& BoardMask::set(const BoardPoint& point)
{
	union {
		uint16 rows[16];
		m128 b[2];
	};
	b[0] = _mm_setzero_si128();
	b[1] = _mm_setzero_si128();
	rows[point.vertical()] = 1 << point.horizontal();
	bits[0] = _mm_or_si128(bits[0], b[0]);
	bits[1] = _mm_or_si128(bits[1], b[1]);
	return *this;
}

inline BoardMask& BoardMask::clear(const BoardPoint& point)
{
	union {
		uint16 rows[16];
		m128 b[2];
	};
	b[0] = _mm_setzero_si128();
	b[1] = _mm_setzero_si128();
	rows[point.vertical()] = 1 << point.horizontal();
	bits[0] = _mm_andnot_si128(b[0], bits[0]);
	bits[1] = _mm_andnot_si128(b[1], bits[1]);
	return *this;
}

inline bool BoardMask::isSet(const BoardPoint& point) const
{
	union {
		uint16 rows[16];
		m128 b[2];
	};
	b[0] = bits[0];
	b[1] = bits[1];
	return rows[point.vertical()] & (1 << point.horizontal());
}

inline bool BoardMask::isEmpty() const
{
	return operator==(BoardMask());
}

BoardPoint BoardMask::firstPoint() const
{
	uint32 part;
	part = _mm_cvtsi128_si32(bits[0]);
	if(part)
		return BoardPoint(__builtin_ctzl(part));
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[0], 4));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 32);
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[0], 8));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 64);
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[0], 12));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 96);
	part = _mm_cvtsi128_si32(bits[1]);
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 128);
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[1], 4));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 160);
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[1], 8));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 192);
	part = _mm_cvtsi128_si32(_mm_srli_si128(bits[1], 12));
	if(part)
		return BoardPoint(__builtin_ctzl(part) + 224);
	return BoardPoint();
}

void printMask(std::ostream& out, const uint16* data)
{
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	for(int y = 14; y >= 0; --y) {
		out.width(2);
		out << y + 1 << " ";
		for(int x = 0; x < 15; ++x)
			out << ((data[y] & (1 << x)) ? "0" : ".");
		out << " ";
		out.width(2);
		out << y + 1;
		out << std::endl;
	}
	out << "   ABCDEFGHIJKLMNO" << std::endl;
}

inline std::ostream& operator<<(std::ostream& out, const BoardMask& in)
{
	// Avoid http://gcc.gnu.org/bugzilla/show_bug.cgi?id=35414
	uint16 data[16] __attribute__ ((aligned (16)));
	_mm_store_si128((m128*)(data), in.bits[0]);
	_mm_store_si128((m128*)(data + 8), in.bits[1]);
	printMask(out, data);
	return out;
}

//
//   P O I N T   I T E R A T O R
//

class PointIterator
{
public:
	static BoardPoint firstPoint(const BoardMask& boardMask) ssefunc;
	static BoardPoint randomPoint(const BoardMask& boardMask) ssefunc;
	
	PointIterator(const BoardMask& boardMask) ssefunc;
	~PointIterator() { }
	
	bool next() ssefunc;
	BoardPoint point() const { return _point; }
	
protected:
	const BoardMask& _boardMask;
	BoardPoint _point;
	int _plane;
	uint32 _bits;
};

inline PointIterator::PointIterator(const BoardMask& boardMask)
: _boardMask(boardMask)
, _point()
, _plane(0)
, _bits(_mm_cvtsi128_si32(_boardMask.bits[0]))
{
}

inline bool PointIterator::next()
{
	while(_bits == 0) {
		if(_plane == 7)
			return false;
		++_plane;
		m128 vector = (_plane < 4) ? _boardMask.bits[0] : _boardMask.bits[1];
		switch(_plane % 4) {
		case 0: _bits = _mm_cvtsi128_si32(vector); break;
		case 1: _bits = _mm_cvtsi128_si32(_mm_srli_si128(vector, 4)); break;
		case 2: _bits = _mm_cvtsi128_si32(_mm_srli_si128(vector, 8)); break;
		case 3: _bits = _mm_cvtsi128_si32(_mm_srli_si128(vector, 12)); break;
		}
	}
	int index = __builtin_ctzl(_bits);
	_point = BoardPoint(index + 32 * _plane);
	_bits ^= 1UL << index;
	return true;
}

BoardPoint PointIterator::firstPoint(const BoardMask& boardMask)
{
	PointIterator pi(boardMask);
	pi.next();
	return pi.point();
}

BoardPoint PointIterator::randomPoint(const BoardMask& boardMask)
{
	int pointIndex = rand() % boardMask.popcount();
	PointIterator pi(boardMask);
	do
		pi.next();
	while(pointIndex--);
	return pi.point();
}


//
//  B O A R D   M A S K   M O V E S
//

std::string BoardMask::toMoves() const
{
	std::stringstream result;
	PointIterator pi(*this);
	bool empty = true;
	while(pi.next()) {
		if(!empty)
			result << "-";
		result << pi.point();
		empty = false;
	}
	return result.str();
}

BoardMask BoardMask::fromMoves(const std::string& moves)
{
	std::stringstream ss(moves);
	BoardPoint move;
	BoardMask result;
	while(ss.good()) {
		ss >> move;
		std::cerr << "Move " << move << std::endl;
		ss.get(); // the minus character
		result.set(move);
	}
	return result;
}

//
//   G R O U P   I T E R A T O R
//

class GroupIterator
{
public:
	static int count(const BoardMask& boardMask) ssefunc;
	
	GroupIterator(const BoardMask& boardMask) ssefunc;
	~GroupIterator() ssefunc { }
	
	bool hasNext() const ssefunc { return !_remaining.isEmpty(); }
	bool next() ssefunc;
	
	BoardMask group() const ssefunc { return _group; } 
	
protected:
	BoardMask _remaining;
	BoardMask _group;
};

int GroupIterator::count(const BoardMask& boardMask)
{
	int result = 0;
	GroupIterator gi(boardMask);
	while(gi.next())
		++result;
	return result;
}

GroupIterator::GroupIterator(const BoardMask& boardMask)
: _remaining(boardMask)
, _group()
{
}

bool GroupIterator::next()
{
	if(_remaining.isEmpty())
		return false;
	
	// Find the next group
	_group = _remaining.connected(BoardMask(_remaining.firstPoint()));
	
	// Subtract group from remaining
	_remaining -= _group;
	return true;
}

//
//   B O A R D
//

class Board {
public:
	Board() ssefunc;
	Board(const Board& board) ssefunc;
	~Board() ssefunc { }
	
	const BoardMask& white() const ssefunc { return _white; }
	Board& white(const BoardMask& value) ssefunc { _white = value; return *this; }
	const BoardMask& black() const ssefunc { return _black; }
	Board& black(const BoardMask& value) ssefunc { _black = value; return *this; }
	const BoardMask& player() const ssefunc { return (whiteToMove()) ? _white : _black; }
	const BoardMask& opponent() const ssefunc { return (whiteToMove()) ? _black : _white; }
	bool hasExpanded() const { return _hasExpanded; }
	bool whiteToMove() const { return (_turn % 2) == 0; }
	bool blackToMove() const { return (_turn % 2) == 1; }
	
	bool gameOver() const ssefunc;
	sint32 score() const ssefunc;
	uint32 turn() const { return _turn; }
	
	void playTurn(const BoardMask& move) ssefunc;
	void whiteTurn(const BoardMask& move) ssefunc;
	void blackTurn(const BoardMask& move) ssefunc;
	
protected:
	friend std::ostream& operator<<(std::ostream& out, const Board& board);
	const static uint64 zobristWhite[15*15];
	const static uint64 zobristBlack[15*15];
	BoardMask _white;
	BoardMask _black;
	bool _hasExpanded;
	uint32 _turn;
};

std::ostream& operator<<(std::ostream& out, const Board& board) ssefunc;

Board::Board()
: _white()
, _black()
, _hasExpanded(false)
, _turn(0)
{
}

Board::Board(const Board& board)
: _white(board._white)
, _black(board._black)
, _hasExpanded(board._hasExpanded)
, _turn(board._turn)
{
}

void Board::playTurn(const BoardMask& move)
{
	if(whiteToMove())
		whiteTurn(move);
	else
		blackTurn(move);
}

void Board::whiteTurn(const BoardMask& move)
{
	// Update the basics
	bool exploreMove = (_white.expanded() & move).isEmpty();
	_hasExpanded |= !exploreMove;
	_white |= move;
	++_turn;
}

void Board::blackTurn(const BoardMask& move)
{
	bool exploreMove = (_black.expanded() & move).isEmpty();
	_hasExpanded |= !exploreMove;
	_black |= move;
	++_turn;
}

bool Board::gameOver() const
{
	return (~(_white | _black)).isEmpty(); 
}

sint32 Board::score() const
{
	sint32 score = 0;
	
	// Each piece is one point
	score += white().popcount();
	score -= black().popcount();
	
	// For each separate group 6 points will be subtracted.
	score -= 6 * GroupIterator::count(white());
	score += 6 * GroupIterator::count(black());
	
	// The player with the highest score gets 100 bonus points.
	if(gameOver())
		score += 100 * sgn(score);
	
	return score;
}

void printBoard(std::ostream& out, uint16* white, uint16* black, uint32 turn)
{
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	for(int y = 14; y >= 0; --y) {
		out.width(2);
		out << y + 1 << " ";
		for(int x = 0; x < 15; ++x) {
			if(black[y] & (1 << x))
				out << "B";
			else if(white[y] & (1 << x))
				out << "W";
			else
				out << ".";
		}
		out << " ";
		out.width(2);
		out << y + 1;
		out << std::endl;
	}
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	
	out << "turn: " << turn << ((turn % 2 == 0) ? " white to move" : " black to move")  << std::endl;
}

inline std::ostream& operator<<(std::ostream& out, const Board& board)
{
	uint16 white[16] __attribute__ ((aligned (16)));
	uint16 black[16] __attribute__ ((aligned (16)));
	_mm_store_si128((m128*)(white), board._white.bits[0]);
	_mm_store_si128((m128*)(white + 8), board._white.bits[1]);
	_mm_store_si128((m128*)(black), board._black.bits[0]);
	_mm_store_si128((m128*)(black + 8), board._black.bits[1]);
	printBoard(out, white, black, board.turn());
	return out;
}

//
//  S C O R E   H E U R I S T I C
//

class ScoreHeuristic {
public:
	ScoreHeuristic();
	ScoreHeuristic(sint32, sint32, sint32, sint32, sint32, sint32, sint32, sint32, sint32);
	~ScoreHeuristic() { }
	static sint32 mix(sint32 a, sint32 b, sint32 left, sint32 right, sint32 pos);
	
	// Game scores, multiplied by 1000
	static const sint32 piecePoints = 1000;
	static const sint32 deadGroupPoints = -6000;
	
	// Heuristics
	sint32 earlyGroupPoints;
	sint32 earlyManyGroupPoints;
	sint32 manyTransition;
	sint32 earlyTransistionBegin;
	sint32 earlyTransistionEnd;
	sint32 firstFreedomPoints;
	sint32 secondFreedomPoints;
	sint32 thirdFreedomPoints;
	sint32 fourthFreedomPoints;
	
	void irradiate(sint32 sievert);
	
	sint32 evaluate(const BoardMask& player, const BoardMask& opponent, uint playerLiveGroups, uint opponentLiveGroups) const ssefunc;
	sint32 evaluate(const Board& board) const ssefunc;
	
	uint cohesion(const BoardMask& player, const BoardMask& unoccupied) const ssefunc;
	
protected:
	void irradiate(sint32* parameter, sint32 sievert);
};

ScoreHeuristic::ScoreHeuristic()
: earlyGroupPoints(10197)
, earlyManyGroupPoints(-2277)
, manyTransition(6)
, earlyTransistionBegin(147)
, earlyTransistionEnd(215)
, firstFreedomPoints(130)
, secondFreedomPoints(66)
, thirdFreedomPoints(29)
, fourthFreedomPoints(13)
{
}

ScoreHeuristic::ScoreHeuristic(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f, sint32 g, sint32 h, sint32 i)
: earlyGroupPoints(a)
, earlyManyGroupPoints(b)
, manyTransition(c)
, earlyTransistionBegin(d)
, earlyTransistionEnd(e)
, firstFreedomPoints(f)
, secondFreedomPoints(g)
, thirdFreedomPoints(h)
, fourthFreedomPoints(i)
{
}

sint32 ScoreHeuristic::mix(sint32 a, sint32 b, sint32 left, sint32 right, sint32 pos)
{
	if(pos <= left)
		return a;
	if(pos >= right);
		return b;
	sint32 leftWeight = pos - left;
	sint32 rightWeight = right - pos;
	sint32 sum = right - left;
	return ((a * leftWeight) + (b * rightWeight)) / sum;
}

void ScoreHeuristic::irradiate(sint32 sievert)
{
	// Irradiate 
	irradiate(&earlyGroupPoints, sievert);
	irradiate(&earlyManyGroupPoints, sievert);
	irradiate(&manyTransition, sievert);
	irradiate(&earlyTransistionBegin, sievert);
	irradiate(&earlyTransistionEnd, sievert);
	irradiate(&firstFreedomPoints, sievert);
	irradiate(&secondFreedomPoints, sievert);
	irradiate(&thirdFreedomPoints, sievert);
	irradiate(&fourthFreedomPoints, sievert);
	
	// Sanetize
	if(manyTransition > 40)
		manyTransition = 40;
	if(manyTransition < 0)
		manyTransition = 0;
	if(earlyTransistionBegin < 0)
		earlyTransistionBegin = 0;
	if(earlyTransistionEnd > 225)
		earlyTransistionEnd = 225;
	if(earlyTransistionBegin > earlyTransistionEnd)
		earlyTransistionEnd = earlyTransistionBegin;
}

void ScoreHeuristic::irradiate(sint32* parameter, sint32 sievert)
{
	// sievert is the maximum mutation expressed in thousands of the value
	sint32 value = *parameter;
	sint32 maxDeviation = abs((value * sievert) / 1000);
	if(maxDeviation <= 0)
		maxDeviation = 1;
	sint32 min = value - maxDeviation;
	sint32 max = value + maxDeviation;
	sint32 newValue;
	newValue = min + (rand() % (max - min));
	*parameter = newValue;
}

sint32 ScoreHeuristic::evaluate(const BoardMask& player, const BoardMask& opponent, uint playerLiveGroups, uint opponentLiveGroups) const
{
	/// @todo Distinguish live versus dead groups
	///  - Group 'liveliness' ?
	
	/// @todo Measure 'influence',
	///  - areas you can reach before the opponent
	///  - small surrounded areas (the smaller the better!)
	
	/// @todo Measure distance between groups, how many moves to connect all groups?
	///       Discount if groups can not be connected anymore?
	
	/// @todo Count the number of groups created by ~player. i.e. the division of the board
	
	sint32 score = 0;
	sint32 playerPieces = player.popcount();
	sint32 opponentPieces = opponent.popcount();
	sint32 progress = playerPieces + opponentPieces;
	
	// The score of each player is determined by counting the number of stones he has placed on the board. 
	score += piecePoints * (playerPieces - opponentPieces);
	
	/// NEW: Count the number of groups created by the players
	score += 10000 * GroupIterator::count(~player);
	score -= 10000 * GroupIterator::count(~opponent);
	
	BoardMask unoccupied = ~(player | opponent);
	
	/// NEW: Differentiate live and non-live groups
	playerLiveGroups = 0;
	opponentLiveGroups = 0;
	uint playerDeadGroups = 0;
	uint opponentDeadGroups = 0;
	uint playerSepparation = 0;
	uint opponentSepparation = 0;
	GroupIterator gip(player);
	while(gip.next()) {
		const BoardMask& group = gip.group();
		if((group.expanded() & unoccupied).isEmpty())
			++playerDeadGroups;
		else
			++playerLiveGroups;
		
		// NEW: Count the number of moves to connect the groups to each other
		BoardMask other = player - group;
		BoardMask groupExpand = group;
		uint groupSepparation = 0;
		for(;;) {
			groupExpand = groupExpand.expanded() & unoccupied;
			if(groupExpand & other)
				break;
			++groupSepparation;
		}
		playerSepparation += groupSepparation;
	}
	GroupIterator gio(opponent);
	while(gio.next()) {
		const BoardMask& group = gio.group();
		if((group.expanded() & unoccupied).isEmpty())
			++opponentDeadGroups;
		else
			++opponentLiveGroups;
		
		
	}
	
	// Six point subtracted for dead groups
	score += deadGroupPoints * (playerDeadGroups - opponentDeadGroups);
	
	// For each separate group 6 points will be subtracted.
	sint32 lateGroupScore = deadGroupPoints * (playerLiveGroups - opponentLiveGroups);
	sint32 earlyGroupScore = 0;
	if(playerLiveGroups <= manyTransition) {
		earlyGroupScore += earlyGroupPoints * playerLiveGroups;
	} else {
		earlyGroupScore += earlyGroupPoints * manyTransition;
		earlyGroupScore += earlyManyGroupPoints * (playerLiveGroups - manyTransition);
	}
	if(opponentLiveGroups <= manyTransition) {
		earlyGroupScore -= earlyGroupPoints * opponentLiveGroups;
	} else {
		earlyGroupScore -= earlyGroupPoints * manyTransition;
		earlyGroupScore -= earlyManyGroupPoints * (opponentLiveGroups - manyTransition);
	}
	score += mix(earlyGroupScore, lateGroupScore, earlyTransistionBegin, earlyTransistionEnd, progress);
	
	// Add points for expansion room
	BoardMask playerExpanded = player.expanded() & unoccupied;
	BoardMask opponentExpanded = opponent.expanded() & unoccupied;
	score += (firstFreedomPoints - secondFreedomPoints - thirdFreedomPoints - fourthFreedomPoints) * (playerExpanded.popcount() - opponentExpanded.popcount());
	playerExpanded.expand();
	playerExpanded &= unoccupied;
	opponentExpanded.expand();
	opponentExpanded &= unoccupied;
	score += (secondFreedomPoints - thirdFreedomPoints - fourthFreedomPoints) * (playerExpanded.popcount() - opponentExpanded.popcount());
	playerExpanded.expand();
	playerExpanded &= unoccupied;
	opponentExpanded.expand();
	opponentExpanded &= unoccupied;
	score += (thirdFreedomPoints - fourthFreedomPoints) * (playerExpanded.popcount() - opponentExpanded.popcount());
	playerExpanded.expand();
	playerExpanded &= unoccupied;
	opponentExpanded.expand();
	opponentExpanded &= unoccupied;
	score += fourthFreedomPoints * (playerExpanded.popcount() - opponentExpanded.popcount());
	return score;
}

sint32 ScoreHeuristic::evaluate(const Board& board) const
{
	BoardMask unoccupied = ~(board.white() | board.black());
	uint playerLiveGroups = 0;
	uint opponentLiveGroups = 0;
	uint playerDeadGroups = 0;
	uint opponentDeadGroups = 0;
	
	GroupIterator playerGroups(board.player());
	while(playerGroups.next()) {
		BoardMask group = playerGroups.group();
		if((group.expanded() & unoccupied).isEmpty())
			++playerDeadGroups;
		else
			++playerLiveGroups;
	}
	GroupIterator opponentGroups(board.opponent());
	while(opponentGroups.next()) {
		BoardMask group = opponentGroups.group();
		if((group.expanded() & unoccupied).isEmpty())
			++opponentDeadGroups;
		else
			++opponentLiveGroups;
	}
	
	return evaluate(
		board.player(),
		board.opponent(),
		playerLiveGroups + playerDeadGroups,
		opponentLiveGroups + opponentDeadGroups
	);
}

/// Returns the number of stones required to connect as many of the groups of the player as possible
/// @note It may be impossible to connect some groups!
uint ScoreHeuristic::cohesion(const BoardMask& player, const BoardMask& unoccupied) const
{
	
}

std::ostream& operator<<(std::ostream& out, const ScoreHeuristic& heuristic)
{
	out << "Heuristic(";
	out << heuristic.earlyGroupPoints << ", ";
	out << heuristic.earlyManyGroupPoints << ", ";
	out << heuristic.manyTransition << ", ";
	out << heuristic.earlyTransistionBegin << ", ";
	out << heuristic.earlyTransistionEnd << ", ";
	out << heuristic.firstFreedomPoints << ", ";
	out << heuristic.secondFreedomPoints << ", ";
	out << heuristic.thirdFreedomPoints << ", ";
	out << heuristic.fourthFreedomPoints << ")";
	return out;
}

//
//   T U R N   I T E R A T O R
//

class GreedyMovesFinder
{
public:
	static BoardMask bestMove(const Board& board, const ScoreHeuristic& heuristic) ssefunc;
	
	GreedyMovesFinder(const Board& board, const ScoreHeuristic& heuristic) ssefunc;
	~GreedyMovesFinder() ssefunc { }
	
	BoardMask bestMove() ssefunc;
	
	bool done() const ssefunc { return _moves.isEmpty(); }
	const BoardMask& moves() const ssefunc { return _moves; }
	void choose(const BoardPoint& point) ssefunc;
	
	const BoardMask& turnMoves() const ssefunc { return _turnMoves; }
	
protected:
	const ScoreHeuristic& _heuristic;
	const BoardMask& _player;
	const BoardMask& _opponent;
	BoardMask _turnMoves;
	BoardMask _moves;
};

BoardMask GreedyMovesFinder::bestMove(const Board& board, const ScoreHeuristic& heuristic)
{
	GreedyMovesFinder gmi(board, heuristic);
	return gmi.bestMove();
}

GreedyMovesFinder::GreedyMovesFinder(const Board& board, const ScoreHeuristic& heuristic)
: _heuristic(heuristic)
, _player(board.player())
, _opponent(board.opponent())
, _turnMoves()
, _moves(~(_player | _opponent)) // All empty spaces
{
}

void GreedyMovesFinder::choose(const BoardPoint& point)
{
	_turnMoves.set(point);
	BoardMask unoccupied = ~(_player | _opponent);
	
	// The player can make one exploration move
	if(!_player.expanded().isSet(point)) {
		_moves = BoardMask();
		return;
	}
	
	// Or the player can expand every group by one stone
	GroupIterator gi(_player);
	_moves = _player.expanded() & unoccupied;
	while(gi.next()) {
		BoardMask groupExpansion = gi.group().expand();
		groupExpansion &= unoccupied;
		if(!(_turnMoves & groupExpansion).isEmpty())
			_moves -= groupExpansion;
	}
}

BoardMask GreedyMovesFinder::bestMove()
{
	/// @todo Go over all opponents single-piece-moves and take the minimum
	/// Will this will work when doing a greedy search?
	
	uint opponentGroups = GroupIterator::count(_opponent);
	while(!done()) {
		BoardPoint bestPoint;
		sint32 bestScore = -0x7FFFFFFF;
		uint bestCount = 0;
		PointIterator pi(moves());
		while(pi.next()) {
			BoardMask player =  _player | turnMoves();
			player.set(pi.point());
			
			Board b;
			b.white(player);
			b.black(_opponent);
			sint32 score = _heuristic.evaluate(b);
			
			if(score > bestScore) {
				bestScore = score;
				bestPoint = pi.point();
				bestCount = 1;
			} else if (score == bestScore && (rand() % (bestCount++) == 0)) {
				bestPoint = pi.point();
			}
		}
		choose(bestPoint);
	}
	return turnMoves();
}

//
//   M O V E S   F I N D E R
//

class MovesFinder
{
public:
	static BoardMask bestOrGreedy(const Board& board, const ScoreHeuristic& heuristic) ssefunc;
	static BoardMask bestMove(const Board& board, const ScoreHeuristic& heuristic) ssefunc;
	
	MovesFinder(const Board& board, const ScoreHeuristic& heuristic) ssefunc;
	~MovesFinder() ssefunc { }
	void findMoves() ssefunc;
	const BoardMask& bestMove() const ssefunc { return _bestMove; }
	const BoardMask& randomMove() const ssefunc { return _randomMove; }
	
	void printStats();
	
	static uint total() { return _total; }
	static void total(uint value) { _total = value; }
	
protected:
	const Board& _board;
	const ScoreHeuristic& _heuristic;
	BoardMask _groupExpanded[100];
	uint _groupExpandedSize;
	uint _opponentGroupCount;
	uint _playerDeadGroupCount;
	BoardMask _bestMove;
	sint32 _bestScore;
	uint _bestMoveCount;
	BoardMask _randomMove;
	uint _count;
	static uint _total;
	
	void expandMoves(uint index, const BoardMask& expandable, BoardMask& movePoints) ssefunc;
	void evaluateMove(const BoardMask& movePoints) ssefunc;
	sint32 estimateScore(const BoardMask& movePoints) ssefunc;
};

uint MovesFinder::_total = 0;

BoardMask MovesFinder::bestOrGreedy(const Board& board, const ScoreHeuristic& heuristic)
{
	// Fall back to a faster greedy search if the board gets to complex
	uint progress = (board.white() | board.black()).popcount(); 
	if(_total > 5000000 && progress < 140)
		return GreedyMovesFinder::bestMove(board, heuristic);
	else
		return MovesFinder::bestMove(board, heuristic);
}

BoardMask MovesFinder::bestMove(const Board& board, const ScoreHeuristic& heuristic)
{
	MovesFinder mf(board, heuristic);
	mf.findMoves();
	mf.printStats();
	return mf.bestMove();
}

MovesFinder::MovesFinder(const Board& board, const ScoreHeuristic& heuristic)
: _board(board)
, _heuristic(heuristic)
, _groupExpanded()
, _groupExpandedSize(0)
, _opponentGroupCount(0)
, _playerDeadGroupCount(0)
, _bestMove()
, _bestScore(-0x7FFFFFFF)
, _count(0)
{
}

void MovesFinder::findMoves()
{
	// Do some board algebra
	BoardMask unoccupied = ~(_board.player() | _board.opponent());
	BoardMask expandable = _board.player().expanded() & unoccupied;
	BoardMask explorable = unoccupied - expandable;
	
	// Calculate the opponent groups
	_opponentGroupCount = GroupIterator::count(_board.opponent());
	
	// Collect the groups
	GroupIterator gi(_board.player());
	while(gi.next()) {
		BoardMask group = gi.group();
		BoardMask groupExpandable = group.expanded() & expandable;
		if(groupExpandable.isEmpty())
			++_playerDeadGroupCount;
		else
			_groupExpanded[_groupExpandedSize++] = groupExpandable;
	}
	
	// Evaluate the explore moves
	PointIterator exploreMoves(explorable);
	while(exploreMoves.next())
		evaluateMove(exploreMoves.point());
	
	// Recursively search the expansion moves
	BoardMask movePoints;
	expandMoves(0, expandable, movePoints);
	
	_total += _count;
}

void MovesFinder::printStats()
{
	std::cerr << "findMoves: count = " << _count << "\t bestCount = " << _bestMoveCount << "\t total = " << _total;
	std::cerr << "\t turn = " << _board.turn() << "\t progress = " << (_board.white() | _board.black()).popcount();
	std::cerr << std::endl ;
}

void MovesFinder::expandMoves(uint index, const BoardMask& expandable, BoardMask& movePoints)
{
	// If all groups are expanded we are done
	if(index >= _groupExpandedSize) {
		if(movePoints.isEmpty())
			return;
		
		// Evaluate the move
		evaluateMove(movePoints);
		
		// Black special move
		if(_board.blackToMove() && !_board.hasExpanded() && _board.turn() > 10) {
			BoardMask player = _board.player() | movePoints;
			BoardMask unoccupied = ~(player | _board.opponent());
			BoardMask expandable = player.expanded() & unoccupied;
			BoardMask explorable = unoccupied - expandable;
			PointIterator exploreMoves(explorable);
			while(exploreMoves.next()) {
				BoardMask withExplore = movePoints;
				withExplore.set(exploreMoves.point());
				evaluateMove(withExplore);
			}
		}
		return;
	}
	
	// Recurse over the expansion options
	BoardMask groupExpand = _groupExpanded[index] & expandable;
	
	// If the group is not alive or already grown we continue with the next group
	if(groupExpand.isEmpty()) {
		expandMoves(index + 1, expandable, movePoints);
		return;
	}
	
	// Recurse over the expansion choice!
	/// @todo If every expansion leads to blocking of two groups, also try no groups?
	PointIterator pi(groupExpand);
	while(pi.next()) {
		BoardMask newExpandable = expandable;
		for(uint i = 0; i < _groupExpandedSize; ++i)
			if(_groupExpanded[i].isSet(pi.point()))
				newExpandable -= _groupExpanded[i];
		
		movePoints.set(pi.point());
		expandMoves(index + 1, newExpandable, movePoints);
		movePoints.clear(pi.point());
	}
}

void MovesFinder::evaluateMove(const BoardMask& movePoints)
{
	/// @todo Go over all opponents single-piece-moves and take the minimum
	
	++_count;
	
	// Resrevoir sample a random move
	if((rand() % _count) == 0) {
		_randomMove = movePoints;
	}
	
	// Find a best move
	sint32 score = estimateScore(movePoints);
	if(score > _bestScore) {
		_bestScore = score;
		_bestMove = movePoints;
		_bestMoveCount = 1;
	} else if (score == _bestScore) {
		++_bestMoveCount;
		
		// Reservoir sample the best move
		if((rand() % _bestMoveCount) == 0)
			_bestMove = movePoints;
	}
}

/// Quickly evaluate the score of a given move
sint32 MovesFinder::estimateScore(const BoardMask& movePoints)
{
	uint moveCount = movePoints.popcount();
	
	// Count the number of groups after the move
	sint32 groupCount = 0;
	if(moveCount == 1) {
		groupCount = _playerDeadGroupCount + _groupExpandedSize;
		
		// Test if the move is an explore move. If so, a new group is created
		if((movePoints.expanded() & _board.player()).isEmpty())
			++groupCount;
		
	} else {
		// Overlapping bridges never occur in simulation, so we just ignore them
		groupCount = _playerDeadGroupCount + moveCount - movePoints.countBridges();
	}
	
	BoardMask player = _board.player() | movePoints;
	
	sint32 score = _heuristic.evaluate(player, _board.opponent(), groupCount, _opponentGroupCount);
	return score;
}
/*


*/

#ifdef LOCAL

//
// T R A I N
//

class Train {
public:
	Train(const ScoreHeuristic& white, const ScoreHeuristic& black) ssefunc;
	~Train() { }
	
	void play() ssefunc;
	
	sint32 score() { return _board.score(); }
	
protected:
	Board _board;
	const ScoreHeuristic& _whiteHeuristic;
	const ScoreHeuristic& _blackHeuristic;
};

Train::Train(const ScoreHeuristic& white, const ScoreHeuristic& black)
: _board()
, _whiteHeuristic(white)
, _blackHeuristic(black)
{
}

void Train::play()
{
	uint totalWhite = 0;
	uint totalBlack =  0;
	while(!_board.gameOver()) {
		const ScoreHeuristic& heuristic = _board.whiteToMove() ? _whiteHeuristic : _blackHeuristic;
		// std::cerr << (_board.whiteToMove() ? "white " : "black ");
		BoardMask bestMove;
		if(_board.whiteToMove()) {
			MovesFinder::total(totalWhite);
			bestMove = MovesFinder::bestOrGreedy(_board, heuristic);
			totalWhite = MovesFinder::total();
		} else {
			MovesFinder::total(totalBlack);
			bestMove = MovesFinder::bestOrGreedy(_board, heuristic);
			totalBlack = MovesFinder::total();
		}
		_board.playTurn(bestMove);
		// std::cerr << _board << std::endl;
	}
}

//
// Benchmark
//

class Benchmark {
public:
	Benchmark(const ScoreHeuristic& a, const ScoreHeuristic& b);
	
	void measure();
	
	bool cutoff() const { return _trials >= _cutoff; }
	bool firstWon() const { return !cutoff() && (mean() > 0.0); }
	bool secondWon() const { return !cutoff() && (mean() < 0.0); }
	
	double mean() const { return _sum / double(_trials); }
	double variance() const { return (_squared / double(_trials)) - mean() * mean(); }
	double stddev() const { return sqrt(variance()); }
	
	void round();
	
protected:
	static const uint32 _cutoff = 100;
	const ScoreHeuristic& _heuristicA;
	const ScoreHeuristic& _heuristicB;
	uint32 _trials;
	double _sum;
	double _squared;
};

Benchmark::Benchmark(const ScoreHeuristic& a, const ScoreHeuristic& b)
: _heuristicA(a)
, _heuristicB(b)
, _trials(0)
, _sum(0.0)
, _squared(0.0)
{
}

void Benchmark::measure()
{
	// We statistically test whether E(score()) < 0 or E(score()) > 0 using the central limit theorem
	std::cerr << "Benchmarking: " << std::flush;
	
	// Kick of with a few rounds
	while(_trials < 15)
		round();
	
	for(;;) {
		double s2 = (double(_trials) / double(_trials - 1)) * variance();
		const double z = 1.96;
		double muMin = mean() - z * sqrt(s2 / _trials);
		double muMax = mean() + z * sqrt(s2 / _trials);
		
		// Stop if we have found significant results
		if(muMin > 0.0 || muMax < 0.0 || _trials >= _cutoff)
			break;
		
		// Otherwise do another round
		round();
	}
	
	if(firstWon())
		std::cerr << " first won" << std::endl;
	if(secondWon())
		std::cerr << " second won" << std::endl;
	if(cutoff())
		std::cerr << " cutoff reached" << std::endl;
}

void Benchmark::round()
{
	std::cerr << "." << std::flush;
	// Play two games, A vs B and B vs A
	// return the sum of the final scores
	sint32 score = 0;
	Train AvsB(_heuristicA, _heuristicB);
	AvsB.play();
	score += AvsB.score();
	Train BvsA(_heuristicB, _heuristicA);
	BvsA.play();
	score -= BvsA.score();
	
	// Combine into the average
	++_trials;
	_sum += score;
	_squared += score * score;
}

//
//  E V O L V E
//

void evolve(const ScoreHeuristic& heuristic)
{
	/// @todo Change to a system where we benchmark the total points collected against a reference group of opponents
	
	ScoreHeuristic def = heuristic;
	ScoreHeuristic best = heuristic;
	const int initialMutationRate = 100;
	const int rounds = 1500;
	for(int i = 0; i < rounds; ++i) {
		
		int sievert = initialMutationRate - ((initialMutationRate * i) / rounds);
		
		std::cerr << "Testing mutant " << i << " irradiated with " << sievert << " sievert" << std::endl;
		
		// Create a mutant form
		ScoreHeuristic mutant = best;
		mutant.irradiate(sievert);
		
		// The mutant must win from the current best
		Benchmark bm(mutant, best);
		bm.measure();
		if(!bm.firstWon())
			continue;
		
		// The mutant must win from the defaults
		Benchmark bmdef(mutant, def);
		bmdef.measure();
		if(!bmdef.firstWon())
			continue;
		
		// Keep the mutant if it wins both from the default and current best
		std::cerr << i << "\t" << bmdef.mean() << "\t" << bm.mean() << "\t" << mutant << std::endl;
		std::cerr << std::endl;
		best = mutant;
	}
}
#endif

//
//  I N T E R A C T I V E   G A M E
//

class InteractiveGame {
public:
	InteractiveGame(const ScoreHeuristic& heuristic);
	~InteractiveGame() ssefunc { }
	
	void play();
	
protected:
	const ScoreHeuristic& _heuristic;
	Board _board;
	std::string bestMove();
	void bestMoveImpl(char* buffer) ssefunc;
	BoardMask bestMoveImpl() ssefunc;
	void playMoves(const std::string& str);
	void playMovesImpl(const char* str) ssefunc;
	
};

InteractiveGame::InteractiveGame(const ScoreHeuristic& heuristic)
: _heuristic(heuristic)
, _board()
{
}

void InteractiveGame::play()
{
	std::string line;
	std::cin >> line;
	std::cerr << "In: " << line << std::endl;
	if(line != "Start")
		playMoves(line);
	for(;;) {
		// std::cerr << _board << std::endl;
		std::string move = bestMove();
		std::cerr << "Out: " << move << std::endl;
		std::cout << move << std::endl;
		if(_board.gameOver())
			break;
		std::cin >> line;
		std::cerr << "In: " << line << std::endl;
		playMoves(line);
		// std::cerr << _board << std::endl;
		if(_board.gameOver())
			break;
	}
	std::cin >> line;
	std::cerr << "In: " << line << std::endl;
	std::cerr << "Quiting" << line << std::endl;
}

std::string InteractiveGame::bestMove()
{
	char buffer[5000];
	bestMoveImpl(buffer);
	return std::string(buffer);
}

void InteractiveGame::bestMoveImpl(char* buffer)
{
	BoardMask move = bestMoveImpl();
	
	// Sort explore moves before expand moves
	BoardMask unoccupied = ~(_board.player() | _board.opponent());
	BoardMask expandable = _board.player().expanded() & unoccupied;
	BoardMask explorable = unoccupied - expandable;
	
	// First expand
	bool empty = true;
	PointIterator pi(move & expandable);
	while(pi.next()) {
		uint ver = pi.point().vertical() + 1;
		if(!empty)
			*buffer++ = '-';
		*buffer++ = 'A' + pi.point().horizontal();
		if(ver >= 10)
			*buffer++ = '0' + ver / 10;
		*buffer++ = '0' + ver % 10;
		empty = false;
	}
	
	// Then explore
	PointIterator pi2(move & explorable);
	while(pi2.next()) {
		uint ver = pi2.point().vertical() + 1;
		if(!empty)
			*buffer++ = '-';
		*buffer++ = 'A' + pi2.point().horizontal();
		if(ver >= 10)
			*buffer++ = '0' + ver / 10;
		*buffer++ = '0' + ver % 10;
		empty = false;
	}
	
	// Then close
	*buffer++ = 0;
	
	// Apply the move locally
	_board.playTurn(move);
}

BoardMask InteractiveGame::bestMoveImpl()
{
	return MovesFinder::bestOrGreedy(_board, _heuristic);
}

void InteractiveGame::playMoves(const std::string& str)
{
	playMovesImpl(str.c_str());
}

void InteractiveGame::playMovesImpl(const char* str)
{
	BoardMask move;
	while(*str) {
		uint hor = *str - 'A';
		++str;
		uint ver = *str - '0';
		++str;
		if(*str >= '0' & *str <= '9') {
			ver *= 10;
			ver += *str - '0';
			++str;
		}
		if(*str == '-')
			++str;
		
		BoardPoint p(hor, ver - 1);
		move.set(p);
	}
	_board.playTurn(move);
}

//
//  M A I N
//

int main(int argc, char* argv[]);
int main(int argc, char* argv[])
{
	srand(time(0));
	#ifdef LOCAL
		srand(rand() ^ getpid());
	#endif
	std::cerr << "R " << argv[0]  << std::endl;
	std::cerr << "sizeof(int): " << sizeof(int) << std::endl;
	std::cerr << "sizeof(void*): " << sizeof(void*) << std::endl;
	std::cerr << "sizeof(uint64): " << sizeof(uint64_t) << std::endl;
	std::cerr << "sizeof(m128): " << sizeof(m128) << std::endl;
	
	ScoreHeuristic heuristic(10197, -2277, 6, 147, 215, 130, 66, 29, 13); // E8
	std::cerr << heuristic << std::endl;
	// evolve(heuristic);
	InteractiveGame g(heuristic);
	g.play();
	
	std::cerr << "Exit" << std::endl;
	return 0;
}

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
	int popcount() const ssefunc;
	BoardMask& clear() ssefunc;
	BoardMask& set(const BoardPoint& point) ssefunc;
	BoardMask& clear(const BoardPoint& point) ssefunc;
	bool isSet(const BoardPoint& point) const ssefunc;
	bool isEmpty() const ssefunc;
	BoardPoint firstPoint() const ssefunc;
	
	std::string toMoves();
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

int BoardMask::popcount() const
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

std::string BoardMask::toMoves()
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

enum Colour {
	White,
	Black
};

class Board {
public:
	Board() ssefunc;
	Board(const Board& board) ssefunc;
	~Board() ssefunc { }
	
	const BoardMask& white() const ssefunc { return _white; }
	Board& white(const BoardMask& value) ssefunc { _white = value; return *this; }
	const BoardMask& black() const ssefunc { return _black; }
	Board& black(const BoardMask& value) ssefunc { _black = value; return *this; }
	uint64 hash() const { return _hash; }
	bool hasExpanded() const { return _hasExpanded; }
	void recalculateHash() ssefunc;
	
	bool gameOver() const ssefunc { return (~(_white | _black)).isEmpty(); }
	sint32 score();
	
	void whiteMove(const BoardPoint& move) ssefunc;
	void blackMove(const BoardPoint& move) ssefunc;
	
	void whiteMove(const BoardMask& move) ssefunc;
	void blackMove(const BoardMask& move) ssefunc;
	
protected:
	friend std::ostream& operator<<(std::ostream& out, const Board& board);
	const static uint64 zobristWhite[15*15];
	const static uint64 zobristBlack[15*15];
	BoardMask _white;
	BoardMask _black;
	bool _hasExpanded;
	uint32 _turn;
	uint64 _hash;
};

std::ostream& operator<<(std::ostream& out, const Board& board) ssefunc;

Board::Board()
: _white()
, _black()
, _hasExpanded(false)
, _hash(0)
{
}

Board::Board(const Board& board)
: _white(board._white)
, _black(board._black)
, _hasExpanded(board._hasExpanded)
, _hash(board._hash)
{
}

void Board::whiteMove(const BoardPoint& move)
{
	_hasExpanded |= _white.expanded().isSet(move);
	_white.set(move);
	_hash ^= zobristWhite[(move.vertical() * 15) + move.horizontal()];
}

void Board::blackMove(const BoardPoint& move)
{
	_hasExpanded |= _black.expanded().isSet(move);
	_black.set(move);
	_hash ^= zobristBlack[(move.vertical() * 15) + move.horizontal()];
}

void Board::whiteMove(const BoardMask& move)
{
	_hasExpanded |= !((_white.expanded() | move).isEmpty());
	_white |= move;
	PointIterator pi(move);
	while(pi.next())
		_hash ^= zobristWhite[(pi.point().vertical() * 15) + pi.point().horizontal()];
}

void Board::blackMove(const BoardMask& move)
{
	_hasExpanded |= !((_white.expanded() | move).isEmpty());
	_black |= move;
	PointIterator pi(move);
	while(pi.next())
		_hash ^= zobristBlack[(pi.point().vertical() * 15) + pi.point().horizontal()];
}


void Board::recalculateHash()
{
	_hash = 0;
	for(int i = 0; i < 15; ++i)
	for(int j = 0; j < 15; ++j) {
		BoardPoint p(i, j);
		if(_white.isSet(p))
			_hash ^= zobristWhite[p.number()];
		if(_black.isSet(p))
			_hash ^= zobristWhite[p.number()];
	}
}

void printBoard(std::ostream& out, uint16* white, uint16* black, uint64 hash)
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
	
	out << "hash: ";
	out.width(16);
	out.fill('0');
	out << std::hex << hash << std::dec << std::endl;
	out.fill(' ');
}

sint32 Board::score()
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

inline std::ostream& operator<<(std::ostream& out, const Board& board)
{
	uint16 white[16] __attribute__ ((aligned (16)));
	uint16 black[16] __attribute__ ((aligned (16)));
	_mm_store_si128((m128*)(white), board._white.bits[0]);
	_mm_store_si128((m128*)(white + 8), board._white.bits[1]);
	_mm_store_si128((m128*)(black), board._black.bits[0]);
	_mm_store_si128((m128*)(black + 8), board._black.bits[1]);
	printBoard(out, white, black, board.hash());
	return out;
}

//
//   T U R N   I T E R A T O R
//

class TurnIterator
{
public:
	TurnIterator(const BoardMask& player, const BoardMask& opponent) ssefunc;
	~TurnIterator() ssefunc { }
	
	bool done() const ssefunc { return _moves.isEmpty(); }
	BoardMask moves() const ssefunc { return _moves; }
	void choose(const BoardPoint& point) ssefunc;
	
	std::vector<BoardMask> validMoves();
	
protected:
	const BoardMask& _player;
	const BoardMask& _opponent;
	BoardMask _turnMoves;
	BoardMask _moves;
};

TurnIterator::TurnIterator(const BoardMask& player, const BoardMask& opponent)
: _player(player)
, _opponent(opponent)
, _turnMoves()
, _moves(~(_player | _opponent)) // All empty spaces
{
}

void TurnIterator::choose(const BoardPoint& point)
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
	
	// After all expansion moves are done
	// and no expansion moves happened before
	// black has the option of an additional exploration move
	/// TODO
	
	/// @todo From the fifth group (or fifth move) black can deploy its move
}

//
//   M O V E S   F I N D E R
//

class MovesFinder
{
public:
	MovesFinder(const Board& board);
	
	void findWhiteMoves();
	void findBlackMoves();
	
	std::vector<BoardMask> validMoves() const { return _validMoves; }
	
	uint32 count() const { return _validMoves.size(); }
	uint32 countUnique() const;
	
protected:
	const Board& _board;
	BoardMask _player;
	std::vector<BoardMask> _validMoves;
	
	void findExploreMoves(const BoardMask& player, const BoardMask& opponent);
	void findExpandMoves(const BoardMask& player, const BoardMask& opponent);
	void expandMoves(const BoardMask& expandable, GroupIterator gi, const BoardMask& movePoints);
};

MovesFinder::MovesFinder(const Board& board)
: _board(board)
{
}

void MovesFinder::findWhiteMoves()
{
	_player = _board.white();
	findExploreMoves(_board.white(), _board.black());
	findExpandMoves(_board.white(), _board.black());
}

void MovesFinder::findBlackMoves()
{
	_player = _board.black();
	findExploreMoves(_board.black(), _board.white());
	findExpandMoves(_board.black(), _board.white());
	if(!_board.hasExpanded()) {
		/// @todo If the turn number is ≥ 5 include the black expand-explore move
	}
}

void MovesFinder::findExploreMoves(const BoardMask& player, const BoardMask& opponent)
{
	// Do some board algebra
	BoardMask unoccupied = ~(player | opponent);
	BoardMask expandable = player.expanded() & unoccupied;
	BoardMask explorable = unoccupied - expandable;
	
	// Add the explore moves
	PointIterator exploreMoves(explorable);
	while(exploreMoves.next()) {
		BoardMask exploreMove;
		exploreMove.set(exploreMoves.point());
		_validMoves.push_back(exploreMove);
	}
}

void MovesFinder::findExpandMoves(const BoardMask& player, const BoardMask& opponent)
{
	// Do some board algebra
	BoardMask unoccupied = ~(player | opponent);
	BoardMask expandable = player.expanded() & unoccupied;
	BoardMask explorable = unoccupied - expandable;
	
	// Add any expand moves, but be careful not to duplicate!
	/// @todo The order of expanding the groups can be relevant!
	GroupIterator gi(player);
	BoardMask movePoints;
	expandMoves(expandable, gi, movePoints);
}

void MovesFinder::expandMoves(const BoardMask& expandable, GroupIterator gi, const BoardMask& movePoints)
{
	// If all groups are expanded we are done
	if(!gi.next()) {
		/// @todo Black additional explore move special case
		// std::cerr << "Move = " << (_player | movePoints);
		_validMoves.push_back(movePoints);
		return;
	}
	
	// Recurse over the expansion options
	const BoardMask& group = gi.group();
	BoardMask groupExpand = group.expanded() & expandable;
	
	// If the group is not alive or already grown we continue with the next group
	if(groupExpand.isEmpty()) {
		expandMoves(expandable, gi, movePoints);
		return;
	}
	
	// Recurse over the expansion choice!
	PointIterator pi(groupExpand);
	while(pi.next()) {
		BoardMask myMove = movePoints;
		myMove.set(pi.point());
		BoardMask connected = _player.connected(myMove.expanded());
		BoardMask newExpandable = expandable;
		newExpandable -= connected.expand();
		expandMoves(newExpandable, gi, myMove);
	}
}

uint32 MovesFinder::countUnique() const
{
	return _validMoves.size();
	std::vector<BoardMask> unique;
	for(uint32 i = 0; i < _validMoves.size(); ++i) {
		bool newMove = true;
		for(uint32 j = 0; j < unique.size(); ++j) {
			if(unique[j] == _validMoves[i]) {
				newMove = false;
				break;
			}
		}
		if(newMove)
			unique.push_back(_validMoves[i]);
	}
	return unique.size();
}


//
//  T A B L E   E N T R Y
//

class TableEntry
{
public:
	enum Kind {
		notFound = 0,
		lowerBound = 1,
		upperBound = 2,
		exact = 3
	};
	
	TableEntry();
	TableEntry(uint64 hash, sint32 score, Kind kind);
	TableEntry(const Board& board, sint32 score, Kind kind);
	~TableEntry();
	
	uint64 hash() const { return _hash; }
	sint32 score() const { return _score; }
	TableEntry& score(sint32 value) { _score = value; return *this; }
	Kind kind() const { return _kind; } 
	TableEntry& kind(Kind value) { _kind = value; return *this; }
	int depth() const { return _depth; }
	TableEntry&  depth(int value) { _depth = value; return *this; }
	int bestMove() const { return _bestMove; }
	TableEntry& bestMove(int value) { _bestMove = value; return *this; }
	
protected:
	/// TODO:
	/// - uint32 kind + hash-key
	uint64 _hash;
	sint32 _score;
	Kind _kind;
	int _depth;
	int _bestMove;
};

TableEntry::TableEntry()
: _hash(0)
, _score(0)
, _kind(notFound)
{
}

TableEntry::TableEntry(uint64 hash, sint32 score, TableEntry::Kind kind)
: _hash(hash)
, _score(score)
, _kind(kind)
{
}

TableEntry::TableEntry(const Board& board, sint32 score, TableEntry::Kind kind)
: _hash(board.hash())
, _score(score)
, _kind(kind)
{
}


TableEntry::~TableEntry()
{
}


//
//   T A B L E
//

class Table
{
public:
	Table();
	~Table();
	
	static int size() { return _size; }
	
	TableEntry get(uint64 hash);
	void put(const TableEntry& entry);
	
protected:
	friend std::ostream& operator<<(std::ostream& out, const Table& table);
	static int _size;
	int _hits;
	int _misses;
	int _collisions;
	TableEntry* _table;
	uint64 nextHash(uint64 hash);
};

int Table::_size = (60 << 20) / sizeof(TableEntry);

Table table;

Table::Table()
: _hits(0)
, _misses(0)
, _collisions(0)
{
	if(_size % 2 == 0)
		++_size;
	_table = new TableEntry[_size];
}

Table::~Table()
{
	delete[] _table;
}

TableEntry Table::get(uint64 hash)
{
	TableEntry result;
	uint64 targetHash = hash;
	for(int i = 0; i < 20; ++i) {
		result = _table[hash % _size];
		if(result.hash() == targetHash) {
			++_hits;
			return result;
		}
		if (result.kind() == TableEntry::notFound) {
			++_misses;
			return TableEntry();
		}
		hash = nextHash(hash);
	}
	++_collisions;
	return TableEntry();
}

void Table::put(const TableEntry& entry)
{
	uint64 hash = entry.hash();
	TableEntry result;
	for(int i = 0; i < 20; ++i) {
		result = _table[hash % _size];
		if(result.hash() == entry.hash() || result.kind() == TableEntry::notFound) {
			_table[hash % _size] = entry;
			return;
		}
		hash = nextHash(hash);
	}
	
	// Evict a random entry
	hash = entry.hash();
	int r = rand() % 20;
	while(r--)
		hash = nextHash(hash);
	_table[hash % _size] = entry;
}

uint64 Table::nextHash(uint64 hash)
{
	const uint64 k64 = 0x436174bab1d5558dULL;
	hash *= k64;
	hash = __builtin_bswap64(hash);
	return hash;
}

std::ostream& operator<<(std::ostream& out, const Table& table)
{
	out << "Table size:       " << table._size << " (" << (table._size * sizeof(TableEntry)) << " bytes)" << std::endl;
	out << "Total queries:    " << table._hits  + table._misses + table._collisions << std::endl;
	out << "Table hits:       " << table._hits << std::endl;
	out << "Table misses:     " << table._misses << std::endl;
	out << "Table collisions: " << table._collisions << std::endl;
	return out;
}

//
//  S C O R E   H E U R I S T I C
//

class ScoreHeuristic {
public:
	ScoreHeuristic();
	ScoreHeuristic(sint32, sint32, sint32, sint32, sint32, sint32, sint32, sint32, sint32, sint32);
	~ScoreHeuristic() { }
	static sint32 mix(sint32 a, sint32 b, sint32 left, sint32 right, sint32 pos);
	
	// Game scores, multiplied by 1000
	static const sint32 piecePoints = 1000;
	static const sint32 groupPoints = -6000;
	
	// Heuristics
	sint32 earlyGroupPoints;
	sint32 earlyManyGroupPoints;
	sint32 manyTransitionBegin;
	sint32 manyTransitionEnd;
	sint32 earlyTransistionBegin;
	sint32 earlyTransistionEnd;
	sint32 firstFreedomPoints;
	sint32 secondFreedomPoints;
	sint32 thirdFreedomPoints;
	sint32 fourthFreedomPoints;
	
	void irradiate(sint32 sievert);
	
	sint32 evaluate(const Board& board) const ssefunc;
	
protected:
	void irradiate(sint32* parameter, sint32 sievert);
};

// ScoreHeuristic heuristic(3000, -6000, 7, 13, 180, 220, 187, 87, 37, 12);

// ScoreHeuristic heuristic(2370, -5410, 5, 5, 167, 177, 148, 82, 33, -1); // E1
// ScoreHeuristic heuristic(2271, -6105, 5, 5, 156, 215, 158, 78, 34, 5); // E2
// ScoreHeuristic heuristic(7864, -2363, 6, 8, 155, 180, 160, 70, 45, 12); // E3 <-- Very good!
// ScoreHeuristic heuristic(1256, -823, 5, 5, 144, 201, 115, 53, 43, 2); // E4
// ScoreHeuristic heuristic(341, -4576, 5, 5, 139, 191, 103, 57, 22, 12); // E5
// ScoreHeuristic heuristic(4710, -17597, 6, 6, 167, 172, 98, 35, 29, -10); // E6
// ScoreHeuristic heuristic(1716, -4330, 5, 5, 145, 209, 90, 38, 31, 3); // E7

// New re-runs on E3
// ScoreHeuristic heuristic(10197, -2277, 6, 6, 147, 215, 130, 66, 29, 13); // E8 <-- Slightly better!
// ScoreHeuristic heuristic(7127, -2330, 6, 6, 151, 188, 147, 65, 43, 11); // E9
// ScoreHeuristic heuristic(6526, -1872, 6, 8, 172, 172, 156, 75, 42, 13); // E10
// ScoreHeuristic heuristic(5813, -3234, 6, 6, 143, 154, 109, 61, 31, 11); // E11

// Evolution of E8
// ScoreHeuristic heuristic(10738, -2378, 6, 6, 164, 213, 122, 59, 23, 8); // E12
// ScoreHeuristic heuristic(13007, -2269, 6, 6, 153, 198, 129, 61, 27, 10); // E13
// ScoreHeuristic heuristic(9970, -2214, 5, 5, 139, 219, 114, 58, 30, 12); // E14
// ScoreHeuristic heuristic(11030, -2112, 6, 6, 144, 215, 121, 64, 25, 10); // E15

ScoreHeuristic::ScoreHeuristic()
: earlyGroupPoints(10197)
, earlyManyGroupPoints(-2277)
, manyTransitionBegin(6)
, manyTransitionEnd(6)
, earlyTransistionBegin(147)
, earlyTransistionEnd(215)
, firstFreedomPoints(130)
, secondFreedomPoints(66)
, thirdFreedomPoints(29)
, fourthFreedomPoints(13)
{
}

ScoreHeuristic::ScoreHeuristic(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f, sint32 g, sint32 h, sint32 i, sint32 j)
: earlyGroupPoints(a)
, earlyManyGroupPoints(b)
, manyTransitionBegin(c)
, manyTransitionEnd(d)
, earlyTransistionBegin(e)
, earlyTransistionEnd(f)
, firstFreedomPoints(g)
, secondFreedomPoints(h)
, thirdFreedomPoints(i)
, fourthFreedomPoints(j)
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
	irradiate(&manyTransitionBegin, sievert);
	irradiate(&manyTransitionEnd, sievert);
	irradiate(&earlyTransistionBegin, sievert);
	irradiate(&earlyTransistionEnd, sievert);
	irradiate(&firstFreedomPoints, sievert);
	irradiate(&secondFreedomPoints, sievert);
	irradiate(&thirdFreedomPoints, sievert);
	irradiate(&fourthFreedomPoints, sievert);
	
	// Sanetize
	if(manyTransitionEnd > 40)
		manyTransitionEnd = 40;
	if(manyTransitionBegin < 0)
		manyTransitionBegin = 0;
	if(earlyTransistionBegin < 0)
		earlyTransistionBegin = 0;
	if(earlyTransistionEnd > 225)
		earlyTransistionEnd = 225;
	if(manyTransitionBegin > manyTransitionEnd)
		manyTransitionEnd = manyTransitionBegin;
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

sint32 ScoreHeuristic::evaluate(const Board& board) const
{
	/*
	TableEntry te = table.get(board.hash());
	if(te.kind() != TableEntry::notFound) {
		return te.score();
	}
	*/
	
	sint32 score = 0;
	sint32 whitePieces = board.white().popcount();
	sint32 blackPieces = board.black().popcount();
	sint32 progress = whitePieces + blackPieces;
	
	// The score of each player is determined by counting the number of stones he has placed on the board. 
	score += piecePoints * (whitePieces - blackPieces);
	
	// For each separate group 6 points will be subtracted.
	int whiteGroups = GroupIterator::count(board.white());
	int blackGroups = GroupIterator::count(board.black());
	sint32 lateGroupScore = groupPoints * (whiteGroups - blackGroups);
	sint32 earlyGroupScore = 0;
	for(int i = 0; i < whiteGroups; ++i)
		earlyGroupScore += mix(earlyGroupPoints, earlyManyGroupPoints, manyTransitionBegin, manyTransitionEnd, i);
	for(int i = 0; i < blackGroups; ++i)
		earlyGroupScore -= mix(earlyGroupPoints, earlyManyGroupPoints, manyTransitionBegin, manyTransitionEnd, i);
	score += mix(earlyGroupScore, lateGroupScore, earlyTransistionBegin, earlyTransistionEnd, progress);
	
	// Add points for expansion room
	BoardMask unoccupied = ~(board.white() | board.black());
	BoardMask whiteExpanded = board.white().expanded() & unoccupied;
	BoardMask blackExpanded = board.black().expanded() & unoccupied;
	score += (firstFreedomPoints - secondFreedomPoints - thirdFreedomPoints - fourthFreedomPoints) * (whiteExpanded.popcount() - blackExpanded.popcount());
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += (secondFreedomPoints - thirdFreedomPoints - fourthFreedomPoints) * (whiteExpanded.popcount() - blackExpanded.popcount());
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += (thirdFreedomPoints - fourthFreedomPoints) * (whiteExpanded.popcount() - blackExpanded.popcount());
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += fourthFreedomPoints * (whiteExpanded.popcount() - blackExpanded.popcount());
	
	// The player with the highest score gets 100 bonus points.
	//if(progress == 225)
	//	score += 100000 * sgn(score);
	
	// Cache
	// table.put(TableEntry(board, score, TableEntry::exact));
	
	return score;
}

std::ostream& operator<<(std::ostream& out, const ScoreHeuristic& heuristic)
{
	out << "Heuristic(";
	out << heuristic.earlyGroupPoints << ", ";
	out << heuristic.earlyManyGroupPoints << ", ";
	out << heuristic.manyTransitionBegin << ", ";
	out << heuristic.manyTransitionEnd << ", ";
	out << heuristic.earlyTransistionBegin << ", ";
	out << heuristic.earlyTransistionEnd << ", ";
	out << heuristic.firstFreedomPoints << ", ";
	out << heuristic.secondFreedomPoints << ", ";
	out << heuristic.thirdFreedomPoints << ", ";
	out << heuristic.fourthFreedomPoints << ")";
	return out;
}

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
	bool whitesTurn = true;
	while(!_board.gameOver()) {
		
		MovesFinder mf(_board);
		if(whitesTurn) {
			mf.findWhiteMoves();
		} else {
			mf.findBlackMoves();
		}
		// std::cerr << _board << std::endl;
		std::cerr << "Move finder found " << mf.count() << "\t" << mf.countUnique() << std::endl;
		
		TurnIterator ti(
			(whitesTurn) ? _board.white() : _board.black(),
			(whitesTurn) ? _board.black() : _board.white());
		std::vector<BoardPoint> moves;
		while(!ti.done()) {
			BoardMask validMoves = ti.moves();
			BoardMask goodMoves = validMoves;
			sint32 goodScore = -0x7FFFFFFF;
			PointIterator pi(validMoves);
			while(pi.next()) {
				BoardPoint move = pi.point();
				Board afterMove = _board;
				if(whitesTurn)
					afterMove.whiteMove(move);
				else
					afterMove.blackMove(move);
				
				sint32 moveScore = 0;
				if(whitesTurn)
					moveScore = _whiteHeuristic.evaluate(afterMove);
				else
					moveScore = _blackHeuristic.evaluate(afterMove);
				if(!whitesTurn)
					moveScore = -moveScore;
				
				if(moveScore > goodScore) {
					goodScore = moveScore;
					goodMoves = BoardMask();
					goodMoves.set(move);
				} else if (moveScore == goodScore) {
					goodMoves.set(move);
				}
			}
			// std::cerr << goodMoves << std::endl;
			BoardPoint move = PointIterator::randomPoint(goodMoves);
			ti.choose(move);
			moves.push_back(move);
		}
		for(std::size_t i = 0; i < moves.size(); ++i) {
			if(whitesTurn)
				_board.whiteMove(moves[i]);
			else
				_board.blackMove(moves[i]);
		}
		whitesTurn = !whitesTurn;
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
	bool secondWon() const { return !cutoff() && (mean() > 0.0); }
	
	double mean() const { return _sum / double(_trials); }
	double variance() const { return (_squared / double(_trials)) - mean() * mean(); }
	double stddev() const { return sqrt(variance()); }
	
	void round();
	
protected:
	static const uint32 _cutoff = 1000;
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
	
	// Kick of with a few rounds
	while(_trials < 10)
		round();
	
	for(;;) {
		double s2 = (double(_trials) / double(_trials - 1)) * variance();
		const double z = 1.96;
		double muMin = mean() - z * sqrt(s2 / _trials);
		double muMax = mean() + z * sqrt(s2 / _trials);
		
		// Stop if we have found significant results
		if(muMin > 0.0)
			return;
		if(muMax < 0.0)
			return;
		if(_trials >= _cutoff) {
			std::cerr << ":" << std::flush;
			return;
		}
		
		round();
	}
}

void Benchmark::round()
{
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
//  G A M E
//

class Game {
public:
	Game(const ScoreHeuristic& heuristic) ssefunc;
	~Game() ssefunc { }
	
	void play() ssefunc;
	
protected:
	const ScoreHeuristic& _heuristic;
	Board _board;
	bool _isWhite;
	bool _isBlack;
	BoardMask bestMove();
	void receiveMoves(const std::string& moves);
};

Game::Game(const ScoreHeuristic& heuristic)
: _heuristic(heuristic)
, _board()
, _isWhite(false)
, _isBlack(false)
{
}

void Game::play()
{
	std::string line;
	std::cin >> line;
	std::cerr << "In: " << line << std::endl;
	if(line == "Start") {
		_isWhite = true;
		_isBlack = false;
	} else {
		_isWhite = false;
		_isBlack = true;
		receiveMoves(line);
	}
	for(;;) {
		BoardMask bestMoveMask = bestMove();
		std::cerr << _board << std::endl;
		std::cerr << "Out: " << bestMoveMask.toMoves() << std::endl;
		std::cout << bestMoveMask.toMoves() << std::endl;
		if(_isWhite)
			_board.whiteMove(bestMoveMask);
		else
			_board.blackMove(bestMoveMask);
		if(_board.gameOver())
			break;
		std::cin >> line;
		std::cerr << "In: " << line << std::endl;
		receiveMoves(line);
		std::cerr << _board << std::endl;
		if(_board.gameOver())
			break;
	}
	std::cin >> line;
	std::cerr << "In: " << line << std::endl;
	std::cerr << "Quiting" << line << std::endl;
	return;
}

BoardMask Game::bestMove()
{
	TurnIterator ti(
		(_isWhite) ? _board.white() : _board.black(),
		(_isWhite) ? _board.black() : _board.white());
	BoardMask moves;
	while(!ti.done()) {
		BoardMask validMoves = ti.moves();
		BoardMask goodMoves = validMoves;
		sint32 goodScore = -0x7FFFFFFF;
		PointIterator pi(validMoves);
		while(pi.next()) {
			BoardPoint move = pi.point();
			Board afterMove = _board;
			if(_isWhite)
				afterMove.whiteMove(move);
			else
				afterMove.blackMove(move);
			sint32 moveScore = _heuristic.evaluate(afterMove);
			if(_isBlack)
				moveScore = -moveScore;
			if(moveScore > goodScore) {
				goodScore = moveScore;
				goodMoves = BoardMask();
				goodMoves.set(move);
			} else if (moveScore == goodScore) {
				goodMoves.set(move);
			}
		}
		std::cerr << "Good score: " << goodScore << std::endl;
		BoardPoint move = PointIterator::randomPoint(goodMoves);
		ti.choose(move);
		moves.set(move);
	}
	return moves;
}

void Game::receiveMoves(const std::string& moves)
{
	BoardMask moveMask = BoardMask::fromMoves(moves);
	if(_isWhite)
		_board.blackMove(moveMask);
	else
		_board.whiteMove(moveMask);
}

//
//  E V O L V E
//

void evolve(const ScoreHeuristic& heuristic)
{
	ScoreHeuristic def = heuristic;
	ScoreHeuristic best = heuristic;
	const int initialMutationRate = 100;
	const int rounds = 1500;
	for(int i = 0; i < rounds; ++i) {
		
		int sievert = initialMutationRate - ((initialMutationRate * i) / rounds);
		
		std::cerr << "." << std::flush;
		if(i % 10 == 0) {
			std::cerr << " " << i << " " << sievert << std::endl;
		}
		
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
		std::cerr << std::endl;
		std::cerr << i << "\t" << bmdef.mean() << "\t" << bm.mean() << "\t" << mutant << std::endl;
		best = mutant;
	}
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
	std::cerr << table << std::endl;
	
	ScoreHeuristic heuristic(10197, -2277, 6, 6, 147, 215, 130, 66, 29, 13); // E8
	std::cerr << heuristic << std::endl;
	//evolve(heuristic);
	//return 0;
	
	Board b;
	b.whiteMove(BoardPoint(2, 1));
	b.whiteMove(BoardPoint(1, 2));
	b.whiteMove(BoardPoint(3, 2));
	b.whiteMove(BoardPoint(2, 3));
	std::cerr << b << std::endl;
	
	MovesFinder mf(b);
	mf.findWhiteMoves();
	std::cerr << mf.count() << std::endl;
	
	// return 0;
	
	//Train t(heuristic, heuristic);
	//t.play();
	//return 0;
	
	Game g(heuristic);
	g.play();
	
	std::cerr << table << std::endl;
	std::cerr << "Exit" << std::endl;
	return 0;
}

const uint64 Board::zobristWhite[15*15] = {
	0x59ff30cdaeb11f30ULL, 0xcbea4e0e315353c7ULL, 0x36ddafebe344b37fULL, 0x21ac7d5399ea0d14ULL, 0xf08b8086b369794dULL, 0x121694bf54a41b46ULL, 0xf8423970f5f7387fULL, 0x34535bea4b79a822ULL, 0x4119d278d6bdc7b4ULL, 0x972a951be6ed352fULL, 0xba596f4757b00c93ULL, 0x97f51be078b75a3bULL, 0xeaab071cbdab19ddULL, 0xa4eaa35fac046f1aULL, 0x914afed077e93704ULL, 0x9ff7bf8cd00bacacULL, 0x5bb2530a10d3ffb6ULL, 0xa99baf96a648ae4dULL, 0x6235df8ba1cd778aULL, 0x481a6794934883a4ULL, 0xf42aa308cbb358b1ULL, 0xcfb28dbb428ee1acULL, 0x3ff2a201668413b5ULL, 0x60515bbc22abc047ULL, 0xad104bef8b61e30eULL, 0xd7c13324fbfb676cULL, 0xeef173e9cb9c3b75ULL, 0x8c9a7010d13f543fULL, 0xa956fd1e701a0bc2ULL, 0x9ed090bcdfe433e8ULL, 0x83576f23dfa8c045ULL, 0xfb6ac1f38840ed37ULL, 0xbd596919635094b3ULL, 0x6efea445d7e0fe7aULL, 0x1a44301636891d99ULL, 0xe42216c728f19b40ULL, 0xc78e7e0b428aa55cULL, 0x466986d734541f81ULL, 0x3b29aac9a9449847ULL, 0x3bd344c86ef5f182ULL, 0x10f2624d046a24ddULL, 0x4a28dc8fbb176c30ULL, 0x8c2ac0258503c132ULL, 0xc4dcff8d189a0233ULL, 0x8e299ed20e39bdb7ULL, 0x8478adf3f7fb4bd8ULL, 0x591cdb02336b7b3fULL, 0x58be8a9cd6cf0588ULL, 0xf10485f134250c3eULL, 0x1ef6ca189fe6e309ULL, 0xc207244afcfa7a17ULL, 0x9fbdb0794361491cULL, 0x03dadacec98a10a1ULL, 0x51d258f836d7fc6cULL, 0xfc8c79db6f81e8a1ULL, 0x0b208128a417db22ULL, 0x737c6d3307d78f4dULL, 0xacd96a9a1843de9aULL, 0x3c3f9893c4162fc6ULL, 0xfa42dfe81c03cd5cULL, 0x89a9acfd2ba32583ULL, 0x66a092ea8bf7f671ULL, 0x6305c56ec48064ebULL, 0xcddecbd58df6ef2fULL, 0xcdef9d71c478265eULL, 0x247e8c9d44f1eaf2ULL, 0x7e8b41a65c98356fULL, 0x0ab9eb0d9b719badULL, 0x82cfbaf75db0eba2ULL, 0xfe279388dd19d200ULL, 0x103421854adb67c2ULL, 0x0d4b27b3bad83806ULL, 0x3dfe5e933572324bULL, 0xdfa672125bf58c0fULL, 0x8175db82e6fddc41ULL, 0x0e406cacaa3a89d5ULL, 0x58a3b7204d435ae1ULL, 0x18e747e52f3d2ee0ULL, 0x18106fa796424fefULL, 0xa9e0709ab6fae37eULL, 0x1b9094eb361764ecULL, 0x0d391ed73638bd43ULL, 0x91c5a0cb21818f5aULL, 0xa5b9a2e79877b43aULL, 0xe77ae74041883df0ULL, 0x252f557244216b7aULL, 0x6797395ce455ad9fULL, 0xca81efa898877896ULL, 0xdde24236523a7b4bULL, 0x72fcaba306d3fc08ULL, 0x754b3c9e473107deULL, 0x6013dca10adb34bcULL, 0x1754e3fea6e33bc9ULL, 0x587a117014d594acULL, 0xe32349ae3e241ec7ULL, 0xf8967e3b267199e7ULL, 0x0f791764d1d39f53ULL, 0x4d25e0181d0f7fd2ULL, 0x552e814b97319546ULL, 0xdeb45e42aa747fb6ULL, 0x13b29bd3c85834f0ULL, 0x3523ab3da7b15f39ULL, 0xbad831a931e812f9ULL, 0x0bda6df9711c2d64ULL, 0x4ca94e8cb4231f8eULL, 0xea0a4218411e742aULL, 0x51297fbbf74974c2ULL, 0xd1aebd43f5a07526ULL, 0xebac7170dfe9ae23ULL, 0xac7aa86392e6e4d4ULL, 0xe183ce1f7337e01dULL, 0xa0d95fec5f920161ULL, 0x49fa42b5fd3b28d5ULL, 0x7260a56d9befd2a4ULL, 0x3fbd1088b7393158ULL, 0x00421c09db4b607bULL, 0xc02ba051b92bd2a7ULL, 0x1dd7f5473606d174ULL, 0x196aac66cd51d65aULL, 0x110f5c25870fe186ULL, 0xfae679646b22c453ULL, 0x60b2cd3fbcd4170eULL, 0xf0d7815849a93726ULL, 0x2078dc850236dc17ULL, 0x99c9145240221de7ULL, 0x06d141d7ce1df036ULL, 0x3d1fc69e9dac8dc0ULL, 0xdcc1a82b1481eba2ULL, 0xb468c4d6f48dad35ULL, 0x7e291cba71203902ULL, 0x7161ced58d9833e0ULL, 0x2bf62bad86392c68ULL, 0x06e1941aa9952a6cULL, 0xdbe238c50e544f0aULL, 0x11dc2eeed0d9b4eeULL, 0x759195dd3e00060cULL, 0x373d11a02cfc49acULL, 0x1a246c1e74281604ULL, 0x3f1cf73143d85672ULL, 0x8254d9e7693fc961ULL, 0xeaac1241bee7f1c3ULL, 0x8f18c01b5bef7668ULL, 0x9c32b764637978e5ULL, 0x4ae075ba2a03a907ULL, 0x1dba954be380ffacULL, 0x93401921e51a64ffULL, 0xe02a83b6755a8cb3ULL, 0x68190601403a5e23ULL, 0x52f5a692e9506b20ULL, 0x153d8f74ff140197ULL, 0x67a1cd205c078951ULL, 0x8508b7ab10102a1aULL, 0x2d3479cae852f969ULL, 0x43e1b48c7cefb03cULL, 0x9c16d130aa00b797ULL, 0x70b57cd33c650215ULL, 0x13d834a8b07dfaf9ULL, 0x93e3f796e352cf41ULL, 0x0f4725c1e1e25df8ULL, 0x5936430c3a90a776ULL, 0x50ab26b9914fd4c1ULL, 0x5d3afc160d66e4d6ULL, 0x302f9cf18dc97e11ULL, 0x483a4e96d2afeb9aULL, 0xca59a7b0b65ef67dULL, 0x10ad170f3e35dc5dULL, 0x1354ce7806b0ea1dULL, 0x055dee169e51c15cULL, 0xfb67d2c81cc84cafULL, 0x4641918a18dccd02ULL, 0x6032cc4fc2618760ULL, 0x415b47bf01264ccfULL, 0xd12ee7388f693aa0ULL, 0x5c66152f4760be3cULL, 0x03015c6006b75691ULL, 0x14e0129c78055d4eULL, 0x16d13075d249c80bULL, 0x9d0d9ee957447478ULL, 0xef9fe65c3f75cb17ULL, 0x055ae79446825ab9ULL, 0x20febf7cdc05a830ULL, 0xcf655dcce0fd3aeaULL, 0x46deaaf4aa6dec84ULL, 0xafcdd341b5d2dc57ULL, 0xca81124b40bfe3d4ULL, 0xba246d54c0ee7c85ULL, 0x7f03ff01046179c1ULL, 0x6b53303adc1da26fULL, 0xc56fb72211c36b11ULL, 0x62e3ad9ab61120ebULL, 0xbbab7a332573279fULL, 0xca01416bd3878b0dULL, 0xa29c7f6ce25a7535ULL, 0x3105bad3e4f06780ULL, 0x822ef8a2911c85fdULL, 0xd8b2129b06bfa62cULL, 0x77cc000d8d7a847cULL, 0x188a004a2f70b9ecULL, 0x0b503bc5cf47eaaeULL, 0xd9a72cb8990d6214ULL, 0xff636ce9cddc618aULL, 0x7625d14b53fac9aeULL, 0xb176994f98af0724ULL, 0x8142e01dcc4e6788ULL, 0x521291f0f276fd7dULL, 0x8ccddd8bb22bc827ULL, 0x7829daa6283cd94eULL, 0xbbdfd927070a79e0ULL, 0x47b7e816b21a59afULL, 0xe6a906a4ccec0e9fULL, 0x1b745aa87a4cf45aULL, 0xa4455b99400add4bULL, 0xffcd8cf506ecac1eULL, 0xd32b94287e9c332fULL, 0xda559051ce24df1dULL, 0x548faeaad50f89c8ULL, 0x00066f7e64a81be3ULL, 0xcfb30b0bdc8c35eeULL, 0xa16442a3bafa207cULL, 0x06301ad8d2c46ef0ULL, 0x99bd48a7a35d1310ULL, 0x3722456e615f87c6ULL, 0xe0f75c513a6099b7ULL, 0x41f24f0543721aaaULL, 0xa9281264de1e4c82ULL};
const uint64 Board::zobristBlack[15*15] = {
	0x5f7adf4d402775eaULL, 0x54cc1a8c299bd9c6ULL, 0xc29d137b7218812dULL, 0x4678c7faa9ef2c29ULL, 0x120fde7ca0e99718ULL, 0x1270ba4ba8d5c760ULL, 0xf25d30addfb70f63ULL, 0x8fdf2f164cbb28d2ULL, 0xff951250cb365ccbULL, 0xd8cbff7ebcb2e507ULL, 0x8e6de4863c67a0deULL, 0xa6703f8e5239c752ULL, 0xb3da9c6a5e48b829ULL, 0x6394d0a45815a31eULL, 0xa6da15bae8bee576ULL, 0x31cf7ea5639e81beULL, 0x77486f1edcd89345ULL, 0x852dca84ba32fa9fULL, 0x142a70d31f42e2c8ULL, 0x32f2d373f887858cULL, 0x2acf0577bae9ce5bULL, 0x379cc388e6e462daULL, 0xf412c0afd510df69ULL, 0x60e40b45ed4dc5a5ULL, 0x400b3f5548c58635ULL, 0x0847f842a159e368ULL, 0xf5639e0592a31fd4ULL, 0xb180d8460bfc0508ULL, 0xca47994d61b26999ULL, 0x3cd551a05d0bcf09ULL, 0xe9e2787196317d02ULL, 0x0bf282eda44ad123ULL, 0xcf78d77483c2ae1aULL, 0x4c207df0a62cfa27ULL, 0xc1456a3bad954d16ULL, 0x51a88304b2ef8540ULL, 0xf064ecc9cb27159eULL, 0x9383ea4d5e541aceULL, 0x821a7b6ca2b10268ULL, 0x86f7b492ea102697ULL, 0x0c96cc6bde049529ULL, 0xb1e501e58f228c9aULL, 0x6cd248260a441c6bULL, 0x527a42178b694e4eULL, 0x923c6d628cb9ef1dULL, 0x380bd847ea7ffc37ULL, 0xde0ef7644c272808ULL, 0x0a7196fad3deaad6ULL, 0x0a79511d70c07ac9ULL, 0x344962c60f64e415ULL, 0x38cc0314fd710246ULL, 0xbcbc3e0ac338bf82ULL, 0xf7c518f07e3f93afULL, 0xe980a1aeab84f49fULL, 0x6221e6d1fd9f1283ULL, 0x413c06ff3fb69841ULL, 0x1c7d72c9c74f3918ULL, 0xac3ddf47f1dad95bULL, 0xd8de30ff8a4f7b73ULL, 0xd97f8d43c07a923eULL, 0x0cd9e790a1bb0142ULL, 0x96028ad773bc3505ULL, 0x4a62d634c08c8b82ULL, 0x85c7cce5a7fe0ce3ULL, 0xbc38a569dfee5977ULL, 0xbad242867116e30eULL, 0x793f519994835b84ULL, 0xd0ddee1fe1754f03ULL, 0x7a7b9b674dbce967ULL, 0x65a995f849ec2a90ULL, 0x087da69cca3f71ccULL, 0x164268baccf57bb4ULL, 0x3ed7cb774e7ff321ULL, 0x7b70a0ae7dcc7ab6ULL, 0x38809c55512a6529ULL, 0xc17f95f297f78f63ULL, 0xe9924b6f54ab7c69ULL, 0x4a6aeb01c848fb5dULL, 0xd4be24d951c13a6bULL, 0x58f3c97a8543816fULL, 0x96830b5189b973aaULL, 0xe59312d47fb6a33eULL, 0x6104d2ae3dd62e7dULL, 0x07b4b3dc8530efd7ULL, 0x8c02f86be032afe8ULL, 0x4da0826a323f1db2ULL, 0xd70585b8fc3bcf3eULL, 0x3bedfc6b5ff39ef9ULL, 0x79522d331b9ddadeULL, 0x1d5d5de33c0e97feULL, 0x81db09d467aee335ULL, 0x9cc10017d3ec9080ULL, 0xb324065634853626ULL, 0x3fcdfeae172de631ULL, 0x7b5e802518f4ebdfULL, 0xf72357749319c520ULL, 0xb4578867d624d282ULL, 0xc2cdecda21815378ULL, 0x45c0d7d2c26a86e4ULL, 0xe0d916f8c7a51f60ULL, 0xcbff704272273978ULL, 0x30cc678661f916efULL, 0x518a2686b06136efULL, 0xa33fedb16300e15bULL, 0x32b341682ced7e44ULL, 0xfa557a06eec4f264ULL, 0x68908c987d0f7d77ULL, 0xe3361810d61005b6ULL, 0x478d432ec9ce7bd9ULL, 0xcf42a78b504a9f49ULL, 0x789523e8fbdab040ULL, 0xfee72d95cd6bda76ULL, 0x96590cec23fe7477ULL, 0x81ede5aee63734e9ULL, 0x2d6a088e73413f56ULL, 0x8e71b9d5cb79a906ULL, 0x6189e2f835b03bdcULL, 0x4b0e2329fcbf84d6ULL, 0xd3c6bb056d950873ULL, 0x52fdb78e27ddbc2dULL, 0xe96e95e60e83cc78ULL, 0x4046f2d84034b253ULL, 0xe7490eae8c032b82ULL, 0xb6af4a29564fda5eULL, 0x3ac094cbcbd904c5ULL, 0x8cdaedc5fa02d271ULL, 0x80fbb2c13ecf6bd1ULL, 0x7c723aba121849fbULL, 0x4e3cbb3c92bd7b6cULL, 0xb159ef442b9587c6ULL, 0xa8d6338e9b34c936ULL, 0x28efd9c43cdec4aaULL, 0xfe0129546c67a069ULL, 0xe24613044e1df22cULL, 0xecd5dfdc44ac9268ULL, 0xead650328ac06a4dULL, 0x8faa2ac987a99d9aULL, 0x34f5727c41a29b3fULL, 0x2d04f2aa8b89a186ULL, 0xdaed9f9e99203e62ULL, 0x6b3fd646d7bbc0d2ULL, 0xf6d2766fbc6dd6aeULL, 0xbf5b98f0ca50da89ULL, 0x9a1cd3cac3ce6a52ULL, 0x0db1b9143b811034ULL, 0xced33a88a8bde45cULL, 0x83e0d45327d0de5cULL, 0x4514939ddd069631ULL, 0x8de381c5e520adbeULL, 0xc2f73859171ee5fbULL, 0x6ea8756e7865e566ULL, 0x6a6e6bef80a7d753ULL, 0x65501543f355c3ecULL, 0x2d7f605132ec937cULL, 0xa29e7bff9e977401ULL, 0x684f5bcd755d9f0fULL, 0xfe7583858dac447bULL, 0x7f13b12feb3ba762ULL, 0x4c2a9a1953e5d101ULL, 0xd1ac9ec755fa6c8cULL, 0x50debdaf9a835915ULL, 0xa01e2c1d9b83f06fULL, 0x14135afb741a6a65ULL, 0x3fd56180af25c9dbULL, 0x8c775f57663ae08dULL, 0x0206a1d0cbe13ce0ULL, 0x41b9a9cc15dc0575ULL, 0xc7be912d71c2f647ULL, 0x9b26d7e338c6801fULL, 0x804e70fa4738170bULL, 0x625e2a3fc167e0b9ULL, 0xd4dd6519023d3d1cULL, 0xe8517971dd2bd775ULL, 0x539f2d2c526cf99cULL, 0xd3c2b9a7a175944bULL, 0x041ff595c3b5ff8eULL, 0x55d29f36c27070f6ULL, 0xb04d9c4cbda828e5ULL, 0xa126c518bd0b2af3ULL, 0xab49d7171de17064ULL, 0xcadeb4c46af1e634ULL, 0x68f63b4fe2f0d319ULL, 0x50f4457a608a7492ULL, 0xcfd1f8f3c078f3bcULL, 0x09cff900153376bdULL, 0x61195951849b20cbULL, 0x83d37b96ce1dbe03ULL, 0x953b2fafd6f550f9ULL, 0x769850fffa000e02ULL, 0x4da70cb44e07f338ULL, 0x2607b9f713dd3210ULL, 0x11a8143c331af72fULL, 0xfb0447e937ef6c63ULL, 0x16e6aa09a3a63d27ULL, 0x34815a5130e3183bULL, 0x7d4c6cd7bf8e6a79ULL, 0xe2adba8de8352189ULL, 0x46491dad36f08747ULL, 0xd841005859aa36f9ULL, 0xc64f4f870360ac7fULL, 0x32a516e6ea5ba76eULL, 0x9f3f290779cf61d0ULL, 0x156451d3855bcc79ULL, 0x16506e4078b494d0ULL, 0x25acc0dddc887d52ULL, 0x343b241d0f9431c2ULL, 0xca642b3728bd51fdULL, 0x41cc279ca585099cULL, 0x8d4bc0267f9ee6a9ULL, 0xea3c596a683cec2aULL, 0x76221e323dac1d0cULL, 0x42fc3b753c53d094ULL, 0x8862367960a227a7ULL, 0xaba866c0a22b0485ULL, 0x914c35e071fe29b2ULL, 0x6aee593cc8788b2fULL, 0xee232d82b7693bc1ULL, 0xe3b16864cc817356ULL, 0x786f367db8681028ULL, 0x61890bedfc608297ULL, 0x9a9e0c0b0227bf8eULL, 0xb7dd384df50391f6ULL, 0xdaf4e68f0ea5eeb8ULL, 0x609d94a287547027ULL, 0xf73a588917409205ULL};


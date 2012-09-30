#include <stdint.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <time.h>
#include <sstream>
#include <limits>
#include <unistd.h>
#define __MMX__
#define __SSE__
#define __SSE2__
#define __SSE3__
#define __SSSE3__
#define __SSE4_1__
#pragma GCC target ("sse4.1")
#include <smmintrin.h>
#include <tmmintrin.h>
#include <pmmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include <mmintrin.h>
typedef __m128i m128;
typedef uint8_t uint8;
typedef uint16_t uint16;
// typedef uint64_t uint64;
typedef int32_t sint32;
typedef uint32_t uint32;

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

/*
int indexOfNthBit(uint64 bits, int n)
{
	for(;;) {
		int index = __builtin_ctzll(bits);
		if(n == 0)
			return index;
		bits ^= 1ULL << index;
		--n;
	}
}
*/

int indexOfNthBit(uint32 bits, int n)
{
	for(;;) {
		int index = __builtin_ctzl(bits);
		if(n == 0)
			return index;
		bits ^= 1ULL << index;
		--n;
	}
}

std::ostream& operator<<(std::ostream& out, const m128 in)
{
	union{
		uint16 bits[8];
		m128 vector;
	};
	vector = in;
	out.fill('0');
	out << std::hex;
	for(int i = 0; i < 8; ++i) {
		out.width(4);
		out << bits[i];
	}
	out.fill(' ');
	out << std::dec;
	return out;
}

//
//   B O A R D   P O I N T
//

class BoardPoint {
public:
	BoardPoint(): _horizontal(0), _vertical(0) { }
	BoardPoint(int horizontal, int vertical): _horizontal(horizontal), _vertical(vertical) { }
	~BoardPoint() {}
	
	int number() const { return _vertical * 15 + _horizontal; }
	int horizontal() const { return _horizontal; }
	BoardPoint& horizontal(int value) { _horizontal = value; return *this; }
	int vertical() const { return _vertical; }
	BoardPoint& vertical(int value) { _vertical = value; return *this; }
	
	BoardPoint left() const { return BoardPoint(_horizontal - 1, _vertical); }
	BoardPoint right() const { return BoardPoint(_horizontal + 1, _vertical); }
	BoardPoint up() const { return BoardPoint(_horizontal, _vertical + 1); }
	BoardPoint down() const { return BoardPoint(_horizontal, _vertical - 1); }
	
protected:
	int _horizontal;
	int _vertical;
};

std::ostream& operator<<(std::ostream& out, const BoardPoint& point)
{
	char hor[2] = { 'A' + char(point.horizontal()), 0x00 };
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
	BoardMask();
	BoardMask(const BoardMask& other);
	BoardMask(const BoardPoint& point);
	~BoardMask() { }
	BoardMask& operator=(const BoardMask& other);
	bool operator==(const BoardMask& other) const;
	bool operator!=(const BoardMask& other) const { return !operator==(other); }
	BoardMask& operator&=(const BoardMask& other);
	BoardMask& operator|=(const BoardMask& other);
	BoardMask& operator-=(const BoardMask& other);
	BoardMask operator&(const BoardMask& other) const;
	BoardMask operator|(const BoardMask& other) const;
	BoardMask operator-(const BoardMask& other) const;
	BoardMask operator~() const;
	BoardMask expanded() const;
	BoardMask& invert() { return operator=(operator~()); }
	BoardMask& expand() { return operator=(expanded()); }
	int popcount() const;
	BoardMask& clear();
	BoardMask& set(const BoardPoint& point);
	bool isSet(const BoardPoint& point) const;
	bool isEmpty() const;
	BoardPoint firstPoint() const;
	BoardPoint randomPoint() const;
	
protected:
	friend class PointIterator;
	m128 bits[2];
	
	static BoardPoint planePoint(int plane, int index);
};

std::ostream& operator<<(std::ostream& out, const BoardMask& boardMask);

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
	vectCompare = _mm_or_si128(vectCompare, _mm_cmpeq_epi8(bits[1], other.bits[1]));
	return _mm_movemask_epi8(vectCompare);
}

inline BoardMask BoardMask::expanded() const
{
	BoardMask result;
	m128 mask = _mm_set1_epi16(0x7fff);
	std::cerr << mask << std::endl;
	result.bits[0]  = bits[0];
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_epi16(bits[0], 1));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_srli_epi16(bits[0], 1));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_si128(bits[0], 2));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_srli_si128(bits[0], 2));
	result.bits[0] = _mm_or_si128(result.bits[0], _mm_slli_si128(bits[0], 14));
	result.bits[0] = _mm_and_si128(result.bits[0], mask);
	mask = _mm_xor_si128(mask, _mm_slli_si128(mask, 14));
	std::cerr << mask << std::endl;
	result.bits[1] = bits[1];
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_si128(bits[0], 14));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_slli_epi16(bits[1], 1));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_epi16(bits[1], 1));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_slli_si128(bits[1], 2));
	result.bits[1] = _mm_or_si128(result.bits[1], _mm_srli_si128(bits[1], 2));
	result.bits[1] = _mm_and_si128(result.bits[1], mask);
	return result;
}

inline int BoardMask::popcount() const
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
	bits[0] = _mm_andnot_si128(bits[0], other.bits[0]);
	bits[1] = _mm_andnot_si128(bits[1], other.bits[1]);
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
	result.bits[0] = _mm_andnot_si128(mask, bits[0]);
	mask =_mm_xor_si128(mask, _mm_slli_si128(mask, (uint8)(14)));
	result.bits[1] = _mm_andnot_si128(mask, bits[1]);
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

inline BoardPoint BoardMask::firstPoint() const
{
	/// TODO
	/*
	if(bits[0])
		return planePoint(0, __builtin_ctzl(bits[0]));
	if(bits[1])
		return planePoint(1, __builtin_ctzl(bits[1]));
	if(bits[2])
		return planePoint(2, __builtin_ctzl(bits[2]));
	if(bits[3])
		return planePoint(3, __builtin_ctzl(bits[3]));
	if(bits[4])
		return planePoint(4, __builtin_ctzl(bits[4]));
	if(bits[5])
		return planePoint(5, __builtin_ctzl(bits[5]));
	if(bits[6])
		return planePoint(6, __builtin_ctzl(bits[6]));
	if(bits[7])
		return planePoint(7, __builtin_ctzl(bits[7]));
	*/
	return BoardPoint();
}

inline BoardPoint BoardMask::randomPoint() const
{
	/// TODO
	return BoardPoint();
	/*
	int numPoints = popcount();
	if(numPoints == 0)
		return BoardPoint();
	int index = rand() % numPoints;
	int planeCount = __builtin_popcountl(bits[0]);
	if(index < planeCount)
		return planePoint(0, indexOfNthBit(bits[0], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[1]);
	if(index < planeCount)
		return planePoint(1, indexOfNthBit(bits[1], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[2]);
	if(index < planeCount)
		return planePoint(2, indexOfNthBit(bits[2], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[3]);
	if(index < planeCount)
		return planePoint(3, indexOfNthBit(bits[3], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[4]);
	if(index < planeCount)
		return planePoint(4, indexOfNthBit(bits[4], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[5]);
	if(index < planeCount)
		return planePoint(5, indexOfNthBit(bits[5], index));
	index -= planeCount;
	planeCount = __builtin_popcountl(bits[6]);
	if(index < planeCount)
		return planePoint(6, indexOfNthBit(bits[6], index));
	index -= planeCount;
	return planePoint(7, indexOfNthBit(bits[7], index));
	*/
}

inline BoardPoint BoardMask::planePoint(int plane, int index)
{
	int vertical = index / 16;
	int horizontal = index - 16 * vertical;
	return BoardPoint(horizontal, 2 * plane + vertical);
}

std::ostream& operator<<(std::ostream& out, const BoardMask& board)
{
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	for(int y = 15; y >= 0; --y) {
		out.width(2);
		out << y + 1 << " ";
		for(int x = 0; x < 16; ++x)
			out << ((board.isSet(BoardPoint(x, y))) ? "0" : "·");
		out << " ";
		out.width(2);
		out << y + 1;
		out << std::endl;
	}
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	return out;
}


//
//   P O I N T   I T E R A T O R
//

class PointIterator
{
public:
	PointIterator(const BoardMask& boardMask);
	~PointIterator() { }
	
	bool next();
	BoardPoint point() const { return _point; }
	
protected:
	const BoardMask& _boardMask;
	BoardPoint _point;
	int _plane;
	m128 _bits;
};

PointIterator::PointIterator(const BoardMask& boardMask)
: _boardMask(boardMask)
, _point()
, _plane(0)
, _bits(boardMask.bits[0])
{
}

bool PointIterator::next()
{
	/// TODO
	return false;
	/*
	while(_bits == 0) {
		if(_plane >= 7)
			return false;
		++_plane;
		_bits = _boardMask.bits[_plane];
	}
	int index = __builtin_ctzl(_bits);
	_bits ^= 1UL << index;
	_point = BoardMask::planePoint(_plane, index);
	return true;
	*/
}

//
//   G R O U P   I T E R A T O R
//

class GroupIterator
{
public:
	static int count(const BoardMask& boardMask);
	static std::vector<BoardMask> list(const BoardMask& boardMask);
	
	GroupIterator(const BoardMask& boardMask);
	~GroupIterator() { }
	
	bool hasNext() const { return !_remaining.isEmpty(); }
	bool next();
	
	BoardMask group() const { return _group; } 
	
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

std::vector<BoardMask> GroupIterator::list(const BoardMask& boardMask)
{
	std::vector<BoardMask> result;
	GroupIterator gi(boardMask);
	while(gi.next())
		result.push_back(gi.group());
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
	
	// Seed the new group with a single point
	_group = BoardMask(_remaining.firstPoint());
	
	// Expand and mask until fixed point
	BoardMask old;
	do {
		old = _group;
		_group.expand();
		_group &= _remaining;
	} while(_group != old);
	
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
	Board();
	~Board() { }
	
	const BoardMask& white() const { return _white; }
	Board& white(const BoardMask& value) { _white = value; return *this; }
	const BoardMask& black() const { return _black; }
	Board& black(const BoardMask& value) { _black = value; return *this; }
	// uint64 hash() const { return _hash; }
	bool hasExpanded() const { return _hasExpanded; }
	void recalculateHash();
	
	bool gameOver() const { return (~(_white | _black)).isEmpty(); }
	
	void whiteMove(const BoardPoint& move);
	void blackMove(const BoardPoint& move);
	
protected:
	// const static uint64 zobristWhite[15*15];
	// const static uint64 zobristBlack[15*15];
	BoardMask _white;
	BoardMask _black;
	BoardMask _currentMove;
	bool _hasExpanded;
	// uint64 _hash;
};

std::ostream& operator<<(std::ostream& out, const Board& board);

Board::Board()
: _white()
, _black()
, _hasExpanded(false)
// , _hash(0)
{
}

void Board::whiteMove(const BoardPoint& move)
{
	_hasExpanded |= _white.expanded().isSet(move);
	_white.set(move);
	// _hash ^= zobristWhite[move.number()];
}

void Board::blackMove(const BoardPoint& move)
{
	_hasExpanded |= _black.expanded().isSet(move);
	_black.set(move);
	// _hash ^= zobristBlack[move.number()];
}

void Board::recalculateHash()
{
	//_hash = 0;
	for(int i = 0; i < 15; ++i)
	for(int j = 0; j < 15; ++j) {
		BoardPoint p(i, j);
		//if(_white.isSet(p))
		//	_hash ^= zobristWhite[p.number()];
		//if(_black.isSet(p))
		//	_hash ^= zobristWhite[p.number()];
	}
}

std::ostream& operator<<(std::ostream& out, const Board& board)
{
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	for(int y = 14; y >= 0; --y) {
		out.width(2);
		out << y + 1 << " ";
		for(int x = 0; x < 15; ++x) {
			BoardPoint p(x, y);
			if(board.black().isSet(p))
				out << "B"; // "●";
			else if(board.white().isSet(p))
				out << "W"; // "○";
			else
				out << "·";
		}
		out << " ";
		out.width(2);
		out << y + 1;
		out << std::endl;
	}
	out << "   ABCDEFGHIJKLMNO" << std::endl;
	return out;
}


//
//   T U R N   I T E R A T O R
//

class TurnIterator
{
public:
	TurnIterator(const BoardMask& player, const BoardMask& opponent);
	~TurnIterator() { }
	
	bool done() const { return _moves.isEmpty(); }
	BoardMask moves() const { return _moves; }
	void choose(const BoardPoint& point);
	
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
}





//
//  T A B L E   E N T R Y
//

/*
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
	TableEntry(uint64 hash, double score, Kind kind);
	TableEntry(const Board& board, double score, Kind kind);
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
, _score(0.0)
, _kind(notFound)
{
}

TableEntry::TableEntry(uint64 hash, double score, TableEntry::Kind kind)
: _hash(hash)
, _score(score)
, _kind(kind)
{
}

TableEntry::TableEntry(const Board& board, double score, TableEntry::Kind kind)
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
	static int _size;
	TableEntry* _table;
};

int Table::_size = (60 << 20) / sizeof(TableEntry);

Table table;

Table::Table()
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
	static TableEntry notFound;
	if(_table[hash % _size].hash() == hash)
		return _table[hash % _size];
	return notFound;
}

void Table::put(const TableEntry& entry)
{
	_table[entry.hash() % _size] = entry;
}
*/

//
//  S C O R E   H E U R I S T I C
//

class ScoreHeuristic {
public:
	ScoreHeuristic(const Board& board);
	~ScoreHeuristic() {}
	
	sint32 evaluate();
	
protected:
	const Board& _board;
	sint32 spotScore[225][15][15];
	sint32 groupScore[225][15][15];
};

ScoreHeuristic::ScoreHeuristic(const Board& board)
: _board(board)
{
}

sint32 ScoreHeuristic::evaluate()
{
	sint32 score = 0;
	
	int progress = (_board.black() | _board.white()).popcount();
	
	// The score of each player is determined by counting the number of stones he has placed on the board. 
	score += 1000 * _board.white().popcount();
	score -= 1000 * _board.black().popcount();
	
	// For each separate group 6 points will be subtracted. Note that at least 6 points will be subtracted since the stones will always form at least one group.
	int whiteGroups = GroupIterator::count(_board.white());
	int blackGroups = GroupIterator::count(_board.black());
	int groupsDiscount = blackGroups - whiteGroups;
	score += 6000 * groupsDiscount;
	const int convergeStart = 180;
	const int growStart = 9;
	if(progress < convergeStart) {
		score += 9000 * min(whiteGroups, growStart);
		score -= 9000 * min(blackGroups, growStart);
	}
	
	// Add points for expansion room
	BoardMask unoccupied = ~(_board.white() | _board.black());
	BoardMask whiteExpanded = _board.white().expanded() & unoccupied;
	BoardMask blackExpanded = _board.black().expanded() & unoccupied;
	score += 100 * whiteExpanded.popcount();
	score -= 100 * blackExpanded.popcount();
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += 50 * whiteExpanded.popcount();
	score -= 50 * blackExpanded.popcount();
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += 25 * whiteExpanded.popcount();
	score -= 25 * blackExpanded.popcount();
	whiteExpanded.expand();
	whiteExpanded &= unoccupied;
	blackExpanded.expand();
	blackExpanded &= unoccupied;
	score += 12 * whiteExpanded.popcount();
	score -= 12 * blackExpanded.popcount();
	
	// The player with the highest score gets 100 bonus points.
	if(progress == 225)
		score += 100000 * sgn(score);
	
	// // Score upper bound: 15 x 15 - 6 + 100 = 319
	// // Scores are multiplied by 2^10 to create space for fractional points
	// // This gives approximately 10 bits of headroom on msb and lsb side
	// score <<= 10;
	
	return score;
}

//
//
//



class Game {
public:
	Game();
	~Game() { }
	
	void play();
	
protected:
	Board _board;
	bool _isWhite;
	bool _isBlack;
	std::string makeMoves();
	void receiveMoves(const std::string& moves);
};

Game::Game()
: _board()
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
		std::string moves = makeMoves();
		std::cerr << _board << std::endl;
		std::cerr << "Out: " << moves << std::endl;
		std::cout << moves << std::endl;
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

std::string Game::makeMoves()
{
	std::stringstream result;
	TurnIterator ti(
		(_isWhite) ? _board.white() : _board.black(),
		(_isWhite) ? _board.black() : _board.white());
	std::vector<BoardPoint> moves;
	while(!ti.done()) {
		if(!result.str().empty())
			result << "-";
		
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
			ScoreHeuristic sc(afterMove);
			sint32 moveScore = sc.evaluate();
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
		BoardPoint move = goodMoves.randomPoint();
		ti.choose(move);
		result << move;
		moves.push_back(move);
	}
	for(int i = 0; i < moves.size(); ++i) {
		if(_isWhite)
			_board.whiteMove(moves[i]);
		else
			_board.blackMove(moves[i]);
	}
	return result.str();
}

void Game::receiveMoves(const std::string& moves)
{
	std::stringstream ss(moves);
	BoardPoint move;
	while(ss.good()) {
		ss >> move;
		std::cerr << "Move " << move << std::endl;
		ss.get(); // the minus character
		if(_isWhite)
			_board.blackMove(move);
		else
			_board.whiteMove(move);
	}
}


//
//  M A I N
//

int main(int argc, char* argv[])
{
	srand(time(0));
	#ifdef LOCAL
		srand(rand() ^ getpid());
	#endif
	std::cerr << "R " << argv[0]  << std::endl;
	//std::cerr << "Table entry size: " << sizeof(TableEntry) << std::endl;
	//std::cerr << "Table size: " << table.size() << std::endl;
	std::cerr << "sizeof(uint64): " << sizeof(uint64_t) << std::endl;
	std::cerr << "sizeof(m128): " << sizeof(m128) << std::endl;
	
	BoardMask bm;
	std::cerr << bm << std::endl;
	std::cerr << bm << std::endl;
	for(int i = 0; i < 15; ++i)
		bm.set(BoardPoint(i, i));
	std::cerr << bm << std::endl;
	std::cerr << bm.popcount() << std::endl;
	bm.expand();
	std::cerr << bm << std::endl;
	bm.expand();
	std::cerr << bm << std::endl;
	std::cerr << bm.popcount() << std::endl;
	return 0;
	
	Game g;
	g.play();
	
	std::cerr << "Exit" << std::endl;
	return 0;
}

/*

const uint64 Board::zobristWhite[15*15] = {
	0x59ff30cdaeb11f30, 0xcbea4e0e315353c7, 0x36ddafebe344b37f, 0x21ac7d5399ea0d14, 0xf08b8086b369794d, 0x121694bf54a41b46, 0xf8423970f5f7387f, 0x34535bea4b79a822, 0x4119d278d6bdc7b4, 0x972a951be6ed352f, 0xba596f4757b00c93, 0x97f51be078b75a3b, 0xeaab071cbdab19dd, 0xa4eaa35fac046f1a, 0x914afed077e93704, 0x9ff7bf8cd00bacac, 0x5bb2530a10d3ffb6, 0xa99baf96a648ae4d, 0x6235df8ba1cd778a, 0x481a6794934883a4, 0xf42aa308cbb358b1, 0xcfb28dbb428ee1ac, 0x3ff2a201668413b5, 0x60515bbc22abc047, 0xad104bef8b61e30e, 0xd7c13324fbfb676c, 0xeef173e9cb9c3b75, 0x8c9a7010d13f543f, 0xa956fd1e701a0bc2, 0x9ed090bcdfe433e8, 0x83576f23dfa8c045, 0xfb6ac1f38840ed37, 0xbd596919635094b3, 0x6efea445d7e0fe7a, 0x1a44301636891d99, 0xe42216c728f19b40, 0xc78e7e0b428aa55c, 0x466986d734541f81, 0x3b29aac9a9449847, 0x3bd344c86ef5f182, 0x10f2624d046a24dd, 0x4a28dc8fbb176c30, 0x8c2ac0258503c132, 0xc4dcff8d189a0233, 0x8e299ed20e39bdb7, 0x8478adf3f7fb4bd8, 0x591cdb02336b7b3f, 0x58be8a9cd6cf0588, 0xf10485f134250c3e, 0x1ef6ca189fe6e309, 0xc207244afcfa7a17, 0x9fbdb0794361491c, 0x03dadacec98a10a1, 0x51d258f836d7fc6c, 0xfc8c79db6f81e8a1, 0x0b208128a417db22, 0x737c6d3307d78f4d, 0xacd96a9a1843de9a, 0x3c3f9893c4162fc6, 0xfa42dfe81c03cd5c, 0x89a9acfd2ba32583, 0x66a092ea8bf7f671, 0x6305c56ec48064eb, 0xcddecbd58df6ef2f, 0xcdef9d71c478265e, 0x247e8c9d44f1eaf2, 0x7e8b41a65c98356f, 0x0ab9eb0d9b719bad, 0x82cfbaf75db0eba2, 0xfe279388dd19d200, 0x103421854adb67c2, 0x0d4b27b3bad83806, 0x3dfe5e933572324b, 0xdfa672125bf58c0f, 0x8175db82e6fddc41, 0x0e406cacaa3a89d5, 0x58a3b7204d435ae1, 0x18e747e52f3d2ee0, 0x18106fa796424fef, 0xa9e0709ab6fae37e, 0x1b9094eb361764ec, 0x0d391ed73638bd43, 0x91c5a0cb21818f5a, 0xa5b9a2e79877b43a, 0xe77ae74041883df0, 0x252f557244216b7a, 0x6797395ce455ad9f, 0xca81efa898877896, 0xdde24236523a7b4b, 0x72fcaba306d3fc08, 0x754b3c9e473107de, 0x6013dca10adb34bc, 0x1754e3fea6e33bc9, 0x587a117014d594ac, 0xe32349ae3e241ec7, 0xf8967e3b267199e7, 0x0f791764d1d39f53, 0x4d25e0181d0f7fd2, 0x552e814b97319546, 0xdeb45e42aa747fb6, 0x13b29bd3c85834f0, 0x3523ab3da7b15f39, 0xbad831a931e812f9, 0x0bda6df9711c2d64, 0x4ca94e8cb4231f8e, 0xea0a4218411e742a, 0x51297fbbf74974c2, 0xd1aebd43f5a07526, 0xebac7170dfe9ae23, 0xac7aa86392e6e4d4, 0xe183ce1f7337e01d, 0xa0d95fec5f920161, 0x49fa42b5fd3b28d5, 0x7260a56d9befd2a4, 0x3fbd1088b7393158, 0x00421c09db4b607b, 0xc02ba051b92bd2a7, 0x1dd7f5473606d174, 0x196aac66cd51d65a, 0x110f5c25870fe186, 0xfae679646b22c453, 0x60b2cd3fbcd4170e, 0xf0d7815849a93726, 0x2078dc850236dc17, 0x99c9145240221de7, 0x06d141d7ce1df036, 0x3d1fc69e9dac8dc0, 0xdcc1a82b1481eba2, 0xb468c4d6f48dad35, 0x7e291cba71203902, 0x7161ced58d9833e0, 0x2bf62bad86392c68, 0x06e1941aa9952a6c, 0xdbe238c50e544f0a, 0x11dc2eeed0d9b4ee, 0x759195dd3e00060c, 0x373d11a02cfc49ac, 0x1a246c1e74281604, 0x3f1cf73143d85672, 0x8254d9e7693fc961, 0xeaac1241bee7f1c3, 0x8f18c01b5bef7668, 0x9c32b764637978e5, 0x4ae075ba2a03a907, 0x1dba954be380ffac, 0x93401921e51a64ff, 0xe02a83b6755a8cb3, 0x68190601403a5e23, 0x52f5a692e9506b20, 0x153d8f74ff140197, 0x67a1cd205c078951, 0x8508b7ab10102a1a, 0x2d3479cae852f969, 0x43e1b48c7cefb03c, 0x9c16d130aa00b797, 0x70b57cd33c650215, 0x13d834a8b07dfaf9, 0x93e3f796e352cf41, 0x0f4725c1e1e25df8, 0x5936430c3a90a776, 0x50ab26b9914fd4c1, 0x5d3afc160d66e4d6, 0x302f9cf18dc97e11, 0x483a4e96d2afeb9a, 0xca59a7b0b65ef67d, 0x10ad170f3e35dc5d, 0x1354ce7806b0ea1d, 0x055dee169e51c15c, 0xfb67d2c81cc84caf, 0x4641918a18dccd02, 0x6032cc4fc2618760, 0x415b47bf01264ccf, 0xd12ee7388f693aa0, 0x5c66152f4760be3c, 0x03015c6006b75691, 0x14e0129c78055d4e, 0x16d13075d249c80b, 0x9d0d9ee957447478, 0xef9fe65c3f75cb17, 0x055ae79446825ab9, 0x20febf7cdc05a830, 0xcf655dcce0fd3aea, 0x46deaaf4aa6dec84, 0xafcdd341b5d2dc57, 0xca81124b40bfe3d4, 0xba246d54c0ee7c85, 0x7f03ff01046179c1, 0x6b53303adc1da26f, 0xc56fb72211c36b11, 0x62e3ad9ab61120eb, 0xbbab7a332573279f, 0xca01416bd3878b0d, 0xa29c7f6ce25a7535, 0x3105bad3e4f06780, 0x822ef8a2911c85fd, 0xd8b2129b06bfa62c, 0x77cc000d8d7a847c, 0x188a004a2f70b9ec, 0x0b503bc5cf47eaae, 0xd9a72cb8990d6214, 0xff636ce9cddc618a, 0x7625d14b53fac9ae, 0xb176994f98af0724, 0x8142e01dcc4e6788, 0x521291f0f276fd7d, 0x8ccddd8bb22bc827, 0x7829daa6283cd94e, 0xbbdfd927070a79e0, 0x47b7e816b21a59af, 0xe6a906a4ccec0e9f, 0x1b745aa87a4cf45a, 0xa4455b99400add4b, 0xffcd8cf506ecac1e, 0xd32b94287e9c332f, 0xda559051ce24df1d, 0x548faeaad50f89c8, 0x00066f7e64a81be3, 0xcfb30b0bdc8c35ee, 0xa16442a3bafa207c, 0x06301ad8d2c46ef0, 0x99bd48a7a35d1310, 0x3722456e615f87c6, 0xe0f75c513a6099b7, 0x41f24f0543721aaa, 0xa9281264de1e4c82};
const uint64 Board::zobristBlack[15*15] = {
	0x5f7adf4d402775ea, 0x54cc1a8c299bd9c6, 0xc29d137b7218812d, 0x4678c7faa9ef2c29, 0x120fde7ca0e99718, 0x1270ba4ba8d5c760, 0xf25d30addfb70f63, 0x8fdf2f164cbb28d2, 0xff951250cb365ccb, 0xd8cbff7ebcb2e507, 0x8e6de4863c67a0de, 0xa6703f8e5239c752, 0xb3da9c6a5e48b829, 0x6394d0a45815a31e, 0xa6da15bae8bee576, 0x31cf7ea5639e81be, 0x77486f1edcd89345, 0x852dca84ba32fa9f, 0x142a70d31f42e2c8, 0x32f2d373f887858c, 0x2acf0577bae9ce5b, 0x379cc388e6e462da, 0xf412c0afd510df69, 0x60e40b45ed4dc5a5, 0x400b3f5548c58635, 0x0847f842a159e368, 0xf5639e0592a31fd4, 0xb180d8460bfc0508, 0xca47994d61b26999, 0x3cd551a05d0bcf09, 0xe9e2787196317d02, 0x0bf282eda44ad123, 0xcf78d77483c2ae1a, 0x4c207df0a62cfa27, 0xc1456a3bad954d16, 0x51a88304b2ef8540, 0xf064ecc9cb27159e, 0x9383ea4d5e541ace, 0x821a7b6ca2b10268, 0x86f7b492ea102697, 0x0c96cc6bde049529, 0xb1e501e58f228c9a, 0x6cd248260a441c6b, 0x527a42178b694e4e, 0x923c6d628cb9ef1d, 0x380bd847ea7ffc37, 0xde0ef7644c272808, 0x0a7196fad3deaad6, 0x0a79511d70c07ac9, 0x344962c60f64e415, 0x38cc0314fd710246, 0xbcbc3e0ac338bf82, 0xf7c518f07e3f93af, 0xe980a1aeab84f49f, 0x6221e6d1fd9f1283, 0x413c06ff3fb69841, 0x1c7d72c9c74f3918, 0xac3ddf47f1dad95b, 0xd8de30ff8a4f7b73, 0xd97f8d43c07a923e, 0x0cd9e790a1bb0142, 0x96028ad773bc3505, 0x4a62d634c08c8b82, 0x85c7cce5a7fe0ce3, 0xbc38a569dfee5977, 0xbad242867116e30e, 0x793f519994835b84, 0xd0ddee1fe1754f03, 0x7a7b9b674dbce967, 0x65a995f849ec2a90, 0x087da69cca3f71cc, 0x164268baccf57bb4, 0x3ed7cb774e7ff321, 0x7b70a0ae7dcc7ab6, 0x38809c55512a6529, 0xc17f95f297f78f63, 0xe9924b6f54ab7c69, 0x4a6aeb01c848fb5d, 0xd4be24d951c13a6b, 0x58f3c97a8543816f, 0x96830b5189b973aa, 0xe59312d47fb6a33e, 0x6104d2ae3dd62e7d, 0x07b4b3dc8530efd7, 0x8c02f86be032afe8, 0x4da0826a323f1db2, 0xd70585b8fc3bcf3e, 0x3bedfc6b5ff39ef9, 0x79522d331b9ddade, 0x1d5d5de33c0e97fe, 0x81db09d467aee335, 0x9cc10017d3ec9080, 0xb324065634853626, 0x3fcdfeae172de631, 0x7b5e802518f4ebdf, 0xf72357749319c520, 0xb4578867d624d282, 0xc2cdecda21815378, 0x45c0d7d2c26a86e4, 0xe0d916f8c7a51f60, 0xcbff704272273978, 0x30cc678661f916ef, 0x518a2686b06136ef, 0xa33fedb16300e15b, 0x32b341682ced7e44, 0xfa557a06eec4f264, 0x68908c987d0f7d77, 0xe3361810d61005b6, 0x478d432ec9ce7bd9, 0xcf42a78b504a9f49, 0x789523e8fbdab040, 0xfee72d95cd6bda76, 0x96590cec23fe7477, 0x81ede5aee63734e9, 0x2d6a088e73413f56, 0x8e71b9d5cb79a906, 0x6189e2f835b03bdc, 0x4b0e2329fcbf84d6, 0xd3c6bb056d950873, 0x52fdb78e27ddbc2d, 0xe96e95e60e83cc78, 0x4046f2d84034b253, 0xe7490eae8c032b82, 0xb6af4a29564fda5e, 0x3ac094cbcbd904c5, 0x8cdaedc5fa02d271, 0x80fbb2c13ecf6bd1, 0x7c723aba121849fb, 0x4e3cbb3c92bd7b6c, 0xb159ef442b9587c6, 0xa8d6338e9b34c936, 0x28efd9c43cdec4aa, 0xfe0129546c67a069, 0xe24613044e1df22c, 0xecd5dfdc44ac9268, 0xead650328ac06a4d, 0x8faa2ac987a99d9a, 0x34f5727c41a29b3f, 0x2d04f2aa8b89a186, 0xdaed9f9e99203e62, 0x6b3fd646d7bbc0d2, 0xf6d2766fbc6dd6ae, 0xbf5b98f0ca50da89, 0x9a1cd3cac3ce6a52, 0x0db1b9143b811034, 0xced33a88a8bde45c, 0x83e0d45327d0de5c, 0x4514939ddd069631, 0x8de381c5e520adbe, 0xc2f73859171ee5fb, 0x6ea8756e7865e566, 0x6a6e6bef80a7d753, 0x65501543f355c3ec, 0x2d7f605132ec937c, 0xa29e7bff9e977401, 0x684f5bcd755d9f0f, 0xfe7583858dac447b, 0x7f13b12feb3ba762, 0x4c2a9a1953e5d101, 0xd1ac9ec755fa6c8c, 0x50debdaf9a835915, 0xa01e2c1d9b83f06f, 0x14135afb741a6a65, 0x3fd56180af25c9db, 0x8c775f57663ae08d, 0x0206a1d0cbe13ce0, 0x41b9a9cc15dc0575, 0xc7be912d71c2f647, 0x9b26d7e338c6801f, 0x804e70fa4738170b, 0x625e2a3fc167e0b9, 0xd4dd6519023d3d1c, 0xe8517971dd2bd775, 0x539f2d2c526cf99c, 0xd3c2b9a7a175944b, 0x041ff595c3b5ff8e, 0x55d29f36c27070f6, 0xb04d9c4cbda828e5, 0xa126c518bd0b2af3, 0xab49d7171de17064, 0xcadeb4c46af1e634, 0x68f63b4fe2f0d319, 0x50f4457a608a7492, 0xcfd1f8f3c078f3bc, 0x09cff900153376bd, 0x61195951849b20cb, 0x83d37b96ce1dbe03, 0x953b2fafd6f550f9, 0x769850fffa000e02, 0x4da70cb44e07f338, 0x2607b9f713dd3210, 0x11a8143c331af72f, 0xfb0447e937ef6c63, 0x16e6aa09a3a63d27, 0x34815a5130e3183b, 0x7d4c6cd7bf8e6a79, 0xe2adba8de8352189, 0x46491dad36f08747, 0xd841005859aa36f9, 0xc64f4f870360ac7f, 0x32a516e6ea5ba76e, 0x9f3f290779cf61d0, 0x156451d3855bcc79, 0x16506e4078b494d0, 0x25acc0dddc887d52, 0x343b241d0f9431c2, 0xca642b3728bd51fd, 0x41cc279ca585099c, 0x8d4bc0267f9ee6a9, 0xea3c596a683cec2a, 0x76221e323dac1d0c, 0x42fc3b753c53d094, 0x8862367960a227a7, 0xaba866c0a22b0485, 0x914c35e071fe29b2, 0x6aee593cc8788b2f, 0xee232d82b7693bc1, 0xe3b16864cc817356, 0x786f367db8681028, 0x61890bedfc608297, 0x9a9e0c0b0227bf8e, 0xb7dd384df50391f6, 0xdaf4e68f0ea5eeb8, 0x609d94a287547027, 0xf73a588917409205};
*/

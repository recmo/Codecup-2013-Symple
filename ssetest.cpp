#include <iostream>
#include <inttypes.h>
#pragma GCC target ("sse4.1")
#define ssefunc __attribute__ ((__target__ ("sse4.1")))
#define __MMX__
#define __SSE__
#define __SSE2__
#define __SSE3__
#define __SSSE3__
#define __SSE4_1__
#include <smmintrin.h>
#include <pmmintrin.h>
#include <tmmintrin.h>
#include <xmmintrin.h>
#include <mmintrin.h>
typedef uint8_t uint8;
typedef __m128i m128;

// http://sseplus.sourceforge.net/fntable.html
// http://gcc.gnu.org/onlinedocs/gcc/X86-Built_002din-Functions.html#X86-Built_002din-Functions

// and
// and not
// or
// xor

std::ostream& operator<<(std::ostream& out, const m128 in) ssefunc;
inline std::ostream& operator<<(std::ostream& out, const m128 in)
{
	union{
		m128 vector;
		uint8 bits[16];
	};
	vector = in;
	out.fill('0');
	out << std::hex;
	for(int i = 15; i >= 0; --i) {
		out.width(2);
		out << int(bits[i]);
	}
	out.fill(' ');
	out << std::dec;
	return out;
}

m128 zero() ssefunc;
inline m128 zero()
{
	m128 m = _mm_setzero_si128();
	m = _mm_set1_epi16(0x1337);
	return m;
}

int main(int argc, char* argv[])
{
	std::cerr << "uint32: " << sizeof(uint32_t) << std::endl;
	std::cerr << "uint64: " << sizeof(uint64_t) << std::endl;
	std::cerr << "void*: " << sizeof(void*) << std::endl;
	std::cerr << "int: " << sizeof(int) << std::endl;
	std::cerr << "m128: " << sizeof(m128) << std::endl;
	
	std::cerr << zero() << std::endl;
	
	return 0;
}
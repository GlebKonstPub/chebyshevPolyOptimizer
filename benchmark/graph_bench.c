#include <stdio.h>				// printf

#ifdef _MSC_VER
#	include <intrin.h>			// __rdtsc
#else
#	include <x86intrin.h>		// __rdtsc
#endif
#include <immintrin.h>			// _MM_SET_FLUSH_ZERO_MODE(SSE), _MM_SET_DENORMALS_ZERO_MODE(SSE3)

// #define CPU_FEATURES_PRINT
#include "x86_cpu_features.h"	// runtime check for SSE, SSE3
#define ChebPolySum_IMPLEMENTATION
#include "../chebPolySum.h"		// Target functions

#include <stdint.h>
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef int8_t		i8;
typedef int16_t		i16;
typedef int32_t		i32;
typedef int64_t		i64;
typedef float		f32;
typedef double		f64;

#ifndef max
#	define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define f64_u64_max (16.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 - 1.0)
// 2^64 - 1, -1 will be less then LSB here, so it doesn't really matter
inline u64 safe_f64_to_u64(f64 x) {
	if (x < 0.0) return 0;
	if (x >= f64_u64_max) return UINT64_MAX;
	return (u64)x;
}

typedef struct {
	u64 cycles;
	u64 iterations;
	f64 cyclesPerIter;
} BenchResult;

f64 bench (u64 bufferLength, u64* cycles) {
	BenchResult result = {0};
	BenchResult resultNoIntrin = {0};
	BenchResult resultIntrin = {0};
	u64 c0, c1;
	u64 N = 1000;

	u64 alignedBufLen = (bufferLength + 3) & ~3; // multiple of 4 (sizeof(__m256d) / sizeof(f64))
	#ifdef _MSC_VER //msvc specific aligned alloc
		f64* buffer = (f64*)_aligned_malloc(alignedBufLen * 3 * sizeof(f64), 32);
	#else
		f64* buffer = (f64*)aligned_alloc(32, alignedBufLen * 3 * sizeof(f64));
	#endif

	u64 scrap_buf_size = max(bufferLength * sizeof(f64) * 2, 16 * 1024); // 16kB, big enough not to be optimized out, small enough to fit L1 cache
	f64* scrap = malloc(scrap_buf_size);
	if (!scrap || !buffer) {
		printf ("Download more RAM\n");
		return -1;
	}
	for (u64 i = 0; i < scrap_buf_size / sizeof(f64); i++) scrap[i] = (f64)i;
	u64 f64PerOp = bufferLength * 2;
	u64 f64PerScrapBuffer = scrap_buf_size / sizeof(f64);
	u64 singleOpLength = f64PerOp * sizeof(f64);
	u64 opsPerScrapBuffer = scrap_buf_size / singleOpLength;
	if (!opsPerScrapBuffer) { // basically assert
		printf("f64PerOp = %lld\n", f64PerOp);
		printf("scrap_buf_size = %lld\n", scrap_buf_size);
		printf("singleOpLength = %lld\n", singleOpLength);
		return -1;
	}

	{
		u64 i = 0;
		c0 = __rdtsc();
		while (i < N) {
			u64 remaining = N - i;
			u64 ops = (remaining < opsPerScrapBuffer) ? remaining : opsPerScrapBuffer;
			for (u64 j = 0; j < ops; j++)
				chebPolySum_naive(bufferLength,
							&scrap[j * f64PerOp],
							&scrap[j * f64PerOp + bufferLength],
							1.0);
			i += ops;
		}
		c1 = __rdtsc();

		result.cycles			= c1 - c0;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
	}
	resultNoIntrin = result;

	{
		u64 i = 0;
		c0 = __rdtsc();
		while (i < N) {
			u64 remaining = N - i;
			u64 ops = (remaining < opsPerScrapBuffer) ? remaining : opsPerScrapBuffer;
			for (u64 j = 0; j < ops; j++)
				chebPolySum_noAlloc(bufferLength,
							&scrap[j * f64PerOp],
							&scrap[j * f64PerOp + bufferLength],
							1.0,
							buffer);
			i += ops;
		}
		c1 = __rdtsc();

		result.cycles			= c1 - c0;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
	}
	resultIntrin = result;
	*cycles = result.cycles;
	return resultNoIntrin.cyclesPerIter / resultIntrin.cyclesPerIter;
}

int main() {
	CpuFeatures cpu = cpu_detect();
	/* cpu_features_print(&cpu);
	* CpuCacheInfo cpuCache = cpu_cache_detect();
	* cpu_cache_print(&cpuCache);
	* return 0;
	*/
	if (cpu.sse) _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	if (cpu.sse3) _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	if (!cpu.avx2) printf("No AVX2 found on processor. Why are we here?\n");

	FILE *out;
	errno_t err = fopen_s(&out, "output.txt", "w");
	if (err) {
		printf("got wrong!\n");
		return -1;
	}
	for (u64 i = 1; i <= 1500; i++) {
		u64 cycles = 0;
		f64 t = bench(i, &cycles);
		cycles /= 1000;
		fprintf(out, "%llu, %f, %llu\n", i, t, cycles);
		printf("%llu, %f, %llu\n", i, t, cycles);
		fflush(out);
	}
	fclose(out);
	return 0;
}
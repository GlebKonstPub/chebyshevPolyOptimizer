#include <stdio.h>				// printf

#ifdef _MSC_VER
#	include <intrin.h>			// __rdtsc
#else
#	include <x86intrin.h>		// __rdtsc
#endif
#include <immintrin.h>			// _MM_SET_FLUSH_ZERO_MODE(SSE), _MM_SET_DENORMALS_ZERO_MODE(SSE3)

#ifdef _WIN32
#	include <windows.h>			// QueryPerformanceCounter
#else
#	include <time.h>			// timespec
#endif

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

// -------------- Crossplatform time ---------------

typedef struct { u64 value; } Timestamp;

static Timestamp timestamp_now(void) {
	Timestamp t;
#ifdef _WIN32
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	t.value = (u64)li.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	t.value = (u64)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
	return t;
}

static f64 timestamp_freq(void) {
#ifdef _WIN32
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return (f64)li.QuadPart;
#else
	return 1e9; // clock_gettime in nanosecs
#endif
}

// ----------------- Statistics -------------------

typedef struct {
	u64 cycles;
	f64 seconds;
	u64 iterations;
	f64 cyclesPerIter;
} BenchResult;

// --------------- Benchmark ---------------

#define PRE_WARMUP_ITERS	10000000ULL
#define WARMUP_SECS			10.0
#define TARGET_SECS			60.0
// #include <stdlib.h>
int main (int argc, char** argv) {
	CpuFeatures cpu = cpu_detect();
	// cpu_features_print(&cpu);
	// CpuCacheInfo cpuCache = cpu_cache_detect();
	// cpu_cache_print(&cpuCache);
	// system("Pause");
	// return 0;
	if (cpu.sse) _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	if (cpu.sse3) _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	if (!cpu.avx2) printf("No AVX2 found on processor. Why are we here?\n");

	printf("Benchmark compares naive chebPolySum function with handwritten AVX2 version.\n");
	printf("It will run for approx 2 mins. You can call it specifiying size of your polynom like this \"AVX2_Benchmark.exe 71\"\n");
	BenchResult result = {0};
	BenchResult resultNoIntrin = {0};
	BenchResult resultIntrin = {0};
	f64 freq = timestamp_freq();
	Timestamp t0, t1;
	u64 N, c0, c1;
	u64 bufferLength = 32;
	if (argc > 1) {
		u64 temp = atoll(argv[1]);
		if (temp > 0) {
			bufferLength = temp;
		}
	}
	printf("Buffer size: %llu.\n", bufferLength);
	#ifdef __AVX2__
		u64 alignedBufLen = (bufferLength + 3) & ~3; // multiple of 4 (sizeof(__m256d) / sizeof(f64))
		#ifdef _MSC_VER //msvc specific aligned alloc
			f64* buffer = (f64*)_aligned_malloc(alignedBufLen * 3 * sizeof(f64), 32);
		#else
			f64* buffer = (f64*)aligned_alloc(32, alignedBufLen * 3 * sizeof(f64));
		#endif
	#else
		f64* buffer = (f64*)malloc(bufferLength * 3 * sizeof(f64));
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

	printf("%-20s %15s %15s %15s\n", "Function", "Time (s)", "Iterations", "Cycles/iter");
	printf("%-20s %15s %15s %15s\n", "--------------------", "---------------", "---------------", "---------------");

	for (u64 run = 0; run < 3; run++) {
		switch (run) {
			case 0:
				N = PRE_WARMUP_ITERS;
				if (bufferLength > 32) {
					N = safe_f64_to_u64((f64)N * 32.0 * 32.0 / (f64)(bufferLength * bufferLength));
				}
			break;
			case 1:
				N = safe_f64_to_u64((WARMUP_SECS / result.seconds) * (f64)N);
			break;
			default:
				N = safe_f64_to_u64(((TARGET_SECS - result.seconds) / result.seconds) * (f64)N);
			break;
		}

		u64 i = 0;
		t0 = timestamp_now();
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
		t1 = timestamp_now();

		result.cycles			= c1 - c0;
		result.seconds			= (t1.value - t0.value) / freq;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
		printf("%-20s %15.4f %15llu %15.2f\n",
				"chebPolySum_naive",
				result.seconds,
				result.iterations,
				result.cyclesPerIter);
	}
	resultNoIntrin = result;

	for (u64 run = 0; run < 3; run++) {
		switch (run) {
			case 0:
				N = PRE_WARMUP_ITERS;
				if (bufferLength > 32) {
					N = safe_f64_to_u64((f64)N * 32.0 * 32.0 / (f64)(bufferLength * bufferLength));
				}
			break;
			case 1:
				N = safe_f64_to_u64((WARMUP_SECS / result.seconds) * (f64)N);
			break;
			default:
				N = safe_f64_to_u64(((TARGET_SECS - result.seconds) / result.seconds) * (f64)N);
			break;
		}

		u64 i = 0;
		t0 = timestamp_now();
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
		t1 = timestamp_now();

		result.cycles			= c1 - c0;
		result.seconds			= (t1.value - t0.value) / freq;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
		printf("%-20s %15.4f %15llu %15.2f\n",
				"chebPolySum",
				result.seconds,
				result.iterations,
				result.cyclesPerIter);
	}
	resultIntrin = result;

	printf("\nFinal results:\n");
	printf("%-20s %15s %15s %15s\n", "Function", "Time (s)", "Iterations", "Cycles/iter");
	printf("%-20s %15s %15s %15s\n", "--------------------", "---------------", "---------------", "---------------");
	printf("%-20s %15.4f %15llu %15.2f\n",
			"chebPolySum_naive",
			resultNoIntrin.seconds,
			resultNoIntrin.iterations,
			resultNoIntrin.cyclesPerIter);
	printf("%-20s %15.4f %15llu %15.2f\n",
			"chebPolySum",
			resultIntrin.seconds,
			resultIntrin.iterations,
			resultIntrin.cyclesPerIter);
	printf("\nSpeedup: %.2fx\n", resultNoIntrin.cyclesPerIter / resultIntrin.cyclesPerIter);
	return 0;
}
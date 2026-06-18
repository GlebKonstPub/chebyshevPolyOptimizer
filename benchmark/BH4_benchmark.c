#include <stdio.h>				// printf

#ifdef _MSC_VER
	#include <intrin.h>			// __rdtsc
#else
	#include <x86intrin.h>		// __rdtsc
#endif

#include "x86_cpu_features.h"	// runtime check for SSE, SSE3
#include <immintrin.h>			// _MM_SET_FLUSH_ZERO_MODE(SSE), _MM_SET_DENORMALS_ZERO_MODE(SSE3)

#ifdef _WIN32
	#include <windows.h>		// QueryPerformanceCounter
#else
	#include <time.h>			// timespec
#endif

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

// --------------- Target functions ---------------

#define _USE_MATH_DEFINES	// M_PI
#include <math.h>			// cos

inline f64 BH4 (f64 x) {
	// https://www.mathworks.com/matlabcentral/mlc-downloads/downloads/submissions/46092/versions/3/previews/coswin.m/index.html
	// % 4 Term Blackman-Harris window, 92 dB
	return 0.358750287312166
			- 0.4882901074726   * cos(2.0 * M_PI * x)
			+ 0.141279712970519 * cos(4.0 * M_PI * x)
			- 0.011679892244715 * cos(6.0 * M_PI * x);
}

inline f64 optimizedBH4 (f64 x) {
	const f64 c = cos(2.0 * M_PI * x);
	return ((( -0.04671956897886000298
				* c + 0.28255942594103800047 )
				* c - 0.45325043073845500130 )
				* c + 0.21747057434164701606 );
}

// --------------- Benchmark ---------------

#define PRE_WARMUP_ITERS	10000000ULL
#define WARMUP_SECS			10.0
#define TARGET_SECS			60.0

int main () {
	CpuFeatures cpu = cpu_detect();
	if (cpu.sse) _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	if (cpu.sse3) _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	BenchResult result = {0};
	BenchResult resultBH4 = {0};
	BenchResult resultOptBH4 = {0};
	f64 freq = timestamp_freq();
	Timestamp t0, t1;
	u64 N, c0, c1;
	f64 sink = 0.0;
	u64 scrap_buf_size = 1024; // big enough not to be optimized out, small enough to fit L1 cache
	f64* scrap = (f64*)malloc(scrap_buf_size * sizeof(f64));
	if (!scrap) {
		printf ("Download more RAM\n");
		return -1;
	}
	for (u64 i = 0; i < scrap_buf_size; i++) scrap[i] = (f64)i;

	printf("Benchmark compares classic Blackman-Harris window function with it's Chebyshev optimized version.\n");
	printf("It will run for approx 2 mins.\n");
	printf("%-15s %15s %15s %15s\n", "Function", "Time (s)", "Iterations", "Cycles/iter");
	printf("%-15s %15s %15s %15s\n", "---------------", "---------------", "---------------", "---------------");

	for (u64 run = 0; run < 3; run++) {
		switch (run) {
			case 0:
				N = PRE_WARMUP_ITERS;
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
			u64 chunk = (remaining < scrap_buf_size) ? remaining : scrap_buf_size;
			for (u64 j = 0; j < chunk; j++) sink += BH4(scrap[j]);
			i += chunk;
		}
		c1 = __rdtsc();
		t1 = timestamp_now();

		result.cycles			= c1 - c0;
		result.seconds		= (t1.value - t0.value) / freq;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
		printf("%-15s %15.4f %15llu %15.2f\n",
				"BH4",
				result.seconds,
				result.iterations,
				result.cyclesPerIter);
	}
	resultBH4 = result;

	for (u64 run = 0; run < 3; run++) {
		switch (run) {
			case 0:
				N = PRE_WARMUP_ITERS;
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
			u64 chunk = (remaining < scrap_buf_size) ? remaining : scrap_buf_size;
			for (u64 j = 0; j < chunk; j++) sink += optimizedBH4(scrap[j]);
			i += chunk;
		}
		c1 = __rdtsc();
		t1 = timestamp_now();

		result.cycles			= c1 - c0;
		result.seconds			= (t1.value - t0.value) / freq;
		result.iterations		= N;
		result.cyclesPerIter	= (f64)result.cycles / (f64)N;
		printf("%-15s %15.4f %15llu %15.2f\n",
				"optimizedBH4",
				result.seconds,
				result.iterations,
				result.cyclesPerIter);
	}
	resultOptBH4 = result;

	printf("\nFinal results:\n");
	printf("%-15s %15s %15s %15s\n", "Function", "Time (s)", "Iterations", "Cycles/iter");
	printf("%-15s %15s %15s %15s\n", "---------------", "---------------", "---------------", "---------------");
	printf("%-15s %15.4f %15llu %15.2f\n",
			"BH4",
			resultBH4.seconds,
			resultBH4.iterations,
			resultBH4.cyclesPerIter);
	printf("%-15s %15.4f %15llu %15.2f\n",
			"optimizedBH4",
			resultOptBH4.seconds,
			resultOptBH4.iterations,
			resultOptBH4.cyclesPerIter);
	printf("\nSpeedup: %.2fx\n", resultBH4.cyclesPerIter / resultOptBH4.cyclesPerIter);
	printf("(checksum: %f)\n", sink);
	return 0;
}
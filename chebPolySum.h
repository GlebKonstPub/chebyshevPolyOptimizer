#include <stddef.h> // size_t
void chebCosSum					(size_t size, const double* inputCoeffs, double* outputCoeffs);
void chebSinSum					(size_t size, const double* inputCoeffs, double* outputCoeffs);
void chebPolySum				(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind);
void chebPolySum_noAlloc		(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind, double* buffer);
void chebPolySum_naive			(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind);
void chebPolySum_naive_noAlloc	(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind, double* buffer);

#ifdef ChebPolySum_IMPLEMENTATION

#if defined(_MSC_VER) && !defined(__clang__) && defined(__cplusplus)
#	define INLINE_CALL [[msvc::forceinline_calls]]
#else
#	define INLINE_CALL
#endif

#ifdef __AVX2__
#	include <immintrin.h>
#endif

inline void chebCosSum(size_t size, const double* inputCoeffs, double* outputCoeffs) {
	INLINE_CALL chebPolySum(size, inputCoeffs, outputCoeffs, 1.0);
}

inline void chebSinSum(size_t size, const double* inputCoeffs, double* outputCoeffs) {
	INLINE_CALL chebPolySum(size, inputCoeffs, outputCoeffs, 2.0);
}

inline void chebPolySum(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind) {
	const size_t buf_size = (size + 3ull) & ~3ull; // Rounding up size to the next multiple of 4 (sizeof(__m256d) / sizeof(double))
	#ifdef __AVX2__
		#ifdef _MSC_VER //msvc specific aligned alloc
			double* buffer = (double*)_aligned_malloc(buf_size * 3 * sizeof(double), 32);
		#else
			double* buffer = (double*)aligned_alloc(32, buf_size * 3 * sizeof(double));
		#endif
	#else
		double* buffer = (double*)malloc(buf_size * 3 * sizeof(double));
	#endif
	chebPolySum_noAlloc(size, inputCoeffs, outputCoeffs, kind, buffer);
	#if defined(_MSC_VER) && defined(__AVX2__) //msvc specific aligned alloc
		_aligned_free(buffer);
	#else
		free(buffer);
	#endif
	return;
}

void chebPolySum_noAlloc(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind, double* buffer) {
	if (size < 1) return;
	for (size_t i = 0; i < size; i++) {
		outputCoeffs[i] = 0;
	}
	outputCoeffs[0] = inputCoeffs[0];
	if (size < 2) return;
	outputCoeffs[1] = inputCoeffs[1] * kind;

	const size_t buf_size = (size + 3ull) & ~3ull;
	for (size_t i = 0; i < buf_size * 3; i++) {
		buffer[i] = 0.0;
	}
	double* prevprevC = buffer;
	double* prevC = &buffer[buf_size];
	double* C = &buffer[buf_size * 2];

	prevprevC[0] = 1.0;
	prevC[1] = kind; // 1.0 for cos, 2.0 for sin

	#ifdef __AVX2__
		const __m256d two_vec = _mm256_set1_pd(2.0);
	#endif

	for (size_t i = 2; i < size; i++) {
		// Making Chebyshev calculations step
		size_t j = 1;
		#ifdef __AVX2__
			if (size >= 4) {
				// Perform the Chebyshev calculation: C[j] = 2.0 * prevC[j-1] - prevprevC[j]
				for (; j <= size - 4; j += 4) {
					__m256d prevC_vec = _mm256_load_pd(&prevC[j - 1]);
					__m256d prevprevC_vec = _mm256_loadu_pd(&prevprevC[j]);
					__m256d result = _mm256_fmsub_pd(two_vec, prevC_vec, prevprevC_vec);
					_mm256_storeu_pd(&C[j], result);
				}
			}
		#endif
		// Fallback (scalar) implementation for remaining elements or when AVX2 is not enabled
		for (; j < size; j++) {
			C[j] = 2.0 * prevC[j - 1] - prevprevC[j];
		}
		C[0] = -prevprevC[0];

		// Accumulation
		j = 0;
		#ifdef __AVX2__
			if (size >= 4) {
				__m256d input_vec = _mm256_set1_pd(inputCoeffs[i]);
				// outputCoeffs[j] += C[j] * inputCoeffs[i]
				for (; j <= size - 4; j += 4) {
					__m256d output_vec = _mm256_loadu_pd(&outputCoeffs[j]);
					__m256d C_vec = _mm256_load_pd(&C[j]);
					output_vec = _mm256_fmadd_pd(C_vec, input_vec, output_vec);
					_mm256_storeu_pd(&outputCoeffs[j], output_vec);
				}
			}
		#endif
		// Fallback (scalar) implementation for remaining elements or when AVX2 is not enabled
		for (; j < size; j++) {
			outputCoeffs[j] += C[j] * inputCoeffs[i];
		}
		// Revolve buffer
		double* temp = prevprevC;
		prevprevC = prevC;
		prevC = C;
		C = temp;
	}
	return;
}

inline void chebPolySum_naive(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind) {
	double* buffer = (double*)malloc(size * 3 * sizeof(double));
	chebPolySum_naive_noAlloc(size, inputCoeffs, outputCoeffs, kind, buffer);
	free(buffer);
	return;
}

void chebPolySum_naive_noAlloc(size_t size, const double* inputCoeffs, double* outputCoeffs, double kind, double* buffer) {
	if (size < 1) return;
	for (size_t i = 0; i < size; i++) {
		outputCoeffs[i] = 0;
	}
	outputCoeffs[0] = inputCoeffs[0];
	if (size < 2) return;
	outputCoeffs[1] = inputCoeffs[1] * kind;
	for (size_t i = 0; i < size * 3; i++) {
		buffer[i] = 0.0;
	}
	double* prevprevC = buffer;
	double* prevC = &buffer[size];
	double* C = &buffer[size * 2];
	prevprevC[0] = 1.0;
	prevC[1] = kind; // 1.0 for cos, 2.0 for sin
	for (size_t i = 2; i < size; i++) {
		// Making Chebyshev calculations step
		for (size_t j = 1; j < size; j++) {
			C[j] = 2.0 * prevC[j - 1] - prevprevC[j];
		}
		C[0] = -prevprevC[0];
		// Accumulation
		for (size_t j = 0; j < size; j++) {
			outputCoeffs[j] += C[j] * inputCoeffs[i];
		}
		// Revolve buffer
		double* temp = prevprevC;
		prevprevC = prevC;
		prevC = C;
		C = temp;
	}
	return;
}

#endif //ChebPolySum_IMPLEMENTATION
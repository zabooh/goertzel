#ifndef __FFT32_H
#define __FFT32_H


#define FFT_LENGTH      1024

#define __USE_FLOAT

#ifdef __USE_FLOAT
#define FFT_NU_TYPE	float
#define FFT_Q31X_FRMULT(a,b) a * b
#define FFT_MULT(a,b) FFT_Q31X_FRMULT(a,b)
#define FFT_Q31X32(x) x
#define FFT_Q31X32_TO_FLOAT(y) y
#define FFT_Q31X32_PRECISION 0
#else
#define FFT_NU_TYPE	int32_t
#define FFT_Q31X32_PRECISION 4
#define FFT_Q31X32_DECIMAL_POINT_POSITION ((32-2) - FFT_Q31X32_PRECISION)
#define FFT_Q31X32_FRMULT(a,b) \
    ( \
        ( \
            (int32_t)( \
            ((int64_t)a * (int64_t)b)>>32) \
        ) << (2 + FFT_Q31X32_PRECISION) \
    )
#define FFT_MULT(a,b) FFT_Q31X32_FRMULT(a,b)
#define FFT_Q31X32(X) \
    ( \
        (X < 0.0) ? \
            (int32_t) ((1 << FFT_Q31X32_DECIMAL_POINT_POSITION)*(X) - 0.5) \
        : \
            (int32_t) ((1 << FFT_Q31X32_DECIMAL_POINT_POSITION)*(X) + 0.5) \
    )

#define FFT_Q31X32_TO_FLOAT(y)  ((float)y / (float)(1 << FFT_Q31X32_DECIMAL_POINT_POSITION))
#endif



#define TWO(x)  (1 << (x))

typedef struct {
    FFT_NU_TYPE x;
    FFT_NU_TYPE y;
} FFT_complex;

uint32_t FFT_32BitInplace_GetLength(void);
int32_t * FFT_32BitInplace(uint16_t *Sample, uint32_t n_data);
void FFT_32BitInplace_Calculate(void);
int32_t FTT_GetSampleData(uint32_t Index);

void FFT_fft(int N, FFT_complex *X);
void FFT_shuffle(int N, FFT_complex *X); /* \(N\) must be a power of 2 */
void FFT_dftmerge(int N, FFT_complex *XF);
int FFT_bitrev(int n, int B);

void FFT_swap(FFT_complex *a, FFT_complex *b);

void Test_FFT(uint32_t uSampleRate);

FFT_NU_TYPE alpha_max_plus_beta_min(FFT_NU_TYPE x, FFT_NU_TYPE y);

#endif

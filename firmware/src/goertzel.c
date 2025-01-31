
#include <math.h>
#include "goertzel.h"



int32_t iIIR_Tap[N_FILTER];

/*
 *  Initialize integer-based Goertzel filter
 * 
 */
void Goertzel_i_Init(float samplerate, float frequency, GOERTZEL_i_COEFF *coeff) {
    float omega = 2.0f * M_PI * frequency / samplerate;
    float cos_term = cosf(omega);
    float sin_term = sinf(omega);

    // Convert floating-point to fixed-point
    coeff->cos_term = (int32_t) (cos_term * FIXED_POINT_SCALE);
    coeff->sin_term = (int32_t) (sin_term * FIXED_POINT_SCALE);
    coeff->coeff = (int32_t) (2.0f * cos_term * FIXED_POINT_SCALE);

    // Initialize delay elements
    coeff->q0 = 0;
    coeff->q1 = 0;
    coeff->q2 = 0;
    coeff->counter = 0;

    coeff->goertzel_threshold = GOERTZEL_THRESHOLD;
    coeff->max_amplitude = 0;
}

/*
 * Integer-based Goertzel filter implementation
 * Processing time for 512 samples is 4 milli seconds
 * 
 */
void __attribute__((optimize("-O1"))) Goertzel_i_Filter(uint16_t *data_input, int32_t *data_output, GOERTZEL_i_COEFF *coeff) {
    static int32_t down_counter = 0;
    int32_t data_sample;

    for (int i = 0; i < BLOCK_SIZE; i++) {
        // Convert input data and reduce is by factor 4
        data_sample = ((int32_t) (data_input[i]) >> 2);

        // Update max amplitude if necessary
        if (coeff->max_amplitude < data_sample) coeff->max_amplitude = data_sample;

        // Scale the sample for fixed-point arithmetic
        int32_t sample = (data_sample) << (FIXED_POINT_BITS - 2);

        // Apply damping factor
        const int32_t DAMPING = (int32_t) (DAMPING_FACTOR * FIXED_POINT_SCALE);
        int64_t temp = ((int64_t) coeff->coeff * coeff->q1 * DAMPING) >> FIXED_POINT_BITS;

        // Goertzel algorithm core calculations
        coeff->q0 = sample + (int32_t) (temp >> FIXED_POINT_BITS) - coeff->q2;
        // apply optimized damping (  A = B - (B >> 8) = approx. 0.99609375)  
        coeff->q2 = coeff->q1 - (coeff->q1 >> 8);
        coeff->q1 = coeff->q0 - (coeff->q0 >> 8);

        // Calculate real and imaginary parts
        int64_t temp2 = (int64_t) coeff->q2 * coeff->cos_term;
        int32_t real = coeff->q1 - ((temp2) >> FIXED_POINT_BITS);
        temp2 = (int64_t) coeff->q2 * coeff->sin_term;
        int32_t imag = (temp2) >> FIXED_POINT_BITS;

        // Approximate magnitude calculation in fixed-point
        int32_t abs_real = real < 0 ? -real : real;
        int32_t abs_imag = imag < 0 ? -imag : imag;
        int32_t magnitude = abs_real + abs_imag - ((abs_real < abs_imag ? abs_real : abs_imag) >> 1);

        // Convert back and store result
        int32_t temp3 = iFLT_IIR1_Lowpass(0,magnitude >> FIXED_POINT_BITS);
        data_output[i] = temp3;

        // Periodic reset of accumulators
        if (down_counter++ == WINDOW_SIZE) {
            down_counter = 0;
            if (temp3 > coeff->goertzel_threshold) {
                coeff->q0 = 0;
                coeff->q1 = 0;
                coeff->q2 = 0;
                coeff->counter++;
                FLT_vIIR_Init();
            }
        }
    }
}

/*
 * Initialize float-based Goertzel filter
 * Processing time for 512 samples is 22.9 milli seconds
 * This is about 5.5 times more than the fixed point Goertzel 
 * 
 */
void Goertzel_f_Init(float samplerate, float frequency, GOERTZEL_f_COEFF *coeff) {
    float omega = 2.0f * M_PI * frequency / samplerate;
    coeff->cos_term = cosf(omega);
    coeff->sin_term = sinf(omega);

    // Calculate coefficient
    coeff->coeff = 2.0f * coeff->cos_term;

    // Initialize q-values
    coeff->q0 = 0.0f;
    coeff->q1 = 0.0f;
    coeff->q2 = 0.0f;

    coeff->goertzel_threshold = GOERTZEL_THRESHOLD;
    coeff->max_amplitude = 0;
}

/* 
 * Float-based Goertzel filter implementation
 * 
 */
void __attribute__((optimize("-O1"))) Goertzel_f_Filter(uint16_t *data_input, int32_t *data_output, GOERTZEL_f_COEFF *coeff) {
    static int32_t down_counter = 0;
    int32_t data_sample;

    for (int i = 0; i < BLOCK_SIZE; i++) {
        // Convert ADC value to float and reduce is by factor 4
        data_sample = ((int32_t) (data_input[i]) >> 2);        
        float sample = (float) data_sample;

        // Goertzel iteration
        // the DAMPING_FACTOR allows to reduce the internal values when the signal is not present
        // otherwise the magnitude would accumulate allways  
        coeff->q0 = sample + (coeff->coeff * coeff->q1 * DAMPING_FACTOR) - coeff->q2;
        coeff->q2 = coeff->q1 * DAMPING_FACTOR;
        coeff->q1 = coeff->q0 * DAMPING_FACTOR;

        // Calculate magnitude
        float real = coeff->q1 - (coeff->q2 * coeff->cos_term);
        float imag = coeff->q2 * coeff->sin_term;
        uint32_t magnitude = fabsf(real) + fabsf(imag) - fminf(fabsf(real), fabsf(imag)) / 2.0f;

        // Store result
        magnitude = iFLT_IIR1_Lowpass(0,magnitude >> FIXED_POINT_BITS);
        data_output[i] = (uint32_t) magnitude;

        // Periodic reset of accumulators
        if (down_counter++ == WINDOW_SIZE) {
            down_counter = 0;
            if (magnitude > coeff->goertzel_threshold) {
                coeff->q0 = 0.0f;
                coeff->q1 = 0.0f;
                coeff->q2 = 0.0f;
                FLT_vIIR_Init();
            }
        }

    }
}

/**
 * @brief Initialization of IIR Filter Taps
 */
void FLT_vIIR_Init(void) {
    for (int ix = 0; ix < N_FILTER; ix++) {
        iIIR_Tap[ix] = 0;
    }
}

/*
 * IIR Lowpass with one Pol 
 */
int32_t __attribute__((optimize("-O1"))) iFLT_IIR1_Lowpass(int32_t Filter, int32_t iX) {
    /*
     * NORMALIZED BANDWIDTH AND
     * RISE TIME FOR VARIOUS VALUES OF k
     *  k   Bandwidth          Rise time
     *      (normalized        (samples)
     *      to 1 Hz)
     * ========================================
     *  1   0.1197              3
     *  2   0.0466              8
     *  3   0.0217              16
     *  4   0.0104              34
     *  5   0.0051              69
     *  6   0.0026              140
     *  7   0.0012              280
     *  8   0.0007              561
     * 
     *                  1
     *  H(z) = -----------------------
     *         1 - ( 1 - 2^-K ) * z^-1  
     * 
     *   z = 1 ? 2^?4 = 0.9375
     * 
     *  This pole lies within the unit circle, which guarantees the stability of the filter.
     * 
     * This filter is good for smoothing noisy data
     * 
     */
#define LP_SHIFT_VALUE_K   2

    int32_t iTap;

    iTap = iIIR_Tap[Filter];
    iTap = iTap - (iTap >> LP_SHIFT_VALUE_K) + iX;
    iIIR_Tap[Filter] = iTap;
    
    return (iTap >> LP_SHIFT_VALUE_K);

}


int32_t __attribute__((optimize("-O1"))) iFLT_IIR1_Highpass(int32_t Filter, int32_t iX) {
    int32_t iTap; 
    int32_t iY;
    
    /*
     *  one Zero at 1. This surpresses the DC
     *  with higher K, the bandwidth is reduced towards higher frequencies
     * 
     *               1 - z^-1
     *  H(z) = -----------------------
     *         1 - ( 1 - 2^-K ) * z^-1  
     * 
     */

#define HP_SHIFT_VALUE_K 4
    
    iTap = iIIR_Tap[Filter];
    iTap = iTap - (iTap >> HP_SHIFT_VALUE_K) + iX;
    iIIR_Tap[Filter] = iTap;

    // Highpass Calculation
    iY = iX - (iTap >> HP_SHIFT_VALUE_K);
    
    return (iY);
}





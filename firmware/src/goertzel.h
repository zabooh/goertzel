#include <stdint.h>

// Define constants
#define BLOCK_SIZE            512    // Size of the data block to process
#define SAMPLERATE          50000    // Sampling rate in Hz
#define FREQUENCY           10000    // Target frequency to detect
#define GOERTZEL_THRESHOLD   2000    // Threshold for detection
#define DAMPING_FACTOR      0.995    // Damping factor to prevent overflow
#define WINDOW_SIZE             1    // Window size for resetting accumulators

// Fixed-point arithmetic definitions
#define FIXED_POINT_BITS 16
#define FIXED_POINT_SCALE (1 << FIXED_POINT_BITS)

// Structure for integer-based Goertzel algorithm coefficients
typedef struct {
    int32_t coeff;               // Fixed-point coefficient
    int32_t cos_term;            // Fixed-point cosine term
    int32_t sin_term;            // Fixed-point sine term
    int32_t q0;                  // Fixed-point delay elements
    int32_t q1;
    int32_t q2;
    int32_t counter;             // Counter for detections
    int32_t goertzel_threshold;  // Detection threshold
    int32_t max_amplitude;       // Maximum amplitude encountered
} GOERTZEL_i_COEFF;

// Structure for float-based Goertzel algorithm coefficients
typedef struct {
    float coeff;
    float cos_term;
    float sin_term;
    float q0;
    float q1;
    float q2;
    int32_t goertzel_threshold;  // Detection threshold
    int32_t max_amplitude;       // Maximum amplitude encountered    
} GOERTZEL_f_COEFF;

// Function prototypes
void Goertzel_i_Init(float samplerate, float frequency, GOERTZEL_i_COEFF *coeff);
void Goertzel_i_Filter(uint16_t *data_input, int32_t *data_output, GOERTZEL_i_COEFF *coeff);

void Goertzel_f_Init(float samplerate, float frequency, GOERTZEL_f_COEFF *coeff);
void Goertzel_f_Filter(uint16_t *data_input, int32_t *data_output, GOERTZEL_f_COEFF *coeff);


uint32_t uiFLT_IIR1_Lowpass(uint32_t filter, uint32_t x);
void FLT_vIIR_Init(void);

#define N_FILTER 1


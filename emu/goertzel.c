

/**************************************************************

    gcc -std=c11 -Wall -pedantic goertzel.c -o goertzel

**************************************************************/


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define BLOCK_SIZE 512
#define SAMPLERATE 104000
#define FREQUENCY 5000
#define MAX_VALUE 1023

#define FIXED_POINT_FRACTIONAL_BITS 25
#define BLOCK_SIZE 512

typedef struct {
    int32_t coeff;
    int32_t cos_term;
    int32_t sin_term;
    int32_t q0;
    int32_t q1;
    int32_t q2;
} GOERTZEL_COEFF;



void Goertzel_Init(float samplerate, float frequency, GOERTZEL_COEFF *coeff) {
    float omega = 2.0f * M_PI * frequency / samplerate;
    float cosine = cosf(omega);
    float sine = sinf(omega);
    
    // Berechne Koeffizienten in Fließkomma
    float float_coeff = 2.0f * cosine;
    
    // Konvertiere zu Festkomma
    coeff->coeff = (int32_t)(float_coeff * (1 << FIXED_POINT_FRACTIONAL_BITS));
    coeff->cos_term = (int32_t)(cosine * (1 << FIXED_POINT_FRACTIONAL_BITS));
    coeff->sin_term = (int32_t)(sine * (1 << FIXED_POINT_FRACTIONAL_BITS));
    
    // Initialisiere q-Werte
    coeff->q0 = 0;
    coeff->q1 = 0;
    coeff->q2 = 0;
}



void Goertzel_Filter(uint32_t *data_input, uint32_t *data_output, GOERTZEL_COEFF *coeff) {
    // Initialisiere q-Werte
    coeff->q0 = 0;
    coeff->q1 = 0;
    coeff->q2 = 0;
    
    for (int i = 0; i < BLOCK_SIZE; i++) {
        // Konvertiere 10-Bit-ADC-Wert zu 32-Bit-Festkomma
        int32_t sample = data_input[i] << (FIXED_POINT_FRACTIONAL_BITS - 10);
        
        // Goertzel-Iteration
        coeff->q0 = sample + ((coeff->coeff * coeff->q1) >> FIXED_POINT_FRACTIONAL_BITS) - coeff->q2;
        coeff->q2 = coeff->q1;
        coeff->q1 = coeff->q0;
        
        // Berechne Magnitude für jedes Sample
        int32_t real = coeff->q1 - ((coeff->q2 * coeff->cos_term) >> FIXED_POINT_FRACTIONAL_BITS);
        int32_t imag = (coeff->q2 * coeff->sin_term) >> FIXED_POINT_FRACTIONAL_BITS;
        
        // Approximation des Betrags: |a| + |b| - min(|a|,|b|)/2
        int32_t abs_real = real < 0 ? -real : real;
        int32_t abs_imag = imag < 0 ? -imag : imag;
        int32_t magnitude = abs_real + abs_imag - ((abs_real < abs_imag ? abs_real : abs_imag) >> 1);
        
        // Speichere Magnitude für dieses Sample
        data_output[i] = magnitude >> (FIXED_POINT_FRACTIONAL_BITS - 10);
    }
}




int main(void) {
    uint32_t input_data[BLOCK_SIZE];
    uint32_t output_data[BLOCK_SIZE];
    GOERTZEL_COEFF coeff;
    FILE *file;

    // Initialisiere output_data mit 0
    memset(output_data, 0, sizeof(output_data));

    // Erzeuge 5kHz Sinussignal
    for (int i = 0; i < BLOCK_SIZE; i++) {
        double t = (double)i / SAMPLERATE;
        double signal = (sin(2 * M_PI * FREQUENCY * t) + 1) / 2;  // Normalisiere auf [0, 1]
        input_data[i] = (uint32_t)(signal * MAX_VALUE);  // Skaliere auf [0, 1023]
    }

    // Initialisiere Goertzel-Koeffizienten
    Goertzel_Init(SAMPLERATE, FREQUENCY, &coeff);

    // Führe Goertzel-Filter aus
    Goertzel_Filter(input_data, output_data, &coeff);

    // Speichere Ergebnis in output.txt
    file = fopen("output.txt", "w");
    if (file == NULL) {
        printf("Fehler beim Öffnen der Datei!\n");
        return 0;
    }
    for (int i = 0; i < BLOCK_SIZE; i++) {
        fprintf(file, "%04u\t%04u\n", input_data[i], output_data[i]);
    }
    fclose(file);

    printf("Goertzel-Filter ausgeführt. Ergebnisse in output.txt gespeichert.\n");

    return 0;
}


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define BLOCK_SIZE 10000
#define SAMPLERATE 104000
#define FREQUENCY 5000
#define MAX_VALUE 1023

#define FIXED_POINT_BITS 16
#define FIXED_POINT_SCALE (1 << FIXED_POINT_BITS)

typedef struct
{
    float coeff;
    float cos_term;
    float sin_term;
    float q0;
    float q1;
    float q2;
} GOERTZEL_COEFF;

void Goertzel_Init(float samplerate, float frequency, GOERTZEL_COEFF *coeff)
{
    float omega = 2.0f * M_PI * frequency / samplerate;
    coeff->cos_term = cosf(omega);
    coeff->sin_term = sinf(omega);

    // Berechne Koeffizienten
    coeff->coeff = 2.0f * coeff->cos_term;

    // Initialisiere q-Werte
    coeff->q0 = 0.0f;
    coeff->q1 = 0.0f;
    coeff->q2 = 0.0f;
}

void ___Goertzel_Filter(int32_t *data_input, int32_t *data_output, GOERTZEL_COEFF *coeff)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // Konvertiere ADC-Wert zu float
        float sample = (float)data_input[i];

        // Goertzel-Iteration
        coeff->q0 = sample + (coeff->coeff * coeff->q1) - coeff->q2;
        coeff->q2 = coeff->q1;
        coeff->q1 = coeff->q0;

        // Berechne Magnitude für jedes Sample
        float real = coeff->q1 - (coeff->q2 * coeff->cos_term);
        float imag = coeff->q2 * coeff->sin_term;

        // Berechne Magnitude
        float magnitude = fabsf(real) + fabsf(imag) - fminf(fabsf(real), fabsf(imag)) / 2.0f;

        // Konvertiere zurück zu uint32_t und speichere
        data_output[i] = (uint32_t)magnitude;
    }
}

void Goertzel_Filter(int32_t *data_input, int32_t *data_output, GOERTZEL_COEFF *coeff)
{
    // Fensterbreite für gleitende Berechnung
    const int WINDOW_SIZE = 256; // Anpassbar je nach gewünschter Reaktionszeit

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // Konvertiere ADC-Wert zu float
        float sample = (float)data_input[i];

        // Goertzel-Iteration
        coeff->q0 = sample + (coeff->coeff * coeff->q1) - coeff->q2;
        coeff->q2 = coeff->q1;
        coeff->q1 = coeff->q0;

        // Periodisches Reset der Akkumulatoren
        if (i % WINDOW_SIZE == WINDOW_SIZE - 1)
        {
            // Berechne Magnitude vor dem Reset
            float real = coeff->q1 - (coeff->q2 * coeff->cos_term);
            float imag = coeff->q2 * coeff->sin_term;
            float magnitude = fabsf(real) + fabsf(imag) - fminf(fabsf(real), fabsf(imag)) / 2.0f;

            // Speichere Ergebnis
            data_output[i] = (uint32_t)magnitude;

            // Reset der Akkumulatoren
            coeff->q0 = 0.0f;
            coeff->q1 = 0.0f;
            coeff->q2 = 0.0f;
        }
        else
        {
            // Berechne normale Magnitude
            float real = coeff->q1 - (coeff->q2 * coeff->cos_term);
            float imag = coeff->q2 * coeff->sin_term;
            float magnitude = fabsf(real) + fabsf(imag) - fminf(fabsf(real), fabsf(imag)) / 2.0f;

            data_output[i] = (uint32_t)magnitude;
        }
    }
}

typedef struct
{
    int32_t coeff;    // Festkomma Koeffizient
    int32_t cos_term; // Festkomma Cosinus Term
    int32_t sin_term; // Festkomma Sinus Term
    int32_t q0;       // Festkomma Verzögerungsglieder
    int32_t q1;
    int32_t q2;
} GOERTZEL_i_COEFF;

GOERTZEL_i_COEFF g_i_coeff;

void Goertzel_i_Init(float samplerate, float frequency, GOERTZEL_i_COEFF *coeff)
{
    float omega = 2.0f * M_PI * frequency / samplerate;
    float cos_term = cosf(omega);
    float sin_term = sinf(omega);

    // Konvertiere Fließkomma zu Festkomma
    coeff->cos_term = (int32_t)(cos_term * FIXED_POINT_SCALE);
    coeff->sin_term = (int32_t)(sin_term * FIXED_POINT_SCALE);
    coeff->coeff = (int32_t)(2.0f * cos_term * FIXED_POINT_SCALE);

    // Initialisiere Verzögerungsglieder
    coeff->q0 = 0;
    coeff->q1 = 0;
    coeff->q2 = 0;
}

void Goertzel_i_Filter(int32_t *data_input, int32_t *data_output, GOERTZEL_i_COEFF *coeff)
{
    const int WINDOW_SIZE = 256;

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // Konvertiere Eingangswert zu Festkomma
        int32_t sample = data_input[i] << (FIXED_POINT_BITS - 2);

        // Goertzel-Iteration in Festkomma

        // Verwende 64-bit für Zwischenberechnungen
        int64_t temp = (int64_t)coeff->coeff * coeff->q1;
        coeff->q0 = sample + (int32_t)(temp >> FIXED_POINT_BITS) - coeff->q2;
        // coeff->q0 = sample + ((coeff->coeff * coeff->q1) >> FIXED_POINT_BITS) - coeff->q2;

        coeff->q2 = coeff->q1;
        coeff->q1 = coeff->q0;

        // Periodisches Reset der Akkumulatoren
        if (i % WINDOW_SIZE == WINDOW_SIZE - 1)
        {
            // Berechne Magnitude in Festkomma
            int64_t temp = (int64_t)coeff->q2 * coeff->cos_term;
            int32_t real = coeff->q1 - ((temp) >> FIXED_POINT_BITS);
            temp = (int64_t)coeff->q2 * coeff->sin_term;
            int32_t imag = (temp) >> FIXED_POINT_BITS;

            // Approximation des Betrags in Festkomma
            int32_t abs_real = real < 0 ? -real : real;
            int32_t abs_imag = imag < 0 ? -imag : imag;
            int32_t magnitude = abs_real + abs_imag - ((abs_real < abs_imag ? abs_real : abs_imag) >> 1);

            // Konvertiere zurück und speichere
            data_output[i] = magnitude >> FIXED_POINT_BITS;

            // Reset der Akkumulatoren
            coeff->q0 = 0;
            coeff->q1 = 0;
            coeff->q2 = 0;
        }
        else
        {
            // Berechne normale Magnitude in Festkomma
            int64_t temp = (int64_t)coeff->q2 * coeff->cos_term;
            int32_t real = coeff->q1 - ((temp) >> FIXED_POINT_BITS);
            temp = (int64_t)coeff->q2 * coeff->sin_term;
            int32_t imag = (temp) >> FIXED_POINT_BITS;

            int32_t abs_real = real < 0 ? -real : real;
            int32_t abs_imag = imag < 0 ? -imag : imag;
            int32_t magnitude = abs_real + abs_imag - ((abs_real < abs_imag ? abs_real : abs_imag) >> 1);

            data_output[i] = magnitude >> FIXED_POINT_BITS;
        }
    }
}

typedef struct
{
    float a0, a1, a2; // Zähler-Koeffizienten
    float b1, b2;     // Nenner-Koeffizienten
    float x1, x2;     // Verzögerte Eingangswerte
    float y1, y2;     // Verzögerte Ausgangswerte
} BPF_COEFF;

void BPF_Init(BPF_COEFF *bpf, float fs, float f0, float bandwidth)
{
    float w0 = 2.0f * M_PI * f0 / fs;
    float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bandwidth * w0 / sinf(w0));

    // Berechne Filterkoeffizienten
    float norm = 1.0f / (1.0f + alpha);
    bpf->a0 = alpha * norm;
    bpf->a1 = 0.0f;
    bpf->a2 = -alpha * norm;
    bpf->b1 = -2.0f * cosf(w0) * norm;
    bpf->b2 = (1.0f - alpha) * norm;

    // Initialisiere Verzögerungsglieder
    bpf->x1 = bpf->x2 = 0.0f;
    bpf->y1 = bpf->y2 = 0.0f;
}

void BPF_Process(int32_t *data_input, int32_t *data_output, BPF_COEFF *bpf)
{
    // Implementiere die Filtergleichung

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // Konvertiere ADC-Wert zu float
        float sample = (float)(data_input[i]);

        float output = bpf->a0 * sample +
                       bpf->a1 * bpf->x1 +
                       bpf->a2 * bpf->x2 -
                       bpf->b1 * bpf->y1 -
                       bpf->b2 * bpf->y2;

        // Aktualisiere Verzögerungsglieder
        bpf->x2 = bpf->x1;
        bpf->x1 = sample;
        bpf->y2 = bpf->y1;
        bpf->y1 = output;

        data_output[i] = (int32_t)output;
    }
}

BPF_COEFF bpf;
float fs = 104000.0f; // Abtastrate
float f0 = FREQUENCY; // Mittenfrequenz
float bw = 0.5f;      // Bandbreite in Oktaven

typedef struct
{
    int32_t a0, a1, a2; // Zähler-Koeffizienten
    int32_t b1, b2;     // Nenner-Koeffizienten
    int32_t x1, x2;     // Verzögerte Eingangswerte
    int32_t y1, y2;     // Verzögerte Ausgangswerte
} BPF_i_COEFF;

BPF_i_COEFF bpf_i;

void BPF_i_Init(BPF_i_COEFF *bpf, float fs, float f0, float bandwidth)
{
    // Berechne die Filterkoeffizienten zunächst in Fließkomma
    float w0 = 2.0f * M_PI * f0 / fs;
    float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bandwidth * w0 / sinf(w0));
    float norm = 1.0f / (1.0f + alpha);

    // Berechne die Koeffizienten
    float a0_float = alpha * norm;
    float a1_float = 0.0f;
    float a2_float = -alpha * norm;
    float b1_float = -2.0f * cosf(w0) * norm;
    float b2_float = (1.0f - alpha) * norm;

    // Konvertiere zu Festkomma mit FIXED_POINT_BITS Präzision
    bpf->a0 = (int32_t)(a0_float * FIXED_POINT_SCALE);
    bpf->a1 = (int32_t)(a1_float * FIXED_POINT_SCALE);
    bpf->a2 = (int32_t)(a2_float * FIXED_POINT_SCALE);
    bpf->b1 = (int32_t)(b1_float * FIXED_POINT_SCALE);
    bpf->b2 = (int32_t)(b2_float * FIXED_POINT_SCALE);

    // Initialisiere Verzögerungsglieder
    bpf->x1 = 0;
    bpf->x2 = 0;
    bpf->y1 = 0;
    bpf->y2 = 0;
}

void BPF_i_Process(BPF_i_COEFF *bpf, int32_t *data_input, int32_t *data_output)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // Convert float calculations to integer
        int32_t sample = data_input[i];

        // All multiplications and shifts must be done with integers
        int32_t term1 = (int32_t)((bpf->a0 * sample));
        int32_t term2 = (int32_t)((bpf->a1 * bpf->x1));
        int32_t term3 = (int32_t)((bpf->a2 * bpf->x2));
        int32_t term4 = (int32_t)((bpf->b1 * bpf->y1));
        int32_t term5 = (int32_t)((bpf->b2 * bpf->y2));

        int32_t output = (term1 >> FIXED_POINT_BITS) +
                         (term2 >> FIXED_POINT_BITS) +
                         (term3 >> FIXED_POINT_BITS) -
                         (term4 >> FIXED_POINT_BITS) -
                         (term5 >> FIXED_POINT_BITS);

        // Update delay elements
        bpf->x2 = bpf->x1;
        bpf->x1 = sample;
        bpf->y2 = bpf->y1;
        bpf->y1 = output;

        data_output[i] = output;
    }
}

int32_t input_data[BLOCK_SIZE];
int32_t output_data[BLOCK_SIZE];
int32_t output_data_2[BLOCK_SIZE];
GOERTZEL_COEFF g_coeff;
FILE *file;

#define FREQUENCY_START 10
#define FREQUENCY_STOPP (SAMPLERATE >> 2)

typedef struct
{
    float sum;          // Laufende Summe der quadrierten Werte
    float *buffer;      // Ringpuffer für die Samples
    int buffer_size;    // Größe des Ringpuffers
    int buffer_index;   // Aktuelle Position im Ringpuffer
    float scale_factor; // vorberechneter Skalierungsfaktor
} RMS_COEFF;

void RMS_Init(RMS_COEFF *rms, int window_size)
{
    rms->buffer = (float *)calloc(window_size, sizeof(float));
    rms->buffer_size = window_size;
    rms->buffer_index = 0;
    rms->sum = 0.0f;
    // Vorberechnung des Skalierungsfaktors
    rms->scale_factor = 0.00008f;
}

void RMS_Process(int32_t *data_input, int32_t *data_output, RMS_COEFF *rms)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        float sample = (float)(data_input[i]);

        // Subtrahiere das älteste quadrierte Sample
        rms->sum -= rms->buffer[rms->buffer_index] * rms->buffer[rms->buffer_index];

        // Füge das neue Sample hinzu
        rms->buffer[rms->buffer_index] = sample;
        rms->sum += sample * sample;

        // Berechne RMS
        // float output = sqrtf(rms->sum / rms->buffer_size);
        float output = rms->sum * rms->scale_factor;

        // Aktualisiere Buffer Index
        rms->buffer_index = (rms->buffer_index + 1) % rms->buffer_size;

        data_output[i] = (int32_t)output;
    }
}

void RMS_Free(RMS_COEFF *rms)
{
    free(rms->buffer);
}

RMS_COEFF rms;

int main(void)
{

    // Initialisiere output_data mit 0
    memset(output_data, 0, sizeof(output_data));
    memset(output_data_2, 0, sizeof(output_data_2));

    // Erzeuge 5kHz Sinussignal
    //    for (int i = 0; i < BLOCK_SIZE; i++) {
    //        double t = (double)i / SAMPLERATE;
    //        double signal = (sin(2 * M_PI * (FREQUENCY) * t) + 1) / 2;  // Normalisiere auf [0, 1]
    //        input_data[i] = (int32_t)(signal * MAX_VALUE);  // Skaliere auf [0, 1023]
    //    }

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        double t = (double)i / SAMPLERATE;
        // Berechne aktuelle Frequenz linear interpoliert zwischen Start und Stopp
        double current_freq = FREQUENCY_START + (FREQUENCY_STOPP - FREQUENCY_START) * ((double)i / BLOCK_SIZE);
        // Berechne die Phase für den Sweep
        double phase = 2.0 * M_PI * t * current_freq;
        double signal = (sin(phase) + 1) / 2;           // Normalisiere auf [0, 1]
        input_data[i] = (uint32_t)(signal * MAX_VALUE); // Skaliere auf [0, 1023]
    }

    // Initialisiere Goertzel-Koeffizienten
    // Goertzel_Init(SAMPLERATE, FREQUENCY, &g_coeff);
    // Führe Goertzel-Filter aus
    // Goertzel_Filter(input_data, output_data, &g_coeff);

    // Initialisiere Goertzel-Koeffizienten
    Goertzel_i_Init(SAMPLERATE, FREQUENCY, &g_i_coeff);
    // Führe Goertzel-Filter aus
    Goertzel_i_Filter(input_data, output_data, &g_i_coeff);

    // Initialisiere Filter
    BPF_i_Init(&bpf_i, fs, f0, bw);
    // Verarbeite Samples
    BPF_i_Process(&bpf_i, input_data, output_data_2);

    // Initialisiere Filter
    // BPF_Init(&bpf, fs, f0, bw);
    // Verarbeite Samples
    // BPF_Process(input_data, output_data, &bpf);

    // RMS_Init(&rms, 64); // 64 Samples Fensterbreite
    // RMS_Process(output_data, output_data_2, &rms);
    // RMS_Free(&rms);

    // Speichere Ergebnis in output.txt
    file = fopen("output.txt", "w");
    if (file == NULL)
    {
        printf("Fehler beim Öffnen der Datei!\n");
        return 1;
    }

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        fprintf(file, "%08d\t%08d\t%08d\n", input_data[i], output_data[i], output_data_2[i]);
    }
    fclose(file);

    printf("Goertzel-Filter ausgeführt. Ergebnisse in output.txt gespeichert.\n");

    return 0;
}

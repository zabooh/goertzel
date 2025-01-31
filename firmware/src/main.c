/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main_d21.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
 * Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
 *
 * Subject to your compliance with these terms, you may use Microchip software
 * and any derivatives exclusively with Microchip products. It is your
 * responsibility to comply with third party license terms applicable to your
 * use of third party software (including open source software) that may
 * accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <stdio.h>
#include <string.h>
#include "definitions.h"                // SYS function prototypes
#include "goertzel.h"
#include "fft_32BitInplace.h"


/*  STREAM_DATA allows to send 2*512 Samples and Goertzel Results by UART
 *   to a post processing tool on the PC for data visualization. 
 *   Attention: No continues realtime! 
 */
#define STREAM_DATA   

/* USE_FLOAT_GOERTZEL allows to use the float implementation of the Goertzel Algorithm
 * Attention: No continues realtime! 
 */
//#define USE_FLOAT_GOERTZEL

/* USE_ARTIFICIAL_SIGNAL uses a internal wave table instead of the ADc value
 * to check and compare the signal processing with an "perfect" signal
 * Can be used for Realtime
 */
#define USE_ARTIFICIAL_SIGNAL

/* SIGNAL_SCALE_THRESHOLD set the threshold for the Frequency detection
 */
#define SIGNAL_SCALE_THRESHOLD 1.4


#define ADC_VREF          (1.65f)
#define RTC_COMPARE_VAL       100

volatile bool dma_ch0Done = false; // Control flag for DMA Buffer ready
volatile int buffer_current_n = 0; // Controi information which buffer is currently used

int sine_wave_table_index = 0; // Current position in the wave table

GOERTZEL_f_COEFF goertzel_float_coeff; // Control date for Goertzel Float 
GOERTZEL_i_COEFF goertzel_integer_coeff; // Control date for Goertzel Fixpoint

float input_voltage;
float test_voltage = 0.0;

volatile int32_t dma_on = 1;

int32_t seconds_counter = 0;
int32_t beat_sum = 0;
int32_t beat_average = 0;

uint32_t myAppObj = 0;
uint16_t adc_result_array[2][BLOCK_SIZE];

int32_t output_data_0[BLOCK_SIZE];
int32_t output_data_1[BLOCK_SIZE];

#include "wave_array.h"

#define WAVE_TABLE_AMPLITUDE_SHIFT 1

void DmacCh0Cb(DMAC_TRANSFER_EVENT returned_evnt, uintptr_t MyDmacContext) {
    if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
        dma_ch0Done = true;
        if (buffer_current_n == 0) {
            buffer_current_n = 1;
            GPIO_PB16_Set();
            if (dma_on == 1) {
                DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *) &ADC_REGS->ADC_RESULT, (const void *) &adc_result_array[1][0], sizeof (adc_result_array) / 2);
            }
        } else {
            buffer_current_n = 0;
            GPIO_PB16_Clear();
            if (dma_on == 1) {
                DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *) &ADC_REGS->ADC_RESULT, (const void *) &adc_result_array[0][0], sizeof (adc_result_array) / 2);
            }
        }
    } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        while (1);
    }
}
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

uint16_t adc_value;

SYSTICK_TIMEOUT m_timeout;

int __attribute__((optimize("-O0"))) main(void) {
    /* Initialize all modules */
    SYS_Initialize(NULL);
    
    ADC_Enable();

    RTC_Timer32Start();
    RTC_Timer32CompareSet(RTC_COMPARE_VAL);

    SYSTICK_TimerStart();
    
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, DmacCh0Cb, (uintptr_t) & myAppObj);

    buffer_current_n = 0;
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *) &ADC_REGS->ADC_RESULT, (const void *) &adc_result_array[0][0], sizeof (adc_result_array) / 2);

    uint32_t uTC3_TimerFrequency = TC3_TimerFrequencyGet();
    float fTC3_TimerFrequency = (float) uTC3_TimerFrequency;
    uint16_t uTC3_Timer16bitPeriod = TC3_Timer16bitPeriodGet();
    float fTC3_Timer16bitPeriod = (float) uTC3_Timer16bitPeriod;
    float fSampleRate = 1.0 / ((fTC3_Timer16bitPeriod + 1.0)* (1.0 / fTC3_TimerFrequency));
    uint32_t uSampleRate = (uint32_t) fSampleRate;

//    Test_FFT(uSampleRate);
    

#ifdef USE_FLOAT_GOERTZEL
    Goertzel_f_Init(SAMPLERATE, FREQUENCY, &goertzel_float_coeff);
#else
    Goertzel_i_Init(uSampleRate, FREQUENCY, &goertzel_integer_coeff);
#endif    

    FLT_vIIR_Init();

    printf("\n\r---------------------------------------------------------");
    printf("\n\r Description         ADC DMA %d Pulse Detection              ", FREQUENCY);
    printf("\n\r Build Time:         "__DATE__" "__TIME__);
    printf("\n\r BLOCK_SIZE:         %d", BLOCK_SIZE);
    printf("\n\r BLOCK TIME:         %f", ((1.0 / SAMPLERATE) * BLOCK_SIZE));
    printf("\n\r FREQUENCY:          %d", FREQUENCY);
    printf("\n\r GOERTZEL_THRESHOLD: %d", GOERTZEL_THRESHOLD);
    printf("\n\r DAMPING_FACTOR:     %f", DAMPING_FACTOR);
    printf("\n\r WINDOW_SIZE:        %d", WINDOW_SIZE);
    printf("\n\r SAMPLERATE:         %d", (int) uSampleRate);
    printf("\n\r---------------------------------------------------------\n\r");

    TC3_TimerStart();
    SYSTICK_StartTimeOut(&m_timeout, 1000);

    
    while (true) {


        if (SYSTICK_IsTimeoutReached(&m_timeout)) {
            seconds_counter++;
            beat_sum += goertzel_integer_coeff.counter;
            beat_average = beat_sum / seconds_counter;
#ifndef STREAM_DATA               
            printf("Second %d \tBeat %d \tAverage %d \tAmplitude %d threshold %d\n",
                    (int) seconds_counter,
                    (int) goertzel_integer_coeff.counter,
                    (int) beat_average,
                    (int) goertzel_integer_coeff.max_amplitude,
                    (int) goertzel_integer_coeff.goertzel_threshold);
#endif
            goertzel_float_coeff.goertzel_threshold =
                    goertzel_integer_coeff.goertzel_threshold =
                    (goertzel_integer_coeff.max_amplitude * ((int32_t) (SIGNAL_SCALE_THRESHOLD * FIXED_POINT_SCALE))) >> FIXED_POINT_BITS;

            goertzel_integer_coeff.max_amplitude = 0;
            goertzel_integer_coeff.counter = 0;
            SYSTICK_StartTimeOut(&m_timeout, 1000);
        }


        if (dma_ch0Done == true) {
            dma_ch0Done = false;

            if (buffer_current_n == 0) {
#ifndef STREAM_DATA                    
                GPIO_PB17_Set();

#ifdef USE_ARTIFICIAL_SIGNAL                
                for (int ix = 0; ix < BLOCK_SIZE; ix++) {
                    adc_result_array_1[ix] = (sine_wave[sine_wave_table_index++] << WAVE_TABLE_AMPLITUDE_SHIFT) + 512;
                    if (sine_wave_table_index >= NUM_OF_SAMPLES) sine_wave_table_index = 0;
                }
#endif                
                Goertzel_i_Filter(&adc_result_array[1][0], output_data_1, &goertzel_integer_coeff);
                GPIO_PB17_Clear();
#endif                
            } else {
#ifndef STREAM_DATA                    
                GPIO_PB17_Set();
#ifdef USE_ARTIFICIAL_SIGNAL                
                for (int ix = 0; ix < BLOCK_SIZE; ix++) {
                    adc_result_array_0[ix] = (sine_wave[sine_wave_table_index++] << WAVE_TABLE_AMPLITUDE_SHIFT) + 512;
                    if (sine_wave_table_index >= NUM_OF_SAMPLES) sine_wave_table_index = 0;
                }
#endif
                Goertzel_i_Filter(&adc_result_array[0][0], output_data_0, &goertzel_integer_coeff);
                GPIO_PB17_Clear();
#endif                

#ifdef STREAM_DATA            

                dma_on = 0;
                while (!(dma_ch0Done == true && buffer_current_n == 0));
                DMAC_ChannelDisable(DMAC_CHANNEL_0);

#ifdef USE_ARTIFICIAL_SIGNAL
                for (int ix = 0; ix < BLOCK_SIZE; ix++) {
                    adc_result_array[0][ix] = (sine_wave[sine_wave_table_index++] << WAVE_TABLE_AMPLITUDE_SHIFT) + 512;
                    if (sine_wave_table_index >= NUM_OF_SAMPLES) sine_wave_table_index = 0;
                }
                for (int ix = 0; ix < BLOCK_SIZE; ix++) {
                    adc_result_array[1][ix] = (sine_wave[sine_wave_table_index++] << WAVE_TABLE_AMPLITUDE_SHIFT) + 512;
                    if (sine_wave_table_index >= NUM_OF_SAMPLES) sine_wave_table_index = 0;
                }
#endif
                GPIO_PB17_Set();
#ifdef USE_FLOAT_GOERTZEL
                Goertzel_f_Filter(adc_result_array_0, output_data_0, &goertzel_float_coeff);
#else
           //     Goertzel_i_Filter(&adc_result_array[0][0], output_data_0, &goertzel_integer_coeff);
#endif                           
                GPIO_PB17_Clear();
                GPIO_PB17_Set();
#ifdef USE_FLOAT_GOERTZEL
                Goertzel_f_Filter(adc_result_array_1, output_data_1, &goertzel_float_coeff);
#else
           //     Goertzel_i_Filter(&adc_result_array[1][0], output_data_1, &goertzel_integer_coeff);
                
                GPIO_PB17_Set();
                int32_t *ptr;
               
                // measured 224 milli seconds with float
                // measured  60 milli seconds with fixpoint
               
                ptr =  FFT_32BitInplace((uint16_t *)adc_result_array, 1024);
                GPIO_PB17_Clear();
                for(int ix=0;ix<512;ix++)output_data_0[ix] = (*ptr++)*20;
                //for(int ix=0;ix<512;ix++)output_data_0[ix] = iFLT_IIR1_Lowpass(0,(*ptr++)*20);
#endif  
                GPIO_PB17_Clear();

                for (int ix = 0; ix < BLOCK_SIZE; ix++) {
                    uint16_t sample;
                    printf("\r\nTransferred X results to array in SRAM\r\n");
                    for (sample = 0; sample < BLOCK_SIZE; sample++) {
                        adc_value = adc_result_array[0][sample];
                        input_voltage = (float) adc_value * ADC_VREF / 4095U;
                        printf("ADC Count [%d] = %04d, ADC Input Voltage = %f V Goertzel = %d\n", sample, (int) adc_value, input_voltage, (int) output_data_0[sample]);
                        SYSTICK_DelayMs(7);
                    }
                    ix = 0;
                    for (sample = 0; sample < BLOCK_SIZE; sample++) {
                        adc_value = adc_result_array[1][ix++];
                        input_voltage = (float) adc_value * ADC_VREF / 4095U;
                        printf("ADC Count [%d] = %04d, ADC Input Voltage = %f V Goertzel = %d\n", sample + BLOCK_SIZE, (int) adc_value, input_voltage, (int) output_data_1[sample]);
                        SYSTICK_DelayMs(7);
                    }
                }
                buffer_current_n = 0;
                dma_on = 1;
                DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *) &ADC_REGS->ADC_RESULT, (const void *) &adc_result_array[0][0], sizeof (adc_result_array) / 2);

#endif          

            }
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE);
}

/*******************************************************************************
 End of File
 */

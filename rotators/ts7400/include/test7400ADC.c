#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

#include "peekpoke.h"
#include "ep93xx_adc.h"

#define DATA_PAGE    0x12C00000
#define CALIB_LOC    2027          //location of calibration values
#define NUM_SAMPLES  5
#define NUM_CHANNELS 4

/* globals */
static unsigned long adc_page, syscon_page;
char *dr_page;

/*Calculate the adc value corresponding to 0V*/
//val1 is the ADC val corresponding to 0.833V
//val2 is the ADC val corresponding to 2.5V
int calcZeroVal(int val1, int val2)
{
    val2 += 0x10000;
    return (int)(val1 - (((val2 - val1) / (2.5 - 0.833)) * 0.833));
}


//return value of 1 indicates the board has no calibration values
//return value of 0 indicates the board has calibration values
int read_calibration(int buf[NUM_CHANNELS][2])
{
    int i, j, k = 0;
    unsigned short cal[NUM_CHANNELS * 2];
    // read 16 calibration bytes into buffer
    FILE *f = fopen("/etc/ADC-calibration.dat", "r");

    if (!f) { goto empty_calibration; }

    printf("Non-virgin board detected, evaluating stored "
           "calibration values\n");
    printf("Stored Calibration values [");

    if (fread(cal, NUM_CHANNELS * 4, 1, f) == 1)
    {
        fclose(f);

        for (j = 0; j < 2; j++)
            for (i = 0; i < NUM_CHANNELS; i++)
            {
                printf("0x%x", cal[k]);
                buf[i][j] = cal[k];
                k++;

                if (k < NUM_CHANNELS * 2)
                {
                    printf(", ");
                }

            }

        printf("]\n");
        return 1;
    }

empty_calibration:

    printf("/etc/ADC-calibration.dat not found or it's not readable\n");

    fclose(f);

    return 0;
}

void write_calibration(int cal[NUM_CHANNELS][2])
{
    unsigned short buf[16];
    int i, j, k = 0;
    FILE *f = fopen("/etc/ADC-calibration.dat", "w");

    //Convert 32 bit vals to 16 bit vals
    for (j = 0; j < 2; j++)
        for (i = 0; i < NUM_CHANNELS; i++)
        {
            buf[k] = (unsigned short)cal[i][j];
            k++;

        }

    if (!f) { goto unwrite_calibration; }

    if (fwrite(buf, NUM_CHANNELS * 4, 1, f) == 1) { return; }

unwrite_calibration:

    printf("Problem writing /etc/ADC-calibration.dat: %m\n");
}

static void erase_calibration()
{
    printf("Erasing calibration values...\n");
    unlink("/etc/ADC-calibration.dat");
}

int check_calibration(int cal[NUM_CHANNELS][2],
                      int stored_cal[NUM_CHANNELS][2], int state)
{
    double pcnt_diff;
    int i, j, erase_cal = 0;
    int failure = 0;

    if (state == 0) //no calibration values
    {
        printf("Virgin board detected...\n");

        for (j = 0; j < 2; j++)
        {

            for (i = 0; i < NUM_CHANNELS; i++)
            {

                if (j == 0)
                    pcnt_diff = (((double)abs(0xa000 -
                                              cal[i][j])) / 0xa000) * 100;
                else
                    pcnt_diff = (((double)abs(0x3300 -
                                              cal[i][j])) / 0x3300) * 100;

                if (pcnt_diff > 10)
                {
                    printf("Calculated calibration "
                           "values out of range...\n");

                    exit(-1);
                }

            }
        }

        write_calibration(cal);

    }
    else        //calibration values read
    {

        for (j = 0; j < 2; j++)
        {
            for (i = 0; i < NUM_CHANNELS; i++)
            {

                pcnt_diff = (((double)abs(stored_cal[i][j] -
                                          cal[i][j])) / stored_cal[i][j])
                            * 100;

                if (pcnt_diff > 0.25)
                {
                    if (!failure)
                    {
                        printf("Calibration values out"
                               "of range\n");
                        failure = 1;
                        erase_cal = 1;
                    }

                    printf("\tChannel %d: %3.3f%%\n", i
                           , pcnt_diff);
                }
            }
        }
    }

    if (erase_cal) { erase_calibration(); }

    if (failure) { return 0; }

    return 1;
}

void setDR(char *x, int n, int val)
{
    if (n < 0 || n > 8)
    {
        return;
    }

    x[0] = (x[0] & ~(1 << n)) | (val ? (1 << n) : 0);
}

void setD(char *x, int n, int val)
{
    if (n < 0 || n > 8)
    {
        return;
    }

    x[2] = (x[2] & ~(1 << n)) | (val ? (1 << n) : 0);
}

double get_volts(int val, int zero, int range)
{
    if (val <= 0x7000)
    {
        val = val + 0x10000;
    }

    val = val - zero;

    return ((double)val * 3.3) / range;
}

void calc_calibration(int calibration[NUM_CHANNELS][2],
                      int adc_result_1[NUM_CHANNELS][NUM_SAMPLES],
                      int adc_result_2[NUM_CHANNELS][NUM_SAMPLES])
{
    int i, j;

    /* zero out our calibration values */
    for (i = 0; i < NUM_CHANNELS; i++)
        for (j = 0; j < 2; j++)
        {
            calibration[i][j] = 0;
        }

    //convert 0.833V vals to 0V vals
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        for (j = 0; j < NUM_SAMPLES; j++)
        {
            if (i % 2) //odd channels
                adc_result_1[i][j] =
                    calcZeroVal(adc_result_1[i][j],
                                adc_result_1[0][j]);
            else
                adc_result_2[i][j] =
                    calcZeroVal(adc_result_2[i][j],
                                adc_result_2[1][j]);
        }
    }

    //sum the readings
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        for (j = 0; j < NUM_SAMPLES; j++)
        {
            if (i % 2 == 0)
            {
                //0.833 volt values
                calibration[i][0] = adc_result_2[i][j]
                                    + calibration[i][0];
                //2.5 volt values
                calibration[i][1] = adc_result_1[i][j]
                                    + calibration[i][1];
            }
            else
            {
                //0.833 volt values
                calibration[i][0] = adc_result_1[i][j]
                                    + calibration[i][0];
                //2.5 volt values
                calibration[i][1] = adc_result_2[i][j]
                                    + calibration[i][1];
            }
        }
    }

    printf("Calculated Calibration values [");

    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < NUM_CHANNELS; i++)
        {
            calibration[i][j] = (calibration[i][j] / NUM_SAMPLES);

            printf("0x%x", calibration[i][j]);

            if ((i == NUM_CHANNELS - 1) && (j == 1))
            {
                printf("]\n");
            }
            else
            {
                printf(", ");
            }
        }
    }
}

/************************************************************************
 *DESCRIPTION: Read the EP93xx onboard ADC. Discard the first
 *two samples then save the next NUM_SAMPLES.
 ***********************************************************************/
static void read_7xxx_adc(int adc_result[NUM_CHANNELS][NUM_SAMPLES])
{
    int i, j, cur_ch;

    for (i = 0; i < NUM_CHANNELS; i++)
    {
        switch (i)
        {
        case 0:
            cur_ch = ADC_CH0;
            break;

        case 1:
            cur_ch = ADC_CH1;
            break;

        case 2:
            cur_ch = ADC_CH2;
            break;

        case 3:
            cur_ch = ADC_CH3;
            break;

        case 4:
            cur_ch = ADC_CH4;
            break;

        }

        //discard first two samples
        read_channel(adc_page, cur_ch);
        read_channel(adc_page, cur_ch);

        //read more samples
        for (j = 0; j < NUM_SAMPLES; j++)
        {
            hl_usleep(10000);
            adc_result[i][j] = read_channel(adc_page, cur_ch);
        }
    }
}

int test_ADC(int calibration[NUM_CHANNELS][2])
{
    int adc_result_1[NUM_CHANNELS][NUM_SAMPLES];
    int adc_result_2[NUM_CHANNELS][NUM_SAMPLES];
    int i, j, return_val;
    int failure = 0;
    double voltage;

    setD(dr_page, 0, 1);    //ADC1 = ADC3 = 0.833V
    setD(dr_page, 2, 1);    //ADC0 = ADC2 = 2.5V
    read_7xxx_adc(adc_result_1);

    setD(dr_page, 0, 0);    //ADC1 = ADC3 = 2.5V
    setD(dr_page, 2, 1);    //ADC0 = ADC2 = 0.833V
    read_7xxx_adc(adc_result_2);

    //verify results are within range
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        for (j = 0; j < NUM_SAMPLES; j++)
        {
            //use the datasheet values
            voltage = get_volts(adc_result_1[i][j], 0x9E58, 0xC350);

            //even channels 2.5V(+-150mV)
            if (i % 2 == 0 &&
                    (voltage < 2.35 || voltage > 2.65))
            {
                if (!failure)
                {
                    failure = 1;
                    printf("EP93XX ADC out of range\n");
                }

                printf("\tChannel %d: %3.3fV"
                       "(expected 2.5V +- 150mV)\n", i, voltage);
                //odd channels 0.833(+-50mV)
            }
            else if (i % 2 == 1 &&
                     (voltage < 0.333 || voltage > 1.333))
            {
                if (!failure)
                {
                    failure = 1;
                    printf("EP93xx ADC out of range\n");
                }

                printf("\tChannel %d: %3.3fV"
                       "(expected 0.833V +- 50mV)\n", i, voltage);
            }


            //use the datasheet values
            voltage = get_volts(adc_result_2[i][j], 0x9E58, 0xC350);

            //odd channels 2.5V(+-150mV)
            if (i % 2 == 1 &&
                    (voltage < 2.35 || voltage > 2.65))
            {
                if (!failure)
                {
                    failure = 1;
                    printf("EP93XX ADC out of range\n");
                }

                printf("\tChannel %d: %3.3fV"
                       "(expected 2.5V +- 150mV)\n", i, voltage);
                //even channels 0.833(+-50mV)
            }
            else if (i % 2 == 0 &&
                     (voltage < 0.333 || voltage > 1.333))
            {
                if (!failure)
                {
                    failure = 1;
                    printf("EP93xx ADC out of range\n");
                }

                printf("\tChannel %d: %3.3fV"
                       "(expected 0.833V +- 50mV)\n", i, voltage);
            }
        }
    }

    calc_calibration(calibration, adc_result_1, adc_result_2);

    if (failure)
    {
        return_val = 0;
    }
    else { return_val = 1; }

    return return_val;
}


int main(void)
{
    int calibration[NUM_CHANNELS][2];
    int ret_val;
    int devmem = open("/dev/mem", O_RDWR | O_SYNC);
    assert(devmem != -1);

    dr_page = mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
                   MAP_SHARED, devmem, DATA_PAGE);
    assert(&dr_page != MAP_FAILED);
    adc_page = (unsigned long)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
                                   MAP_SHARED, devmem, ADC_PAGE);
    assert(&adc_page != MAP_FAILED);
    syscon_page = (unsigned long)mmap(0, getpagesize(), PROT_READ | PROT_WRITE
                                      , MAP_SHARED, devmem, SYSCON_PAGE);
    assert(&syscon_page != MAP_FAILED);

    init_ADC(adc_page, syscon_page);
    setDR(dr_page, 0, 1);
    setDR(dr_page, 1, 0);
    setDR(dr_page, 2, 1);
    setDR(dr_page, 3, 0);

    if (test_ADC(calibration))
    {
        int state;
        int stored_calibration[NUM_CHANNELS][2];
        printf("ADC tested ok(data sheet values)\n");
        state = read_calibration(stored_calibration);

        if (check_calibration(calibration, stored_calibration, state))
        {
            ret_val = 0;
        }
        else
        {
            ret_val = 1;
        }

    }
    else
    {
        ret_val = 1;
    }

    close(devmem);

    return ret_val;
}

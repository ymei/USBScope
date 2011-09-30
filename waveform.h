#ifndef __WAVEFORM_H__
#define __WAVEFORM_H__

#define TDS2024B_READ_ASK_SIZE 1025
#define TDS2024B_MEM_LENGTH 2500
#define TDS2024B_N_CH 4

#define DPO2024_READ_ASK_SIZE 1025
#define DPO2024_MEM_LENGTH 5000
#define DPO2024_N_CH 4

struct waveform_attribute 
{
    double dt;
    double t0;
    double ymult[TDS2024B_N_CH];
    double yoff[TDS2024B_N_CH];
    double yzero[TDS2024B_N_CH];
};

#endif /* __WAVEFORM_H__ */

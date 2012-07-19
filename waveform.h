#ifndef __WAVEFORM_H__
#define __WAVEFORM_H__

#define SCOPE_NCH 4
#define SCOPE_MEM_LENGTH TDS2024B_MEM_LENGTH

#define TDS2024B_READ_ASK_SIZE 1025
#define TDS2024B_MEM_LENGTH 2500

#define DPO2024_READ_ASK_SIZE 1025
#define DPO2024_MEM_LENGTH 5000
#define DPO2024_N_CH 4

#define DPO5054_READ_ASK_SIZE 1025
#define DPO5054_MEM_LENGTH 5000
#define DPO5054_MEM_LENGTH_MAX 100000

struct waveform_attribute 
{
    double dt;
    double t0;
    double ymult[SCOPE_NCH];
    double yoff[SCOPE_NCH];
    double yzero[SCOPE_NCH];
};

#endif /* __WAVEFORM_H__ */

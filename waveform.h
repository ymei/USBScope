#ifndef __WAVEFORM_H__
#define __WAVEFORM_H__

#define SCOPE_NCH 4
#define SCOPE_MEM_LENGTH_MAX 1000000

#define TDS2024B_READ_ASK_SIZE 1025
#define TDS2024B_MEM_LENGTH 2500

#define DPO2024_READ_ASK_SIZE 1025
#define DPO2024_MEM_LENGTH 5000

struct waveform_attribute 
{
    unsigned int chMask;
    size_t nPt; /* number of points in each event */
    size_t nFrames; /* number of Fast Frames in each event, 0 means off */
    double dt;
    double t0;
    double ymult[SCOPE_NCH];
    double yoff[SCOPE_NCH];
    double yzero[SCOPE_NCH];
};

#endif /* __WAVEFORM_H__ */

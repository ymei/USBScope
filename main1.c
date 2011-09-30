#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>
#include "usbtmc.h"
#include "waveform.h"
#include "hdf5io.h"

char waveformBuf[TDS2024B_N_CH][TDS2024B_MEM_LENGTH+1];

int dpo2024_read(struct usbtmc_device_handle *usbtmcDev, unsigned char *retData)
{
    int i, ret, dataLen=0;

    unsigned char readBuf[DPO2024_READ_ASK_SIZE]={0}, *p;

    if(retData != NULL)
        p = retData;

    ret = 1;
    dataLen = 0;
    while(readBuf[ret-1] != '\n') {
        ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, DPO2024_READ_ASK_SIZE);
        if(retData != NULL) {
            memcpy(p, readBuf, ret);
            p += ret;
        }
        else {
            for(i=0; i<ret; i++)
                printf("%c", readBuf[i]);
        }
        dataLen += ret;
    }
    return dataLen;
}


int dpo2024_get_wavform_attr(struct usbtmc_device_handle *usbtmcDev,
                              struct waveform_attribute *wavAttr)
{
    int ret, ich;
    char cmdBuf[DPO2024_READ_ASK_SIZE], readBuf[DPO2024_READ_ASK_SIZE], *p;

    usbtmc_write(usbtmcDev, "WFMPRE:XINCR?");
    ret = dpo2024_read(usbtmcDev, (unsigned char*)readBuf);
    readBuf[ret+1]='\0';
    p = readBuf + 13;
    wavAttr->dt = atof(p);
//    printf("%s %g\n", readBuf, wavAttr->dt);

    usbtmc_write(usbtmcDev, "WFMPRE:XZERO?");
    ret = dpo2024_read(usbtmcDev, (unsigned char*)readBuf);
    readBuf[ret+1]='\0';
    p = readBuf + 13;
    wavAttr->t0 = atof(p);
//    printf("%s %g\n", readBuf, wavAttr->t0);

    for(ich=0; ich<DPO2024_N_CH; ich++) {
        sprintf(cmdBuf, "DATA:SOURCE CH%d", ich+1);
        usbtmc_write(usbtmcDev, cmdBuf);

        usbtmc_write(usbtmcDev, "WFMPRE:YMULT?");
        ret = dpo2024_read(usbtmcDev, (unsigned char*)readBuf);
        readBuf[ret+1]='\0';
        p = readBuf + 13;
        wavAttr->ymult[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->ymult[ich]);

        usbtmc_write(usbtmcDev, "WFMPRE:YOFF?");
        ret = dpo2024_read(usbtmcDev, (unsigned char*)readBuf);
        readBuf[ret+1]='\0';
        p = readBuf + 12;
        wavAttr->yoff[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->yoff[ich]);

        usbtmc_write(usbtmcDev, "WFMPRE:YZERO?");
        ret = dpo2024_read(usbtmcDev, (unsigned char*)readBuf);
        readBuf[ret+1]='\0';
        p = readBuf + 13;
        wavAttr->yzero[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->yzero[ich]);
    }
    
    printf("%s:\n"
           "     dt    = %g\n"
           "     t0    = %g\n"
           "     ymult = %g %g %g %g\n"
           "     yoff  = %g %g %g %g\n"
           "     yzero = %g %g %g %g\n",
           __FUNCTION__,
           wavAttr->dt, wavAttr->t0,
           wavAttr->ymult[0], wavAttr->ymult[1], wavAttr->ymult[2], wavAttr->ymult[3],
           wavAttr->yoff[0], wavAttr->yoff[1], wavAttr->yoff[2], wavAttr->yoff[3],
           wavAttr->yzero[0], wavAttr->yzero[1], wavAttr->yzero[2], wavAttr->yzero[3]
        );
    
    return 0;
}

int dpo2024_acquire_and_read(struct usbtmc_device_handle *usbtmcDev,
                              int start, int stop, unsigned int chMask)
{
    int ret, wavLen, retWavLen, i, j, digits;
    unsigned int ich;
    char cmdBuf[256], wavBuf[DPO2024_MEM_LENGTH];

    wavLen = stop-start;

    sprintf(cmdBuf, "DATA:START %d", start+1);
    usbtmc_write(usbtmcDev, cmdBuf);
    sprintf(cmdBuf, "DATA:STOP %d", stop);
    usbtmc_write(usbtmcDev, cmdBuf);

    usbtmc_write(usbtmcDev, "ACQUIRE:STATE RUN");

    for(ich=0; ich<DPO2024_N_CH; ich++) {
        if((chMask >> ich) & 0x01) {
            sprintf(cmdBuf, "DATA:SOURCE CH%d", ich+1);
            usbtmc_write(usbtmcDev, cmdBuf);
            usbtmc_write(usbtmcDev, "CURVE?");

            ret = usbtmc_read(usbtmcDev, (unsigned char*)wavBuf, DPO2024_READ_ASK_SIZE);

            cmdBuf[0] = wavBuf[1];
            cmdBuf[1] = '\0';
            digits = atoi(cmdBuf);
            for(i=0; i<digits; i++)
                cmdBuf[i] = wavBuf[i+2];
            cmdBuf[i] = '\0';
            retWavLen = atoi(cmdBuf);
            if(wavLen != retWavLen) {
                fprintf(stderr, "Returned waveform length (%d) != expected (%d)\n",
                        retWavLen, wavLen);
            }
            for(j=0; j<ret-2-digits; j++) {
                waveformBuf[ich][j] = wavBuf[j+2+digits];
                retWavLen--;
            }
            while(retWavLen > 0) {
                ret = usbtmc_read(usbtmcDev, (unsigned char*)wavBuf, DPO2024_MEM_LENGTH);
                for(i=0; i<ret; i++) {
                    waveformBuf[ich][j] = wavBuf[i];
                    j++;
                    retWavLen--;
                }
            }
        }
    }
    return wavLen;
}


int main(int argc, char **argv)
{
    struct usbtmc_device_handle *usbtmcDev;
    struct hdf5io_waveform_file *waveformFile;
    struct waveform_attribute waveformAttr;
    struct hdf5io_waveform_event waveformEvent;

    int i, retWavLen;
    int nEvents, chMask;
    char *p, *outFileName;

    if(argc<4) {
        fprintf(stderr, "%s outFileName nEvents chMask(0x..)\n", argv[0]);
        return EXIT_FAILURE;
    }
    outFileName = argv[1];
    nEvents = atoi(argv[2]);

    errno = 0;
    chMask = strtol(argv[3], &p, 16);
    if(errno != 0 || *p != 0 || p == argv[3] || chMask <= 0 ) {
        fprintf(stderr, "Invalid chMask input: %s\n", argv[3]);
        return EXIT_FAILURE;
    }
    
/*
    FILE *fp;
    if((fp=fopen("wav.dat", "w"))==NULL) {
        perror("wav.dat");
    }
*/
    usbtmcDev = usbtmc_open_device(0x0699, 0x0374); //Tektronix DPO2024

    usbtmc_clear(usbtmcDev);

    usbtmc_write(usbtmcDev, "*CLS;*IDN?");
    dpo2024_read(usbtmcDev, NULL);
    
    usbtmc_write(usbtmcDev, "DATA INIT");
    usbtmc_write(usbtmcDev, "DATA?");
    dpo2024_read(usbtmcDev, NULL);

    usbtmc_write(usbtmcDev, "ACQUIRE:STOPAFTER SEQUENCE");
    usbtmc_write(usbtmcDev, "ACQUIRE?");
    dpo2024_read(usbtmcDev, NULL);

//    dpo2024_get_wavform_attr(usbtmcDev, &waveformAttr);
    printf("here\n");

    waveformFile = hdf5io_open_file(outFileName);
    hdf5io_write_waveform_attribute_in_file_header(waveformFile, &waveformAttr);

    printf("start time = %zd\n", time(NULL));

    for(i=0; i<nEvents; i++) {
        retWavLen = dpo2024_acquire_and_read(usbtmcDev, 0, DPO2024_MEM_LENGTH, chMask);
        waveformEvent.eventId = i;
        waveformEvent.wavBuf = waveformBuf;
        waveformEvent.waveSize = retWavLen;
        waveformEvent.nch = DPO2024_N_CH;
        waveformEvent.chMask = chMask;

        hdf5io_write_event(waveformFile, &waveformEvent);
/*
        for(i=0; i<retWavLen; i++) {
            for(ich=0; ich<DPO2024_N_CH; ich++)
                fprintf(fp, "  %4d", waveformBuf[ich][i]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n\n");
*/
    }

    printf("stop time  = %zd\n", time(NULL));

    hdf5io_close_file(waveformFile);    
    usbtmc_close_device(usbtmcDev);
//    fclose(fp);
    return EXIT_SUCCESS;
}

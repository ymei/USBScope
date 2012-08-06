#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <libusb-1.0/libusb.h>
#include "usbtmc.h"
#include "waveform.h"
#include "hdf5io.h"

#ifdef DEBUG
  #define debug_printf(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); fflush(stderr); \
                                    } while (0)
#else
  #define debug_printf(...) ((void)0)
#endif
#define error_printf(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); fflush(stderr); \
                                  } while(0)

static unsigned int chMask;
static size_t nCh;
static size_t nEvents;

static char *wavBuf;
struct hdf5io_waveform_file *waveformFile;
struct usbtmc_device_handle *usbtmcDev;

void atexit_flush_files(void)
{
    hdf5io_flush_file(waveformFile);
    hdf5io_close_file(waveformFile);
    /* this one seems to generate libusb error (transfer was still being processed) */
    // usbtmc_close_device(usbtmcDev);
}

void signal_kill_handler(int sig)
{
    printf("\nstop time  = %zd\n", time(NULL));
    fflush(stdout);
    
    fprintf(stderr, "Killed, cleaning up...\n");
    atexit_flush_files();
    exit(EXIT_SUCCESS);
}

int tds2024b_get_wavform_attr(struct usbtmc_device_handle *usbtmcDev,
                              struct waveform_attribute *wavAttr)
{
    int ret, ich;
    char cmdBuf[256], readBuf[256], *p;

    wavAttr->nFrames = 0;
    wavAttr->nPt = TDS2024B_MEM_LENGTH;
    wavAttr->chMask = chMask;

    usbtmc_write(usbtmcDev, "WFMPRE:XINCR?");
    ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, 256);
    readBuf[ret+1]='\0';
    p = readBuf + 13;
    wavAttr->dt = atof(p);
//    printf("%s %g\n", readBuf, wavAttr->dt);

    usbtmc_write(usbtmcDev, "WFMPRE:XZERO?");
    ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, 256);
    readBuf[ret+1]='\0';
    p = readBuf + 13;
    wavAttr->t0 = atof(p);
//    printf("%s %g\n", readBuf, wavAttr->t0);

    for(ich=0; ich<SCOPE_NCH; ich++) {
        sprintf(cmdBuf, "DATA:SOURCE CH%d", ich+1);
        usbtmc_write(usbtmcDev, cmdBuf);

        usbtmc_write(usbtmcDev, "WFMPRE:YMULT?");
        ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, 256);
        readBuf[ret+1]='\0';
        p = readBuf + 13;
        wavAttr->ymult[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->ymult[ich]);

        usbtmc_write(usbtmcDev, "WFMPRE:YOFF?");
        ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, 256);
        readBuf[ret+1]='\0';
        p = readBuf + 12;
        wavAttr->yoff[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->yoff[ich]);

        usbtmc_write(usbtmcDev, "WFMPRE:YZERO?");
        ret = usbtmc_read(usbtmcDev, (unsigned char*)readBuf, 256);
        readBuf[ret+1]='\0';
        p = readBuf + 13;
        wavAttr->yzero[ich] = atof(p);
//        printf("%s %g\n", readBuf, wavAttr->yzero[ich]);
    }
    
    printf("%s:\n"
           "     chMask  = 0x%02x\n"
           "     nPt     = %zd\n"
           "     nFrames = %zd\n"
           "     dt      = %g\n"
           "     t0      = %g\n"
           "     ymult   = %g %g %g %g\n"
           "     yoff    = %g %g %g %g\n"
           "     yzero   = %g %g %g %g\n",
           __FUNCTION__,
           wavAttr->chMask, wavAttr->nPt, wavAttr->nFrames,
           wavAttr->dt, wavAttr->t0,
           wavAttr->ymult[0], wavAttr->ymult[1], wavAttr->ymult[2], wavAttr->ymult[3],
           wavAttr->yoff[0], wavAttr->yoff[1], wavAttr->yoff[2], wavAttr->yoff[3],
           wavAttr->yzero[0], wavAttr->yzero[1], wavAttr->yzero[2], wavAttr->yzero[3]
        );
    
    return 0;
}

int tds2024b_acquire_and_read(struct usbtmc_device_handle *usbtmcDev,
                              struct waveform_attribute *wavAttr,
                              int start, int stop)
{
    int ret, wavLen, retWavLen, i, j, digits;
    size_t ich, ichBuf;
    char cmdBuf[256], buf[TDS2024B_READ_ASK_SIZE];

    wavLen = stop-start;

    sprintf(cmdBuf, "DATA:START %d", start+1);
    usbtmc_write(usbtmcDev, cmdBuf);
    sprintf(cmdBuf, "DATA:STOP %d", stop);
    usbtmc_write(usbtmcDev, cmdBuf);

    usbtmc_write(usbtmcDev, "ACQUIRE:STATE RUN");

    ichBuf = 0;
    for(ich=0; ich<SCOPE_NCH; ich++) {
        if((chMask >> ich) & 0x01) {
            sprintf(cmdBuf, "DATA:SOURCE CH%zd", ich+1);
            usbtmc_write(usbtmcDev, cmdBuf);
            usbtmc_write(usbtmcDev, "CURVE?");

            ret = usbtmc_read(usbtmcDev, (unsigned char*)buf, TDS2024B_READ_ASK_SIZE);
            cmdBuf[0] = buf[8];
            cmdBuf[1] = '\0';
            digits = atoi(cmdBuf);
            for(i=0; i<digits; i++)
                cmdBuf[i] = buf[i+9];
            cmdBuf[i] = '\0';
            retWavLen = atoi(cmdBuf);
            if(wavLen != retWavLen) {
                fprintf(stderr, "Returned waveform length (%d) != expected (%d)\n",
                        retWavLen, wavLen);
            }
            for(j=0; j<ret-9-digits; j++) {
                wavBuf[ichBuf * wavAttr->nPt + j] = buf[j+9+digits];
                retWavLen--;
            }
            while(retWavLen > 0) {
                ret = usbtmc_read(usbtmcDev, (unsigned char*)buf, TDS2024B_READ_ASK_SIZE);
                for(i=0; i<ret; i++) {
                    wavBuf[ichBuf * wavAttr->nPt + j] = buf[i];
                    j++;
                    retWavLen--;
                }
            }
            ichBuf++;
        }
    }
    return wavLen;
}


int main(int argc, char **argv)
{
#if 1
    struct waveform_attribute waveformAttr;
    struct hdf5io_waveform_event waveformEvent;

    int i, retWavLen;
    char *p, *outFileName;
    unsigned int v, c;
    size_t nWfmPerChunk = 100;
    size_t wavBufN;

    if(argc<4) {
        fprintf(stderr, "%s outFileName chMask(0x..) nEvents nWfmPerChunk\n", argv[0]);
        return EXIT_FAILURE;
    }
    outFileName = argv[1];
    nEvents = atoi(argv[3]);

    errno = 0;
    chMask = strtol(argv[2], &p, 16);
    v = chMask;
    for(c=0; v; c++) v &= v - 1; /* Brian Kernighan's way of counting bits */
    nCh = c;
    if(errno != 0 || *p != 0 || p == argv[2] || chMask <= 0 || nCh>SCOPE_NCH) {
        fprintf(stderr, "Invalid chMask input: %s\n", argv[3]);
        return EXIT_FAILURE;
    }
    if(argc>=5)
        nWfmPerChunk = atol(argv[4]);

    debug_printf("outFileName: %s, chMask: 0x%02x, nCh: %zd, nEvents: %zd, nWfmPerChunk: %zd\n",
                 outFileName, chMask, nCh, nEvents, nWfmPerChunk);
/*
    FILE *fp;
    if((fp=fopen("wav.dat", "w"))==NULL) {
        perror("wav.dat");
    }
*/
    usbtmcDev = usbtmc_open_device(0x0699, 0x036a); //Tektronix TDS2024B

    usbtmc_clear(usbtmcDev);

    usbtmc_write(usbtmcDev, "*CLS;*IDN?");
    usbtmc_read(usbtmcDev, NULL, TDS2024B_READ_ASK_SIZE);

    usbtmc_write(usbtmcDev, "DATA INIT");
    usbtmc_write(usbtmcDev, "DATA?");
    usbtmc_read(usbtmcDev, NULL, TDS2024B_READ_ASK_SIZE);
    usbtmc_write(usbtmcDev, "ACQUIRE:STOPAFTER SEQUENCE");
    usbtmc_write(usbtmcDev, "ACQUIRE?");
    usbtmc_read(usbtmcDev, NULL, TDS2024B_READ_ASK_SIZE);

    tds2024b_get_wavform_attr(usbtmcDev, &waveformAttr);

    wavBufN = waveformAttr.nPt * nCh;
    wavBuf = (char*)malloc(wavBufN * sizeof(char));

    waveformFile = hdf5io_open_file(outFileName, nWfmPerChunk, nCh);
    hdf5io_write_waveform_attribute_in_file_header(waveformFile, &waveformAttr);

    signal(SIGKILL, signal_kill_handler);
    signal(SIGINT, signal_kill_handler);

    printf("start time = %zd\n", time(NULL));

    for(i=0; i<nEvents; i++) {
        if(i%1 == 0) {
            printf("\r                                            ");
            printf("\rEvent %d", i);
            fflush(stdout);
        }
        retWavLen = tds2024b_acquire_and_read(usbtmcDev, &waveformAttr,
                                              0, TDS2024B_MEM_LENGTH);
        waveformEvent.eventId = i;
        waveformEvent.wavBuf = wavBuf;

        hdf5io_write_event(waveformFile, &waveformEvent);
        hdf5io_flush_file(waveformFile);
/*
        for(i=0; i<retWavLen; i++) {
            for(ich=0; ich<SCOPE_NCH; ich++)
                fprintf(fp, "  %4d", waveformBuf[ich][i]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n\n");
*/
    }

    printf("\nstop time  = %zd\n", time(NULL));
    usbtmc_write(usbtmcDev, "ACQUIRE:STOPAFTER RUNSTOP");
    usbtmc_write(usbtmcDev, "ACQUIRE:STATE RUN");

    free(wavBuf);
    hdf5io_close_file(waveformFile);    
    usbtmc_close_device(usbtmcDev);
//    fclose(fp);
    return EXIT_SUCCESS;
#endif
#if 0
    int i, j;
    char lineBuf[2048];
    
    struct hdf5io_waveform_file *wavFile;
    struct waveform_attribute wavAttr;
    struct hdf5io_waveform_event wavEvent;

    FILE *fp;
    if((fp=fopen("wav.dat", "r"))==NULL) {
        perror("wav.dat");
    }
    
    wavFile = hdf5io_open_file("wav.h5");

    wavAttr.dt = 1.0e-9;
    wavAttr.t0 = 2.66e-07;

    wavAttr.ymult[0] = 0.0002;
    wavAttr.ymult[1] = 0.2;
    wavAttr.ymult[2] = 0.02;
    wavAttr.ymult[3] = 0.008;

    wavAttr.yoff[0] = 75;
    wavAttr.yoff[1] = -15;
    wavAttr.yoff[2] = -172;
    wavAttr.yoff[3] = -250;

    hdf5io_write_waveform_attribute_in_file_header(wavFile, &wavAttr);

    wavEvent.nch = 4;
    wavEvent.chMask = 0x01;
    wavEvent.eventId = 0;
    i=0; j=0;
    while((fgets(lineBuf, 2048, fp) != NULL) && (!feof(fp))) {
        if(lineBuf[0]=='\n') {
            j++;
            if(j==2) {
                j=0;
                wavEvent.waveSize = i;
                i = 0;
                wavEvent.wavBuf = waveformBuf;
                hdf5io_write_event(wavFile, &wavEvent);
                wavEvent.eventId++;
            }
            continue;
        }
        waveformBuf[0][i] = (char)atoi(lineBuf);
        i++;
    }
    
    hdf5io_close_file(wavFile);
    fclose(fp);
    
    return EXIT_SUCCESS;
#endif
}

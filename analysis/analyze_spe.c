#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "waveform.h"
#include "hdf5io.h"

char waveformBuf[TDS2024B_N_CH][TDS2024B_MEM_LENGTH+1];

int main(int argc, char **argv)
{
    int i, iStart, iStop, iCh, chMask, nEvents, nChunk, iChunk, nBaseline, integralHalfWindow, iMax;
    int nEventsInFile;
    double waveform[TDS2024B_MEM_LENGTH+1];
    double sum, baseline, blMax, blMaxThreshold, vMax, vMaxThreshold;
    char *inFileName, *p;
    
    struct hdf5io_waveform_file *waveformFile;
    struct waveform_attribute waveformAttr;
    struct hdf5io_waveform_event waveformEvent;

    if(argc<4) {
        fprintf(stderr, "%s inFileName nEvents chMask(0x..)\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    inFileName = argv[1];
    nEvents = atoi(argv[2]);
    errno = 0;
    chMask = strtol(argv[3], &p, 16);
    if(errno != 0 || *p != 0 || p == argv[3] || chMask <= 0 ) {
        fprintf(stderr, "Invalid chMask input: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    waveformFile = hdf5io_open_file_for_read(inFileName);

    hdf5io_read_waveform_attribute_in_file_header(waveformFile, &waveformAttr);
    fprintf(stderr, "%s:\n"
            "     dt    = %g\n"
            "     t0    = %g\n"
            "     ymult = %g %g %g %g\n"
            "     yoff  = %g %g %g %g\n"
            "     yzero = %g %g %g %g\n",
            "Waveform Attributes:",
            waveformAttr.dt, waveformAttr.t0,
            waveformAttr.ymult[0], waveformAttr.ymult[1],
            waveformAttr.ymult[2], waveformAttr.ymult[3],
            waveformAttr.yoff[0], waveformAttr.yoff[1],
            waveformAttr.yoff[2], waveformAttr.yoff[3],
            waveformAttr.yzero[0], waveformAttr.yzero[1],
            waveformAttr.yzero[2], waveformAttr.yzero[3]
        );
    nEventsInFile = hdf5io_get_number_of_event(waveformFile);
    fprintf(stderr, "Number of events in file: %d\n", nEventsInFile);
    if(nEvents <= 0 || nEvents > nEventsInFile) nEvents = nEventsInFile;

    waveformEvent.wavBuf = waveformBuf;
    waveformEvent.nch = TDS2024B_N_CH;
    waveformEvent.chMask = chMask;
    for(i=0;i<TDS2024B_N_CH;i++) {
        if((chMask>>i) & 0x01) {
            iCh = i;
            fprintf(stderr, "Analyzing Ch%d\n", iCh);
            break;
        }
    }

    for(waveformEvent.eventId=0; waveformEvent.eventId < nEvents; waveformEvent.eventId++) {
        hdf5io_read_event(waveformFile, &waveformEvent);

        for(i=0; i<waveformEvent.waveSize; i++) {
            waveform[i] = - (waveformBuf[iCh][i] - waveformAttr.yoff[iCh])
                           * waveformAttr.ymult[iCh];
        }

        nChunk = 50;
        for(iChunk=0; iChunk<nChunk; iChunk++) {
            iStart = waveformEvent.waveSize / nChunk * iChunk;
            iStop = waveformEvent.waveSize / nChunk * (iChunk+1);

            nBaseline = 5;
            blMaxThreshold = 0.0015;

            baseline = 0.0;
            blMax = 0.0;
            for(i=iStart; i<iStart+nBaseline; i++) {                
                baseline += waveform[i];
                if(fabs(waveform[i]) > blMax) blMax = fabs(waveform[i]);
            }
            baseline /= (double)nBaseline;
            // if(blMax > blMaxThreshold) continue;

            integralHalfWindow = 7;
            vMaxThreshold = 0.0;

            vMax = 0.0;
            for(i=iStart+integralHalfWindow; i<iStop-integralHalfWindow; i++) {
                if(waveform[i] > vMax) {
                    vMax = waveform[i];
                    iMax = i;
                }
            }
            
            if(vMax >= vMaxThreshold) {
                sum = 0.0;
                for(i=iMax-integralHalfWindow; i<iMax+integralHalfWindow; i++)
                    sum += waveform[i]; //-baseline;
                printf("%24.16e\n", sum);
            }

/*
            for(i=iStart; i<iStop; i++)
                printf("%24.16e\n", waveform[i]-baseline);
            printf("\n\n");
*/
        }
    }
    
    hdf5io_close_file(waveformFile);
    
    return EXIT_SUCCESS;
}


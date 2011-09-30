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
    int i, iCh, chMask, nEvents;
    int nEventsInFile;
    double waveform[TDS2024B_MEM_LENGTH+1];
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

    for(waveformEvent.eventId=0; waveformEvent.eventId < nEvents; waveformEvent.eventId++) {
        hdf5io_read_event(waveformFile, &waveformEvent);

        for(i=0; i<waveformEvent.waveSize; i++) {
            printf("%24.16e ", waveformAttr.dt*i);
            for(iCh=0; iCh<TDS2024B_N_CH; iCh++) {
                waveform[i] = (waveformBuf[iCh][i] - waveformAttr.yoff[iCh])
                    * waveformAttr.ymult[iCh];
                printf("%24.16e ", waveform[i]);
            }
            printf("\n");
        }
        printf("\n\n");
    }
    
    hdf5io_close_file(waveformFile);
    
    return EXIT_SUCCESS;
}


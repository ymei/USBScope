#ifndef __HDF5IO_H__
#define __HDF5IO_H__

#include <hdf5.h>
#include "waveform.h"

#define HDF5IO_NAME_BUF_SIZE 256

struct hdf5io_waveform_file 
{
    hid_t waveFid;
};

struct hdf5io_waveform_event
{
    int eventId;
    char (*wavBuf)[SCOPE_MEM_LENGTH+1];
    int waveSize;
    int nch;
    unsigned int chMask;
};

struct hdf5io_waveform_file *hdf5io_open_file(const char *fname);
struct hdf5io_waveform_file *hdf5io_open_file_for_read(const char *fname);
int hdf5io_close_file(struct hdf5io_waveform_file *wavFile);
int hdf5io_flush_file(struct hdf5io_waveform_file *wavFile);

int hdf5io_write_waveform_attribute_in_file_header(struct hdf5io_waveform_file *wavFile,
                                                   struct waveform_attribute *wavAttr);
int hdf5io_read_waveform_attribute_in_file_header(struct hdf5io_waveform_file *wavFile,
                                                   struct waveform_attribute *wavAttr);
int hdf5io_write_event(struct hdf5io_waveform_file *wavFile,
                       struct hdf5io_waveform_event *wavEvent);
int hdf5io_read_event(struct hdf5io_waveform_file *wavFile,
                      struct hdf5io_waveform_event *wavEvent);
int hdf5io_get_number_of_event(struct hdf5io_waveform_file *wavFile);

#endif

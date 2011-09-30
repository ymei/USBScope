#include <stdlib.h>
#include <hdf5.h>
#include "waveform.h"
#include "hdf5io.h"

struct hdf5io_waveform_file *hdf5io_open_file(const char *fname)
{
    struct hdf5io_waveform_file *wavFile;
    wavFile = (struct hdf5io_waveform_file *)malloc(sizeof(struct hdf5io_waveform_file));
    wavFile->waveFid = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    return wavFile;
}

struct hdf5io_waveform_file *hdf5io_open_file_for_read(const char *fname)
{
    struct hdf5io_waveform_file *wavFile;
    wavFile = (struct hdf5io_waveform_file *)malloc(sizeof(struct hdf5io_waveform_file));
    wavFile->waveFid = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
    return wavFile;
}

int hdf5io_close_file(struct hdf5io_waveform_file *wavFile)
{
    herr_t ret;
    
    ret = H5Fclose(wavFile->waveFid);
    free(wavFile);
    return (int)ret;
}

int hdf5io_flush_file(struct hdf5io_waveform_file *wavFile)
{
    herr_t ret;
    
    ret = H5Fflush(wavFile->waveFid, H5F_SCOPE_GLOBAL);
    return (int)ret;
}

int hdf5io_write_waveform_attribute_in_file_header(struct hdf5io_waveform_file *wavFile,
                                                   struct waveform_attribute *wavAttr)
{
    herr_t ret;
    
    hid_t wavAttrTid, wavAttrSid, wavAttrAid, doubleArrayTid, rootGid;
    const hsize_t doubleArrayDims[1]={TDS2024B_N_CH};
    const unsigned doubleArrayRank = 1;

    doubleArrayTid = H5Tarray_create(H5T_NATIVE_DOUBLE, doubleArrayRank, doubleArrayDims);
    
    wavAttrTid = H5Tcreate(H5T_COMPOUND, sizeof(struct waveform_attribute));

    H5Tinsert(wavAttrTid, "wavAttr.dt", HOFFSET(struct waveform_attribute, dt), H5T_NATIVE_DOUBLE);
    H5Tinsert(wavAttrTid, "wavAttr.t0", HOFFSET(struct waveform_attribute, t0), H5T_NATIVE_DOUBLE);
    H5Tinsert(wavAttrTid, "wavAttr.ymult",
              HOFFSET(struct waveform_attribute, ymult), doubleArrayTid);
    H5Tinsert(wavAttrTid, "wavAttr.yoff",
              HOFFSET(struct waveform_attribute, yoff), doubleArrayTid);
    H5Tinsert(wavAttrTid, "wavAttr.yzero",
              HOFFSET(struct waveform_attribute, yzero), doubleArrayTid);

    wavAttrSid = H5Screate(H5S_SCALAR);

    rootGid = H5Gopen(wavFile->waveFid, "/", H5P_DEFAULT);

    wavAttrAid = H5Acreate(rootGid, "Waveform Attributes", wavAttrTid, wavAttrSid,
                           H5P_DEFAULT, H5P_DEFAULT);

    ret = H5Awrite(wavAttrAid, wavAttrTid, wavAttr);

    H5Aclose(wavAttrAid);
    H5Sclose(wavAttrSid);
    H5Tclose(wavAttrTid);
    H5Tclose(doubleArrayTid);
    H5Gclose(rootGid);
    
    return (int)ret;
}

int hdf5io_read_waveform_attribute_in_file_header(struct hdf5io_waveform_file *wavFile,
                                                  struct waveform_attribute *wavAttr)
{
    herr_t ret;

    hid_t wavAttrTid, wavAttrAid, doubleArrayTid;
    const hsize_t doubleArrayDims[1]={TDS2024B_N_CH};
    const unsigned doubleArrayRank = 1;

    doubleArrayTid = H5Tarray_create(H5T_NATIVE_DOUBLE, doubleArrayRank, doubleArrayDims);
    
    wavAttrTid = H5Tcreate(H5T_COMPOUND, sizeof(struct waveform_attribute));

    H5Tinsert(wavAttrTid, "wavAttr.dt", HOFFSET(struct waveform_attribute, dt), H5T_NATIVE_DOUBLE);
    H5Tinsert(wavAttrTid, "wavAttr.t0", HOFFSET(struct waveform_attribute, t0), H5T_NATIVE_DOUBLE);
    H5Tinsert(wavAttrTid, "wavAttr.ymult",
              HOFFSET(struct waveform_attribute, ymult), doubleArrayTid);
    H5Tinsert(wavAttrTid, "wavAttr.yoff",
              HOFFSET(struct waveform_attribute, yoff), doubleArrayTid);
    H5Tinsert(wavAttrTid, "wavAttr.yzero",
              HOFFSET(struct waveform_attribute, yzero), doubleArrayTid);

    wavAttrAid = H5Aopen_by_name(wavFile->waveFid, "/", "Waveform Attributes",
                                 H5P_DEFAULT, H5P_DEFAULT);
    ret = H5Aread(wavAttrAid, wavAttrTid, wavAttr);

    H5Aclose(wavAttrAid);
    H5Tclose(wavAttrTid);
    H5Tclose(doubleArrayTid);

    return (int)ret;
}

int hdf5io_write_event(struct hdf5io_waveform_file *wavFile,
                       struct hdf5io_waveform_event *wavEvent)
{
    char buf[HDF5IO_NAME_BUF_SIZE];
    herr_t ret;
    hid_t eventGid, chSid, chDid, chPid;
    hsize_t chDims[1], chChunkDims[1];
    
    int ich;

    snprintf(buf, HDF5IO_NAME_BUF_SIZE, "/Event%d", wavEvent->eventId);
    eventGid = H5Gcreate(wavFile->waveFid, buf, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    
    for(ich=0; ich<wavEvent->nch; ich++) {
        if((wavEvent->chMask >> ich) & 0x01) {
            chDims[0] = wavEvent->waveSize;
            chSid = H5Screate_simple(1, chDims, NULL);

            chPid = H5Pcreate(H5P_DATASET_CREATE);
            chChunkDims[0] = chDims[0];
            H5Pset_chunk(chPid, 1, chChunkDims);
            H5Pset_deflate(chPid, 6);

            snprintf(buf, HDF5IO_NAME_BUF_SIZE, "Ch%d", ich);
            chDid = H5Dcreate(eventGid, buf, H5T_NATIVE_CHAR, chSid,
                              H5P_DEFAULT, chPid, H5P_DEFAULT);
            ret = H5Dwrite(chDid, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                           wavEvent->wavBuf[ich]);
            H5Dclose(chDid);
            H5Pclose(chPid);
            H5Sclose(chSid);
        }
    }
    
    H5Gclose(eventGid);
    return (int)ret;
}

int hdf5io_read_event(struct hdf5io_waveform_file *wavFile,
                      struct hdf5io_waveform_event *wavEvent)
{
    char buf[HDF5IO_NAME_BUF_SIZE];
    herr_t ret;
    hid_t eventGid, chDid, chDspaceId;
    hsize_t chDims[1];

    int ich;

    snprintf(buf, HDF5IO_NAME_BUF_SIZE, "/Event%d", wavEvent->eventId);
    eventGid = H5Gopen(wavFile->waveFid, buf, H5P_DEFAULT);

    for(ich=0; ich<wavEvent->nch; ich++) {
        if((wavEvent->chMask >> ich) & 0x01) {
            snprintf(buf, HDF5IO_NAME_BUF_SIZE, "Ch%d", ich);
            chDid = H5Dopen(eventGid, buf, H5P_DEFAULT);

            chDspaceId = H5Dget_space(chDid);
            H5Sget_simple_extent_dims(chDspaceId, chDims, NULL);
            wavEvent->waveSize = chDims[0];
            H5Sclose(chDspaceId);

            ret = H5Dread(chDid, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                          wavEvent->wavBuf[ich]);

            H5Dclose(chDid);
        }
    }
    
    H5Gclose(eventGid);
    return (int)ret;
}

int hdf5io_get_number_of_event(struct hdf5io_waveform_file *wavFile)
{
    herr_t ret;
    hid_t rootGid;
    H5G_info_t rootGinfo;
    int nEvents;
    
    rootGid = H5Gopen(wavFile->waveFid, "/", H5P_DEFAULT);
    ret = H5Gget_info(rootGid, &rootGinfo);
    nEvents = rootGinfo.nlinks;

    H5Gclose(rootGid);
    return nEvents;
}

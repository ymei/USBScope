Requires libusb-1
Requires hdf5-1.8

This pile of code is intended to read waveforms from oscilloscopes for
analysis in computer.

It is known to work with Tektronix DPO2024 and TDS2024B oscilloscopes,
from both Linux (2.6 kernel, 2011) and Mac OS X Lion (10.7), through a
USB cable.

Do whatever you want with the code.

Yuan Mei

###############################################################################
RANDOM NOTES

TDS2024B doesn't allow transferring multiple channels at once,
therefore, after each trigger, the acq has to be stopped to allow data
transfer of all channels to complete.

Don't forget to turn on all the channels before starting the program.

Note on HDF5 when storing events:

HDF5 file seems to carry a significant overhead when a lot of
Event?/Ch?  `Groups' are created.  When waveforms are short (i.e.,
2500 bytes), the overhead can equal the truly useful data size, and
chunked dataset compression won't reduce the overhead.  Therefore, for
future improvement, events are preferred to be stored collectively in
a multi-dimensional dataspace.

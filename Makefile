CC=gcc
CFLAGS=-Wall -DH5_NO_DEPRECATED_SYMBOLS
INCLUDE=-I/opt/local/include
LIBS=-L/opt/local/lib -lusb-1.0 -lhdf5

dpo2024: main1.c usbtmc.o hdf5io.o
	$(CC) $(CFLAGS) $(INCLUDE) $^ $(LIBS) $(LDFLAGS) -o $@
tds2024b: main.c usbtmc.o hdf5io.o
	$(CC) $(CFLAGS) $(INCLUDE) $^ $(LIBS) $(LDFLAGS) -o $@
analyze_spe: analysis/analyze_spe.c hdf5io.o
	$(CC) $(CFLAGS) $(INCLUDE) $^ $(LIBS) $(LDFLAGS) -o $@
analyze_int: analysis/analyze_int.c hdf5io.o
	$(CC) $(CFLAGS) $(INCLUDE) $^ $(LIBS) $(LDFLAGS) -o $@
wavedump: analysis/wavedump.c hdf5io.o
	$(CC) $(CFLAGS) $(INCLUDE) $^ $(LIBS) $(LDFLAGS) -o $@
hdf5io.o: hdf5io.c hdf5io.h
	$(CC) $(CFLAGS) $(INCLUDE) -c $<
usbtmc.o: usbtmc.c usbtmc.h
	$(CC) $(CFLAGS) $(INCLUDE) -c $<
usbtmc: usbtmc.c usbtmc.h
	$(CC) $(CFLAGS) $(INCLUDE) -DUSBTMC_DEBUG_ENABLEMAIN $< $(LIBS) $(LDFLAGS) -o $@

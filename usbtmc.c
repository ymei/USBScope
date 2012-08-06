#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <libusb-1.0/libusb.h>
#include "usbtmc.h"

// USB488 message IDs
#define DEV_DEP_MSG_OUT 1
#define REQUEST_DEV_DEP_MSG_IN 2
#define DEV_DEP_MSG_IN 2

#define IOBUFFER_SIZE (1024*1024)
#define DESC_BUF_SIZE 256

#ifdef USBTMC_DEBUG
  #define debug_printf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
  #define debug_printf(...) ((void)0)
#endif

#define error_printf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

static void usbtmc_inc_bTag(struct usbtmc_device_handle *usbtmcDev)
{
    (usbtmcDev->bTag)++;
    if(usbtmcDev->bTag == 0) (usbtmcDev->bTag)++;
}

struct usbtmc_device_handle *
usbtmc_open_device(int vendorID, int productID)
{
    struct usbtmc_device_handle *usbtmcDev;
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    libusb_device_handle *devHandle; //a device handle
    libusb_context *ctx = NULL; //a libusb session

    struct libusb_device_descriptor devDesc;
    struct libusb_config_descriptor *configDesc;
    const struct libusb_endpoint_descriptor *endpointDesc;
    const struct libusb_interface_descriptor *interfaceDesc;

    unsigned char descBufSerial[DESC_BUF_SIZE], descBufManufacturer[DESC_BUF_SIZE],
        descBufProduct[DESC_BUF_SIZE], descBufConfig[DESC_BUF_SIZE];
    int i, ret; //for return values
    ssize_t cnt; //holding number of devices in list

    usbtmcDev = (struct usbtmc_device_handle *)malloc(sizeof(struct usbtmc_device_handle));

    ret = libusb_init(&ctx); //initialize the library for the session we just declared
    if(ret < 0) {
        error_printf("USBTMC Init Error.\n");
        return NULL;
    }
    libusb_set_debug(ctx, 1); //set verbosity level to 3, as suggested in the documentation

    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
    if(cnt < 0) {
        error_printf("USBTMC Get Device Error.\n");
        return NULL;
    }
    debug_printf("%zd devices in list.\n", cnt);

    devHandle = libusb_open_device_with_vid_pid(ctx, vendorID, productID);
    if(devHandle == NULL) {
        error_printf("Cannot open device, VendorID=0x%04x, ProductID=0x%04x\n",
                vendorID, productID);
    } else {
        debug_printf("Device (VendorID=0x%04x, ProductID=0x%04x) opened.\n",
                vendorID, productID);
        debug_printf("Bus = 0x%04x, Address = 0x%04x.\n",
                libusb_get_bus_number(libusb_get_device(devHandle)),
                libusb_get_device_address(libusb_get_device(devHandle)));
    }
    libusb_free_device_list(devs, 1); //free the list, unref the devices in it

    if(libusb_kernel_driver_active(devHandle, 0) == 1) { //find out if kernel driver is attached
        debug_printf("Kernel driver is active.\n");
        if(libusb_detach_kernel_driver(devHandle, 0) == 0) //detach it
            debug_printf("Kernel driver detached.\n");
    }

    ret = libusb_get_device_descriptor(libusb_get_device(devHandle), &devDesc);
    if(ret < 0) {
        error_printf("Cannot get device descriptor.\n");
    }
    libusb_get_string_descriptor_ascii(devHandle, devDesc.iManufacturer,
                                       descBufManufacturer, DESC_BUF_SIZE);
    libusb_get_string_descriptor_ascii(devHandle, devDesc.iProduct,
                                       descBufProduct, DESC_BUF_SIZE);
    libusb_get_string_descriptor_ascii(devHandle, devDesc.iSerialNumber,
                                       descBufSerial, DESC_BUF_SIZE);
    debug_printf(
            "USB spec release number: 0x%08x\n"
            "%s\n"
            "Vendor ID:               0x%04x\n"
            "%s\n"
            "Product ID:              0x%04x\n"
            "Serial:                  %s\n"
            "Device release number:   0x%08x\n"
            "Device Class:            0x%04x\n"
            "Device Sub Class:        0x%04x\n"
            "Device Protocol:         0x%04x\n"
            "EndPoint 0 maxPacketSize: %d\n"
            "Number of configurations: %d\n",
            devDesc.bcdUSB,
            descBufManufacturer,
            devDesc.idVendor,
            descBufProduct,
            devDesc.idProduct,
            descBufSerial,
            devDesc.bcdDevice,
            devDesc.bDeviceClass,
            devDesc.bDeviceSubClass,
            devDesc.bDeviceProtocol,
            devDesc.bMaxPacketSize0,
            devDesc.bNumConfigurations
        );

    ret = libusb_get_active_config_descriptor(libusb_get_device(devHandle), &configDesc);
    if(ret < 0) {
        error_printf("Cannot get active config descriptor.\n");
    }
    libusb_get_string_descriptor_ascii(devHandle, configDesc->iConfiguration,
                                       descBufConfig, DESC_BUF_SIZE);
    debug_printf(
            "Configuration ID:        0x%04x\n"
            "Number of interfaces:    %d\n"
            "Number of alt settings:  %d\n"
            "Max Power:               %d\n",
            configDesc->bConfigurationValue,
            configDesc->bNumInterfaces,
            configDesc->interface->num_altsetting,
            configDesc->MaxPower
        );

    interfaceDesc = configDesc->interface->altsetting;
    debug_printf(
            "Number of endpoints:     %d\n",
            interfaceDesc->bNumEndpoints
        );
    for(i=0; i<interfaceDesc->bNumEndpoints; i++) {
        endpointDesc = interfaceDesc->endpoint + i;
        debug_printf(
                "End point 0x%04x, Attr: 0x%04x, maxPacketSize: %d\n",
                endpointDesc->bEndpointAddress,
                endpointDesc->bmAttributes,
                endpointDesc->wMaxPacketSize
            );
        if((endpointDesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            if(endpointDesc->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK) {
                usbtmcDev->epBulkout = endpointDesc->bEndpointAddress;
                usbtmcDev->outMaxPacketSize = endpointDesc->wMaxPacketSize;
            }
        } else if(endpointDesc->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK) {
            usbtmcDev->epBulkin = endpointDesc->bEndpointAddress;
            usbtmcDev->inMaxPacketSize = endpointDesc->wMaxPacketSize;
        } else if(endpointDesc->bmAttributes == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
            usbtmcDev->epInt = endpointDesc->bEndpointAddress;
        }
    }

    debug_printf(
        "epBulkout: 0x%04x\n"
        "epBulkin:  0x%04x\n"
        "epInt:     0x%04x\n",
        usbtmcDev->epBulkout,
        usbtmcDev->epBulkin,
        usbtmcDev->epInt
        );

    libusb_free_config_descriptor(configDesc);

    ret = libusb_claim_interface(devHandle, 0); //claim interface 0 (the first) of device
    if(ret < 0) {
        error_printf("Cannot claim interface 0.\n");
        return NULL;
    }
    debug_printf("Interface 0 claimed.\n");

    usbtmcDev->devHandle = devHandle;
    usbtmcDev->devContext = ctx;
    usbtmcDev->bTag = 1;

    return usbtmcDev;
}

int usbtmc_close_device(struct usbtmc_device_handle *usbtmcDev)
{
    int ret;
    
    ret = libusb_release_interface(usbtmcDev->devHandle, 0); //release the claimed interface 0
    if(ret) {
        error_printf("Cannot release interface 0.\n");
    }
    debug_printf("Interface 0 released.\n");

    libusb_close(usbtmcDev->devHandle); //close the device we opened
    libusb_exit(usbtmcDev->devContext); //needs to be called to end

    free(usbtmcDev);

    return 0;
}

int usbtmc_clear(struct usbtmc_device_handle *usbtmcDev)
{
    libusb_clear_halt(usbtmcDev->devHandle, usbtmcDev->epBulkout);
    libusb_clear_halt(usbtmcDev->devHandle, usbtmcDev->epBulkin);
    libusb_clear_halt(usbtmcDev->devHandle, usbtmcDev->epInt);
    return 0;
}
    
int usbtmc_write(struct usbtmc_device_handle *usbtmcDev, const char *cmd)
{
    int i, ret, dataLen, actualLen, padLen, remLen;
    size_t size;
    unsigned char data[IOBUFFER_SIZE];

    size = strlen(cmd);
    if(size > IOBUFFER_SIZE-13) { //leaving space for possible addition of '\n'
        error_printf("%s : cmd too long.  Not sending.\n", __FUNCTION__);
        return -1;
    }
    
    strncpy((char*)(data+12), cmd, size);
    if(data[size+11] != '\n') {
        data[size+12] = '\n';
        size++;
    }

    data[0] = DEV_DEP_MSG_OUT;
    data[1] = usbtmcDev->bTag;
    data[2] = ~(usbtmcDev->bTag); usbtmc_inc_bTag(usbtmcDev);
    data[3] = 0x00;

    data[4] = size;
    data[5] = size>>8;
    data[6] = size>>16;
    data[7] = size>>24;

    data[8] = 0x01;

    data[9] = 0x00; data[10] = 0x00; data[11] = 0x00;

    dataLen = size+12;
    padLen = 4 - dataLen % 4; if(padLen == 4) padLen = 0;

    for(i=dataLen; i<dataLen + padLen; i++)
        data[i] = 0x00;
    dataLen += padLen;

    remLen = dataLen;
    i = 0;
    while(remLen > 0) {
        if(remLen > usbtmcDev->outMaxPacketSize)
            padLen = usbtmcDev->outMaxPacketSize;
        else
            padLen = remLen;
        ret = libusb_bulk_transfer(usbtmcDev->devHandle,
                                   usbtmcDev->epBulkout,
                                   data + i, padLen,
                                   &actualLen, 0);
        debug_printf("%s: size = %zd, dataLen = %d, remLen=%d, actualLen = %d\n", __FUNCTION__,
                size, dataLen, remLen, actualLen);
        i += padLen;
        remLen -= padLen;
    }
    return remLen;
}

int usbtmc_read(struct usbtmc_device_handle *usbtmcDev, unsigned char *retData, int askLen)
{
    int i, ret, dataLen, actualLen;

    unsigned char data[IOBUFFER_SIZE], expTag;
    size_t size = IOBUFFER_SIZE - 12;
    
    data[0] = REQUEST_DEV_DEP_MSG_IN;
    data[1] = usbtmcDev->bTag; expTag = usbtmcDev->bTag;
    data[2] = ~(usbtmcDev->bTag); usbtmc_inc_bTag(usbtmcDev);
    data[3] = 0x00;

    data[4] = size;
    data[5] = size>>8;
    data[6] = size>>16;
    data[7] = size>>24;

    data[8] = 0x00; data[9] = 0x00; data[10] = 0x00; data[11] = 0x00;

    dataLen = 12;

    ret = libusb_bulk_transfer(usbtmcDev->devHandle,
                               usbtmcDev->epBulkout,
                               data, dataLen,
                               &actualLen, 0);
    if((ret < 0) || (dataLen != actualLen)) {
        error_printf("%s: dataLen = %d, actualLen = %d, write error.\n",
                __FUNCTION__, dataLen, actualLen);
        return ret;
    }

    ret = libusb_bulk_transfer(usbtmcDev->devHandle,
                               usbtmcDev->epBulkin,
                               data, askLen+12,
                               &actualLen, 0);
    if(data[0] != DEV_DEP_MSG_IN) {
        error_printf("%s: data[0] != DEV_DEP_MSG_IN\n", __FUNCTION__);
    }
    if(data[1] != expTag) {
        error_printf("%s: data[1] != expected tag (0x%04x)\n", __FUNCTION__, expTag);
    }
    // actual useful data size
    size = data[4] | data[5]<<8 | data[6]<<16 | data[7]<<24;
    debug_printf("%s: read ret = %d, askLen = %d, actualLen = %d, datasize = %zd\n",
                 __FUNCTION__, ret, askLen, actualLen, size);

    if(retData != NULL) {
        memcpy(retData, data+12, size);
    } else {
        for(i=12; i<12+size; i++) {
            printf("%c", data[i]);
        }
        // printf("\n");
    }
    
    return size;
}

#ifdef USBTMC_DEBUG_ENABLEMAIN
int main(int argc, char **argv)
{
    struct usbtmc_device_handle *usbtmcDev;

    char buf[3000] = {0};
    int i,j,k, retDataSize;
    FILE *fp;

    if((fp=fopen("log.dat", "w"))==NULL) {
        perror("log.dat");
    }

    usbtmcDev = usbtmc_open_device(0x0699, 0x036a); //Tektronix TDS2024B
//    usbtmcDev = open_usbtmc_device(0x0403, 0x6001);

    usbtmc_clear(usbtmcDev);
    while(buf[0]!='q') {
        fgets(buf, sizeof(buf), stdin);
        usbtmc_write(usbtmcDev, buf);
        usbtmc_read(usbtmcDev, NULL, sizeof(buf));
        puts(buf);
        fputs(buf, fp);
    }

    usbtmc_close_device(usbtmcDev);
    fclose(fp);
    return EXIT_SUCCESS;

#if 0
    struct usbtmc_device_handle *usbtmcDev;

    char wav[3000];
    int i,j,k, retDataSize;
    FILE *fp;

    if((fp=fopen("wav.dat", "w"))==NULL) {
        perror("wav.dat");
    }

    usbtmcDev = usbtmc_open_device(0x0699, 0x036a); //Tektronix TDS2024B
//    usbtmcDev = open_usbtmc_device(0x0403, 0x6001);

    usbtmc_clear(usbtmcDev);

    usbtmc_write(usbtmcDev, "*CLS;*IDN?");
    usbtmc_read(usbtmcDev, NULL, 3000);

    usbtmc_write(usbtmcDev, "DATA INIT");
    usbtmc_write(usbtmcDev, "DATA:START 1150");
    usbtmc_write(usbtmcDev, "DATA:STOP 1350");
    usbtmc_write(usbtmcDev, "DATA?");
    usbtmc_read(usbtmcDev, NULL, 3000);
    usbtmc_write(usbtmcDev, "ACQUIRE:STOPAFTER SEQUENCE");
    usbtmc_write(usbtmcDev, "ACQUIRE?");
    usbtmc_read(usbtmcDev, NULL, 3000);

    printf("start time = %zd\n", time(NULL));
    
    for(j=0; j<10; j++) {
        usbtmc_write(usbtmcDev, "ACQUIRE:STATE RUN");
        usbtmc_write(usbtmcDev, "DATA:SOURCE CH1");
        usbtmc_write(usbtmcDev, "CURVE?");
        for(k=0; k<1; k++) {
            retDataSize = usbtmc_read(usbtmcDev, (unsigned char*)wav, 1100);
            for(i=0; i<retDataSize; i++) {
                fprintf(fp, "%d\n", wav[i]);
            }
        }
        fprintf(fp, "\n");

        usbtmc_write(usbtmcDev, "DATA:SOURCE CH2");
        usbtmc_write(usbtmcDev, "CURVE?");
        for(k=0; k<1; k++) {
            retDataSize = usbtmc_read(usbtmcDev, (unsigned char*)wav, 1100);
            for(i=0; i<retDataSize; i++) {
                fprintf(fp, "%d\n", wav[i]);
            }
        }
        fprintf(fp, "\n\n");
    }

    printf("stop time  = %zd\n", time(NULL));
    
    usbtmc_close_device(usbtmcDev);
    fclose(fp);
    return EXIT_SUCCESS;
#endif
}
#endif

#ifndef __USBTMC_H__
#define __USBTMC_H__

#include <libusb-1.0/libusb.h>

struct usbtmc_device_handle
{
    libusb_device_handle *devHandle; //a device handle
    libusb_context *devContext; //a libusb session
    int outMaxPacketSize;
    int inMaxPacketSize;
    unsigned char bTag;
    unsigned char epBulkout;
    unsigned char epBulkin;
    unsigned char epInt;
};

struct usbtmc_device_handle *usbtmc_open_device(int vendorID, int productID);
int usbtmc_close_device(struct usbtmc_device_handle *usbtmcDev);
int usbtmc_clear(struct usbtmc_device_handle *usbtmcDev);
int usbtmc_write(struct usbtmc_device_handle *usbtmcDev, const char *cmd);
int usbtmc_read(struct usbtmc_device_handle *usbtmcDev, unsigned char *retData, int askLen);

#endif

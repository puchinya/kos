/*
 *	@brief	usb2.0
 *	@author	nabeshima masataka
 */
#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	
#ifdef __ARMCC_VERSION
#pragma pack(1)
#endif
	
/*-----------------------------------------------------------------------------
	USB Device Requests
-----------------------------------------------------------------------------*/

#ifndef __cplusplus
typedef struct usb_device_request_t usb_device_request_t;
#endif
	
#define usb_device_request_size	8

struct usb_device_request_t
{
	uint8_t		bmRequestType;
	uint8_t		bRequest;
	uint16_t	wValue;
	uint16_t	wIndex;
	uint16_t	wLength;
};

/* Standard Device Request Types */
#define USB_REQUEST_TYPE_HOST_TO_DEVICE				(0<<7)
#define USB_REQUEST_TYPE_DEVICE_TO_HOST				(1<<7)
#define USB_REQUEST_TYPE_STANDARD							(0<<5)
#define USB_REQUEST_TYPE_CLASS								(1<<5)
#define USB_REQUEST_TYPE_VENDOR								(2<<5)
#define USB_REQUEST_TYPE_DEVICE								(0)
#define USB_REQUEST_TYPE_INTERFACE						(1)
#define USB_REQUEST_TYPE_ENDPOINT							(2)
#define USB_REQUEST_TYPE_OTHER								(3)

/* Standard Request Codes */
#define USB_REQUEST_CODE_GET_STATUS						0
#define USB_REQUEST_CODE_CLEAR_FEATURE				1
#define USB_REQUEST_CODE_SET_FEATURE					3
#define USB_REQUEST_CODE_SET_ADDRESS					5
#define USB_REQUEST_CODE_GET_DESCRIPTOR				6
#define USB_REQUEST_CODE_SET_DESCRIPTOR				7
#define USB_REQUEST_CODE_GET_CONFIG						8
#define USB_REQUEST_CODE_SET_CONFIG						9
#define USB_REQUEST_CODE_GET_INTERFACE				10
#define USB_REQUEST_CODE_SET_INTERFACE				11
#define USB_REQUEST_CODE_SYNCH_FRAME					12

/*-----------------------------------------------------------------------------
	Standard Descriptors
-----------------------------------------------------------------------------*/

/* Descriptor Types */
#define USB_DESCRIPTOR_TYPE_DEVICE							1
#define USB_DESCRIPTOR_TYPE_CONFIG							2
#define USB_DESCRIPTOR_TYPE_STRING							3
#define USB_DESCRIPTOR_TYPE_INTERFACE						4
#define USB_DESCRIPTOR_TYPE_ENDPOINT						5
#define USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER		6
#define USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIG	7
#define USB_DESCRIPTOR_TYPE_INTERFACE_POWER			8

#ifndef __cplusplus
typedef struct usb_device_descriptor_t usb_device_descriptor_t;
typedef struct usb_device_qualifier_t usb_device_qualifier_t;
typedef struct usb_config_descriptor_t usb_config_descriptor_t;
typedef struct usb_other_speed_config_descriptor_t usb_other_speed_config_descriptor_t;
typedef struct usb_interface_descriptor_t usb_interface_descriptor_t;
typedef struct usb_endpoint_descriptor_t usb_endpoint_descriptor_t;
typedef struct usb_string_descriptor_t usb_string_descriptor_t;
#endif
	
#define usb_device_descriptor_size	18
	
struct usb_device_descriptor_t {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	bcdUSB;
	uint8_t		bDeviceClass;
	uint8_t		bDeviceSubClass;
	uint8_t		bDeviceProtocol;
	uint8_t		bMaxPacketSize0;
	uint16_t	idVendor;
	uint16_t	idProduct;
	uint16_t	bcdDevice;
	uint8_t		iManufacture;
	uint8_t		iProduct;
	uint8_t		iSerialNumber;
	uint8_t		bNumConfigurations;
};

#define usb_device_qualifier_size	10

struct usb_device_qualifier_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	bcdUSB;
	uint8_t		bDeviceClass;
	uint8_t		bDeviceSubClass;
	uint8_t		bDeviceProtocol;
	uint8_t		bMaxPacketSize0;
	uint8_t		bNumConfigurations;
	uint8_t		bReserved;
};

#define usb_configu_descriptor_size 9

struct usb_config_descriptor_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	wTotalLength;
	uint8_t		bNumInterfaces;
	uint8_t		bConfigurationValue;
	uint8_t		iConfiguration;
	uint8_t		bmAttributes;
	uint8_t		bMaxPower;
};

#define usb_other_speed_config_descriptor_size 9

struct usb_other_speed_config_descriptor_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	wTotalLength;
	uint8_t		bNumInterfaces;
	uint8_t		bConfigurationValue;
	uint8_t		iConfiguration;
	uint8_t		bmAttributes;
	uint8_t		bMaxPower;
};

#define usb_interface_descriptor_size 9

struct usb_interface_descriptor_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bInterfaceNumber;
	uint8_t		bAlternateSetting;
	uint8_t		bNumEndpoints;
	uint8_t		bInterfaceClass;
	uint8_t		bInterfaceSubClass;
	uint8_t		bInterfaceProtocol;
	uint8_t		iInterface;
};

#define usb_endpoint_descriptor_size 7

struct usb_endpoint_descriptor_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bEndpointAddress;
	uint8_t		bmAttributes;
	uint16_t	wMaxPacketSize;
	uint8_t		bInterval;
};

struct usb_string_descriptor_t
{
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	wLANGID[1];
};

/*-----------------------------------------------------------------------------
	Standard Device Classes
-----------------------------------------------------------------------------*/
#define USB_DEVICE_CLASS_COMMINICATIONS

#ifdef __ARMCC_VERSION
#pragma pack()
#endif

#ifdef __cplusplus
};
#endif

#endif

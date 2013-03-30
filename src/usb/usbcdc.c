
#include "usbcdc.h"

#define USBCDC_SET_LINE_CODING		0x20
#define USBCDC_GET_LINE_CODING		0x21
#define USBCDC_CONTROL_LINE_STATE	0x22

static int usbcdc_process_device_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request);
static void usbcdc_process_device_request_get_descriptor(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request);
static void usbcdc_process_class_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request);
static unsigned int usbcdc_min(unsigned int x, unsigned int y);

static const uint8_t usbcdc_dev_desc[18] =
{
	0x12,
	0x01,
	0x10, 0x01,
	0x02,
	0x00,
	0x00,
	0x40,
	0xff, 0x26,
	0x12, 0x49,
	0x00, 0x00,
	0x01,
	0x02,
	0x03,
	0x01
};

static const uint8_t usbcdc_cfg_desc[] =
{

	/* config */
	//
		0x09,
		0x02,
		0x43, 0x00,
		0x02,
		0x01,
		0x00,
		0x80,
		0x32,
	//
	
	/* interface 0 */
	//
		0x09,
		0x04,
		0x00,
		0x00,
		0x01,
		0x02,
		0x02,
		0x00,
		0x00,
	//
	
	//
		0x05,
		0x24,
		0x00,
		0x10, 0x01,
	//
	
	//
		0x05,
		0x24,
		0x06,
		0x00,
		0x01,
	//
	
	//
		0x05,
		0x24,
		0x01,
		0x00,
		0x01,
	//
	
	//
		0x04,
		0x24,
		0x02,
		0x02,
	//
	
	/* endpoint */
	//
		0x07,
		0x05,
		0x85,
		0x03,
		0x10, 0x00,
		0x01,
	//
	
	/* interface 1 */
	//
		0x09,
		0x04,
		0x01,
		0x00,
		0x02,
		0x0a,
		0x00,
		0x00,
		0x00,
	//
	
	/* endpoint */
	//
		0x07,
		0x05,
		0x01,
		0x02,
		0x40, 0x00,
		0x00,
	//
	
	/* endpoint */
	//
		0x07,
		0x05,
		0x82,
		0x02,
		0x40, 0x00,
		0x0
	//
};

static const uint8_t usbcdc_string_desc0[] =
{
	0x04, 0x03, 0x09, 0x04
};

static const uint8_t usbcdc_string_desc1[] =
{
	10,
	0x03,
	'K', 0x00,
	'U', 0x00,
	'M', 0x00,
	'A', 0x00
};

static const uint8_t usbcdc_string_desc2[] =
{
	10,
	0x03,
	'V', 0x00,
	'C', 0x00,
	'O', 0x00,
	'M', 0x00
};

static const uint8_t usbcdc_string_desc3[] =
{
	18,
	0x03,
	'0', 0x00,
	'0', 0x00,
	'0', 0x00,
	'0', 0x00,
	'0', 0x00,
	'0', 0x00,
	'0', 0x00,
	'1', 0x00
};

struct usbcdc_t
{
	usbdrv_dev_t	*dev;
	unsigned int	ctrl_line;
	uint8_t				line_code[7];
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_id_t			w_semid;
	kos_id_t			r_semid;
#endif
};

usbcdc_t *usbcdc_create(int usb_port)
{
	usbdrv_dev_t *dev;
	static usbcdc_t cdc;
	
	dev = usbdrv_create(usb_port);
	
	cdc.dev = dev;
	cdc.ctrl_line = 0;
	cdc.line_code[0] = 0xb0;
	cdc.line_code[1] = 0x04;
	cdc.line_code[2] = 0x00;
	cdc.line_code[3] = 0x00;
	cdc.line_code[4] = 0x00;
	cdc.line_code[5] = 0x00;
	cdc.line_code[6] = 0x08;
	
	dev->func_table.device_request_func = usbcdc_process_device_request;
	
#ifdef USBDRV_CFG_SUPPORT_KOS
	{
		kos_csem_t csem;
		csem.sematr = 0;
		csem.isemcnt = 1;
		csem.maxsem = 1;
		cdc.r_semid = kos_cre_sem(&csem);
		cdc.w_semid = kos_cre_sem(&csem);
	}
#endif
	return &cdc;
}

int usbcdc_open(usbcdc_t *cdc)
{
	usbdrv_begin_connect_setting(cdc->dev);
	usbdrv_config_ep(cdc->dev, usbcdc_cfg_desc);
	usbdrv_end_connect_setting(cdc->dev);
	return 0;
}

int usbcdc_close(usbcdc_t *cdc)
{
	return 0;
}

int usbcdc_write(usbcdc_t *cdc, const void *buf, unsigned int buf_size)
{
#ifdef USBDRV_CFG_SUPPORT_KOS
	int r;
	kos_wai_sem(cdc->w_semid);
	r = usbdrv_ep_write(&cdc->dev->ep[2-1], (const uint8_t *)buf, buf_size);
	kos_sig_sem(cdc->w_semid);
	return r;
#else
	return usbdrv_ep_write(&cdc->dev->ep[2-1], (const uint8_t *)buf, buf_size);
#endif
}

int usbcdc_read(usbcdc_t *cdc, void *buf, unsigned int buf_size)
{
#ifdef USBDRV_CFG_SUPPORT_KOS
	int r;
	kos_wai_sem(cdc->r_semid);
	r = usbdrv_ep_read(&cdc->dev->ep[1-1], (uint8_t *)buf, buf_size);
	kos_sig_sem(cdc->r_semid);
	return r;
#else
	return usbdrv_ep_read(&cdc->dev->ep[1-1], (uint8_t *)buf, buf_size);
#endif
}

static unsigned int usbcdc_min(unsigned int x, unsigned int y)
{
	return x < y ? x : y;
}

static void usbcdc_process_device_request_get_descriptor(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	unsigned int wLength;
	wLength = device_request->wLength;
	switch(((uint8_t *)&device_request->wValue)[1])
	{
	case 0x01:
		usbdrv_ep0_begin_write(dev, usbcdc_dev_desc,
			usbcdc_min(usbcdc_dev_desc[0], wLength));
		break;
	case 0x02:
		usbdrv_ep0_begin_write(dev, usbcdc_cfg_desc,
			usbcdc_min(((unsigned int)usbcdc_cfg_desc[3] << 8) | usbcdc_cfg_desc[2],
				wLength));
		break;
	case 0x03:
		switch(((uint8_t *)&device_request->wValue)[0]) {
		case 0x00:
			usbdrv_ep0_begin_write(dev, usbcdc_string_desc0,
				usbcdc_min(usbcdc_string_desc0[0], wLength));
			break;
		case 0x01:
			usbdrv_ep0_begin_write(dev, usbcdc_string_desc1,
				usbcdc_min(usbcdc_string_desc1[0], wLength));
			break;
		case 0x02:
			usbdrv_ep0_begin_write(dev, usbcdc_string_desc2,
				usbcdc_min(usbcdc_string_desc2[0], wLength));
			break;
		case 0x03:
		default:
			usbdrv_ep0_begin_write(dev, usbcdc_string_desc3,
				usbcdc_min(usbcdc_string_desc3[0], wLength));
			break;
		}
		break;
	default:
		break;
	}
}

static void usbcdc_process_class_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	usbcdc_t *cdc;
	unsigned int wLength;
	wLength = device_request->wLength;
	
	cdc = (usbcdc_t *)dev->unique_data;
	
	switch(device_request->bRequest)
	{
	case USBCDC_SET_LINE_CODING:
		usbdrv_ep0_begin_read(dev, cdc->line_code, 7);
		break;
	case USBCDC_GET_LINE_CODING:
		usbdrv_ep0_begin_write(dev, cdc->line_code, usbcdc_min(7, wLength));
		break;
	case USBCDC_CONTROL_LINE_STATE:
		cdc->ctrl_line = wLength;
	default:
		break;
	}
}

static int usbcdc_process_device_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	switch((device_request->bmRequestType >> 5) & 3) {
	case 0:	/* Standard */
		switch(device_request->bRequest) {
		case USB_REQUEST_CODE_GET_DESCRIPTOR:
			usbcdc_process_device_request_get_descriptor(dev, device_request);
			break;
		default:
			break;
		}
		break;
	case 1: /* Class */
		usbcdc_process_class_request(dev, device_request);
		break;
	case 2: /* Vendor */
		break;
	default:
		break;
	}
	
	return 0;
}


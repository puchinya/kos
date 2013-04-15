/* user initalize code. */
#include <usbcdc.h>
#include <ksh.h>
#include <led_drv.h>
#include <usbhdrv.h>

static int s_usbhost_tsk_stk[0x400/sizeof(int)];

static void usbhost_tsk(kos_vp_t exinf)
{
	usbdrv_er_t er;
	usbhdrv_dev_t *dev;
	
	dev = usbhdrv_open(1);
	
	/* デバイスの接続を待つ*/
	usbhdrv_wait_for_device(dev);
	
	/* デバイスディスクプリタを取得 */
	{
		usb_device_request_t device_request;
		uint16_t data[0x12/2];
		
		device_request.bmRequestType = USB_REQUEST_TYPE_DEVICE_TO_HOST;
		device_request.bRequest = USB_REQUEST_CODE_GET_DESCRIPTOR;
		device_request.wValue = USB_DESCRIPTOR_TYPE_DEVICE;
		device_request.wIndex = 0;
		device_request.wLength = 0x8;
		
		usbhdrv_send_sof_token(dev);
		usbhdrv_setup_transaction(dev, &device_request);
		usbhdrv_begin_in_transaction(dev, 0);
		usbhdrv_in_transaction(dev, 0, data, 0x8);
		usbhdrv_end_in_transaction(dev, 0);
		
	}
	
	
}

const kos_ctsk_t s_ctsk_usbhost_tsk =
{
	0,
	0,
	usbhost_tsk,
	2,
	sizeof(s_usbhost_tsk_stk),
	s_usbhost_tsk_stk
};

static kos_id_t s_usbhost_tsk_id = 0;

void start_usbh(void)
{
	s_usbhost_tsk_id = kos_cre_tsk(&s_ctsk_usbhost_tsk);
	kos_act_tsk(s_usbhost_tsk_id);
}

/* This function is called by kernel. */
void kos_usr_init(void)
{
	init_led();
	usbdrv_init();
	
	ksh_start();
	
	start_usbh();
}

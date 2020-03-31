#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/usb.h>
#include<linux/timer.h>
#include<linux/slab.h>

#define log(fmt,...) ({ printk(KERN_INFO "[%s]:-->  ",__func__); \
		printk(fmt, ##__VA_ARGS__); \
})

#define SANDISK_VID 		 0x0781
#define SANDISK_PID 		 0x5567
#define HP_VID 				 0x0951
#define HP_PID 				 0x1643



#define READ_CAPACITY_LENGTH 0x08
#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])
#define MAX_RETRY 5

/********************************************************************************************************************/


enum ep_directions {
	USB_EP_IN = 0x80,
	USB_EP_OUT = 0x00
};



struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};


static uint8_t cmd_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};

int send_command(struct usb_device*,uint8_t, uint8_t, uint8_t*, uint8_t, uint8_t, uint8_t*);
int get_csw(struct usb_device*, uint8_t,uint8_t);
int read_capacity(struct usb_device *, uint8_t, uint8_t);

/********************************************************************************************************************/





static void usbdev_disconnect(struct usb_interface *interface)
{
	log("USBDEV Device Removed\n");
	return;
}

static struct usb_device_id usbdev_table [] = {
	{USB_DEVICE(SANDISK_VID, SANDISK_PID)},
	{USB_DEVICE(HP_VID, HP_PID)},
	{} /*terminating entry*/	
};



int read_capacity(struct usb_device *device, uint8_t epin, uint8_t epout)
{
	uint8_t command[16] = {0};
	uint8_t *buffer = NULL;
	uint8_t lun = 0; 				// logical device number among multiple devices
	uint8_t tag;
	int ret;
	int size = 0;
	long max_lba,block_size;
	long device_size = 99;

	if ( !(buffer = (uint8_t *)kmalloc(sizeof(uint8_t)*64, GFP_KERNEL)) ) {
		log("Cannot allocate memory for rcv buffer\n");
		return -1;
	}

	command[0] = 0x25; 				// read capacity
	if( send_command(device,epout,lun,command,USB_EP_IN,READ_CAPACITY_LENGTH,&tag) != 0 ) {
		log("Send command error\n");
		return -1;
	}

	// SENDING THE COMMAND TO RECEIVE DATA
	ret = usb_bulk_msg(device, usb_rcvbulkpipe(device,epin), (unsigned char*)buffer, READ_CAPACITY_LENGTH, &size, 1000);

	if ( ret !=0 ) {
		log("Reading endpoint command error\n");
		return -1;
	}

	max_lba = be_to_int32(buffer);
	block_size = be_to_int32(buffer+4);
	device_size = ((max_lba+1))*block_size/(1024*1024*1024); 
	log("SIZE OF PENDRIVE -->  %ld GB\n", device_size);
	
	kfree(buffer);
	if ( get_csw(device,epin,tag) == -1 ) {
		log("cannot get command status block\n");
		return -1;
	}


	return 0;
}



int get_csw(struct usb_device *device, uint8_t ep, uint8_t tag)
{
	int retry = 0;
	int ret,size;
	struct command_status_wrapper *csw;

	if( !(csw = (struct command_status_wrapper*)kmalloc(sizeof(struct command_status_wrapper), GFP_KERNEL)) ) {
		log("Cannot allocate memory for command status buffer\n");
		return -1;
	}


	do {
		ret = usb_bulk_msg(device,usb_rcvbulkpipe(device,ep),(unsigned char*)csw,13,&size,1000);
		if (ret != 0 ) 
			usb_clear_halt(device,usb_sndbulkpipe(device,ep));
		retry++;
	} while ( (ret!=0) && (retry < MAX_RETRY));
	

	kfree(csw);
	return ret;
}





int send_command(struct usb_device *device,uint8_t ep, uint8_t lun, uint8_t *cmd, uint8_t dir, uint8_t data_len, uint8_t *rtag)
{
	struct command_block_wrapper *cbw = NULL;
	unsigned int tag = 100; 							
	uint8_t cmd_len;
	int ret,retry =0;
	int size = 0;
	
	cbw = (struct command_block_wrapper*)kmalloc(sizeof(struct command_block_wrapper), GFP_KERNEL);
	if ( cbw == NULL ) {
		log("Cannot allocate memory\n");
		return -1;
	}

	*rtag = tag;
	cmd_len = cmd_length[cmd[0]];

	// check if command array is valid or not
	if (cmd == NULL)
		return -1;

	// Check if valid endpoint
	if ( ep & USB_EP_IN ) {
		log(" Cannot send command on IN endpoint\n");
		return -1;
	}

	
	memset(cbw,'\0',sizeof(struct command_block_wrapper));
	cbw->dCBWSignature[0] = 'U';
	cbw->dCBWSignature[1] = 'S';
	cbw->dCBWSignature[2] = 'B';
	cbw->dCBWSignature[3] = 'C';
	cbw->dCBWTag = tag++;
	cbw->dCBWDataTransferLength = data_len;
	cbw->bmCBWFlags = dir;
	cbw->bCBWLUN = lun;
	cbw->bCBWCBLength = cmd_len;
	memcpy(cbw->CBWCB, cmd, cmd_len);

	// resetting the USB device
	usb_reset_device(device);

	// SENDING THE COMMAND
 	do {
		ret = usb_bulk_msg(device,usb_sndbulkpipe(device,ep), (unsigned char*)cbw, 31, &size, 1000);
		if( ret != 0 )
			usb_clear_halt(device,usb_sndbulkpipe(device,ep));
		retry++;
	} while( ret!=0 && (retry < 10) );


	if ( ret !=0 ) {
		log("send endpoint command error\n");
		return -1;
	}

	kfree(cbw);
	return 0;

}


static int myUSBdev_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int i;
	uint8_t epin, epout; 												// In endpoint and out endpoint address
	struct usb_endpoint_descriptor *ep_desc;
	struct usb_device *device;

	// get device structure out of interface
	device = interface_to_usbdev(interface);
	if ( device == NULL ) {
		log("cannot fetch device structure from interface structure\n");
		return -1;
	}

	// Getting VID and PID of the device from device descriptor
	log("VID of the device: %04x\n", device->descriptor.idVendor);
	log("PID of the device: %04x\n", device->descriptor.idProduct);

	epin = epout = 0;
	
	if( (id->idProduct == SANDISK_PID) || (id->idProduct == HP_PID) )
	{
		log("Known USB drive detected\n");
	}
	else {
		log("Unknown device\n");
		return -1;
	}

	// Getting other info of the interface of the device
	log("USB Interface class : %04x\n", interface->cur_altsetting->desc.bInterfaceClass);
	log("USB Interface Subclass : %04x\n", interface->cur_altsetting->desc.bInterfaceSubClass);
	log("USB Interface Protocol : %04x\n", interface->cur_altsetting->desc.bInterfaceProtocol);
	log("No. of Endpoints = %d\n", interface->cur_altsetting->desc.bNumEndpoints);
	log("No. of Altsettings = %d\n",interface->num_altsetting);

	for(i=0; i < interface->cur_altsetting->desc.bNumEndpoints; i++)
	{
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;

		if ( (ep_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) & USB_ENDPOINT_XFER_BULK ) {
			log( "Bulk transfer endpoint found\n");
			if (ep_desc->bEndpointAddress & USB_EP_IN) {
				if(!epin)
					epin = ep_desc->bEndpointAddress;
				log("IN endpoint found with address %d\n",epin);
			}
			else {
				if(!epout)
					epout = ep_desc->bEndpointAddress;
				log("OUT endpoint found with address %d\n",epout);
			}
		}
	}

	if ( read_capacity(device,epin, epout) !=0 ) {
		log("read capacity error\n");
		return -1;
	}

	return 0;
}



/*Operations structure*/
static struct usb_driver usbdev_driver = {
	name: "atul_usbdev",  											//name of the device
	probe: myUSBdev_probe, 											// Whenever Device is plugged in
	disconnect: usbdev_disconnect, 									// When we remove a device
	id_table: usbdev_table, 										//  List of devices served by this driver
};


int device_init(void)
{
	log("UAS READ Capacity Driver Inserted\n");
	usb_register(&usbdev_driver);
	return 0;
}

void device_exit(void)
{
	usb_deregister(&usbdev_driver);
	log("UAS READ Capacity Driver Removed\n");
}

module_init(device_init);
module_exit(device_exit);
MODULE_LICENSE("GPL");


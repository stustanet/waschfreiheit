/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */

#include "usb_storage_helper.h"

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	(void)pdrv;
	if (!usb_storage_ok())
	{
		return STA_NODISK;
	}

	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	(void)pdrv;
	if (!usb_storage_ok())
	{
		return STA_NODISK;
	}

	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	(void)pdrv;

	enum USB_RESULT res = usb_storage_read_wait(sector, count, buff);

	switch (res)
	{
		case USB_RESULT_OK:
			return RES_OK;

		case USB_RESULT_OUT_OF_BOUNDS:
			return RES_PARERR;

		default:
		case USB_RESULT_ERROR:
		case USB_RESULT_FATAL:
			return RES_ERROR;
	}
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	(void)pdrv;

	enum USB_RESULT res = usb_storage_write_wait(sector, count, buff);

	switch (res)
	{
		case USB_RESULT_OK:
			return RES_OK;

		case USB_RESULT_OUT_OF_BOUNDS:
			return RES_PARERR;

		default:
		case USB_RESULT_ERROR:
		case USB_RESULT_FATAL:
			return RES_ERROR;
	}
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	(void)pdrv;

	switch (cmd)
	{
		case GET_SECTOR_SIZE:
			(*(WORD *)buff) = usb_storage_sector_size();
			return RES_OK;

		case GET_SECTOR_COUNT:
			(*(DWORD *)buff) = usb_storage_sector_count();
			return RES_OK;

		default:
			return RES_PARERR;
	}
}


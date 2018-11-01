/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "usb_storage_helper.h"
#include <string.h>


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (void)
{
	// Put your code here

	// Wait for storage?
	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	BYTE* buff,		/* Pointer to the destination object */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Offset in the sector */
	UINT count		/* Byte count (bit15:destination) */
)
{
	static uint8_t buffer[4096];
	static uint32_t buffered_sector = ~0U;

	if (!usb_storage_ok() || usb_storage_sector_size() > sizeof(buffer))
	{
		// BAD: Not ready or too large sectors
		return RES_ERROR;
	}

	if (buffered_sector != sector)
	{
		// need to load more data
		enum USB_READ_RESULT res = usb_storage_read_wait(sector, 1, buffer);
		switch(res)
		{
			case USB_READ_OUT_OF_BOUNDS:
			case USB_READ_ERROR:
				return RES_PARERR;
			case USB_READ_OK:
				break;
			case USB_READ_FATAL:
			default:
				return RES_ERROR;
		}
	}

	memcpy(buff, buffer + offset, count);

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep (
	const BYTE* buff,		/* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
	DWORD sc		/* Sector number (LBA) or Number of bytes to send */
)
{
	(void)buff;
	(void)sc;
	return RES_ERROR;
}


#include "globals.h"

#define MAGIC 0x5AFE

#pragma pack(1)
static struct st_header
{
	uint16_t magic;
	uint16_t width;
	uint16_t height;
	uint8_t rle_type;
} hdr;

enum
{
	TYPE_RAW,
	TYPE_RLE1,
	TYPE_RLE2,
	NUM_RLE_TYPES
};

static int fd;
static int check_mode;

uint32_t loadImage(XImage *img);
uint32_t readPixel();
void readBytes(void *ptr, int size);

void saveHeader(string &filepath, uint8_t rle_type);
uint32_t saveImage(XImage *img);
uint32_t writeRunLength(uint32_t len);
void writePixel(uint32_t pixel);
void writeBytes(void *ptr, int size);

//////////////////////////////////// LOAD ////////////////////////////////////

tuple<XImage *,int,int,uint32_t> loadPicFile(string filepath)
{
	string matchpath;
	en_error err;
	uint32_t bytes = 0;

	err = matchLoadPath(filepath,matchpath,LOGO_PIC_FILE_EXT);
	if (err != OK) throw t_error({ err, filepath });

	if ((fd = open(matchpath.c_str(),O_RDONLY)) == -1)
		throw t_error({ ERR_OPEN_FAIL, matchpath });
	try
	{
		readBytes(&hdr,sizeof(hdr));
		bytes = sizeof(hdr);
	}
	catch(...)
	{
		throw t_error({ ERR_READ_FAIL, matchpath });
	}

	hdr.magic = ntohs(hdr.magic);
	if (hdr.magic != MAGIC)
		throw t_error({ ERR_NOT_PICTURE_FILE, matchpath });

	if (hdr.rle_type < 0 || hdr.rle_type >= NUM_RLE_TYPES)
		throw t_error({ ERR_INVALID_RLE, matchpath });

	hdr.width = ntohs(hdr.width);
	hdr.height = ntohs(hdr.height);
	xWindowResize(hdr.width,hdr.height,true);

	XImage *img = xImageCreate(hdr.width,hdr.height);

	try
	{
		bytes += loadImage(img);
	}
	catch(en_error err)
	{
		XDestroyImage(img);
		throw t_error({ err, matchpath });
	}
	return { img,hdr.width,hdr.height,bytes };
}




/***
 The Run Length Encoding used is as follows:
 - For non repeating pixels the bytes are simply written out as 3 individual
   bytes in MSB order.
 - For repeating pixels the bytes are repeated followed by the number of 
   repeated bytes *following* the double byte. eg:

    With a 1 byte run length 10 ones would be encoded as 2 three byte pixel
    values followed by a single byte run length of value 8 (10 - 2):
    -------------- -------------- ------
   | 01 | 00 | 00 | 01 | 00 | 00 | 0x08 |
    -------------- -------------- ------

    2 byte run length:
    -------------- -------------- --------
   | 01 | 00 | 00 | 01 | 00 | 00 | 0x0008 |
    -------------- -------------- --------

    4 byte run length:
    -------------- -------------- ------------
   | 01 | 00 | 00 | 01 | 00 | 00 | 0x00000008 |
    -------------- -------------- ------------
***/
uint32_t loadImage(XImage *img)
{
	uint32_t pixel = 0;
	uint32_t prev_pixel = 0;
	uint32_t bytes = 0;
	uint16_t len2 = 0;
	uint8_t len1 = 0;
	int len = 0;
	int do_check = 0;
	int x;
	int y;

	for(x=0;x < hdr.width;++x)
	{
		for(y=0;y < hdr.height;++y)
		{
			if (len)
			{
				XPutPixel(img,x,y,pixel);
				--len;	
				continue;
			}
			pixel = readPixel();
			bytes += 3;
			if (do_check)
			{
				if (pixel == prev_pixel)
				{
					// Get the RLE type and length
					switch(hdr.rle_type)
					{
					case TYPE_RLE1:
						readBytes(&len1,1);
						++bytes;
						len = (uint8_t)len1;
						break;
					case TYPE_RLE2:
						readBytes(&len2,2);
						bytes += 2;
						len = (int)ntohs(len2);
						break;
					default:
						throw ERR_INVALID_RLE;
					}
					XPutPixel(img,x,y,pixel);
					do_check = 0;
					continue;
				}
			}
			XPutPixel(img,x,y,pixel);
			prev_pixel = pixel;
			do_check = (hdr.rle_type != TYPE_RAW);
		}
	}
	return bytes;
}




/*** Only 3 bytes of the pixel are used: 00 FF FF FF ***/
uint32_t readPixel()
{
	uint32_t pixel = 0;
	u_char p[3];

	readBytes(p,3);

	/* Stored as LSB */
	pixel = p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
	return pixel;
}




void readBytes(void *ptr, int size)
{
	int len;
	if ((len = read(fd,ptr,size)) == -1 || len != size) throw ERR_READ_FAIL;
}


//////////////////////////////////// SAVE ////////////////////////////////////

tuple<XImage *,int,int,uint32_t> savePicFile(string filepath)
{
	string matchpath;
	en_error err;
	uint32_t min_bytes;
	uint32_t bytes;
	uint8_t min_rle_type = 0;
	uint8_t type;

	XImage *img = XGetImage(
		display,win,
		0,0,win_width,win_height,AllPlanes,XYPixmap);

	// Just create image and leave it at that
	if (filepath == "") return { img, win_width, win_height, 0 };

	err = matchSavePath(filepath,matchpath,LOGO_PIC_FILE_EXT);
	if (err != OK) throw t_error({ err, filepath });
	
	check_mode = 1;
	min_bytes = 0;

	// Last 2 iterations are for the loop logic 
	for(type=0;type <= NUM_RLE_TYPES+1;++type)
	{
		if (check_mode)
		{
			if (type == NUM_RLE_TYPES)
			{
				check_mode = 0;
				continue;
			}
			hdr.rle_type = type;
		}
		else saveHeader(matchpath,min_rle_type);

		bytes = sizeof(hdr) + saveImage(img);
		if (!type || bytes < min_bytes)
		{
			min_bytes = bytes;
			min_rle_type = hdr.rle_type;
		}
		if (!check_mode) break;
	}
	return { img, win_width, win_height, bytes };
}




void saveHeader(string &filepath, uint8_t rle_type)
{
	if ((fd = open(filepath.c_str(),O_CREAT | O_TRUNC | O_WRONLY,0666)) == -1)
		throw t_error({ ERR_OPEN_FAIL, filepath });

	hdr.magic = htons((uint16_t)MAGIC);
	hdr.width = htons((uint16_t)win_width);
	hdr.height = htons((uint16_t)win_height);
	hdr.rle_type = rle_type;
	writeBytes(&hdr,sizeof(hdr));
}



/*** See loadImage() for RLE info ***/
uint32_t saveImage(XImage *img)
{
	uint32_t pixel = 0;
	uint32_t prev_pixel = 0;
	uint32_t max_len = 0;
	uint32_t len = 0;
	uint32_t bytes = 0;
	int do_match = 0;
	int get_run_len = 0;
	int x;
	int y;

	switch(hdr.rle_type)
	{
	case TYPE_RAW:
		break;
	case TYPE_RLE1:
		max_len = 0xFF;
		break;
	case TYPE_RLE2:
		max_len = 0xFFFF;
		break;
	default:
		assert(0);
	}
	for(x=0;x < win_width;++x)
	{
		for(y=0;y < win_height;++y)
		{
			pixel = (uint32_t)XGetPixel(img,x,y);
			if (get_run_len)
			{
				if (pixel == prev_pixel && len < max_len)
				{
					++len;
					continue;
				}
				bytes += writeRunLength(len);
				get_run_len = 0;
				do_match = 0;
			}
			writePixel(pixel);
			bytes += 3;

			if (do_match && pixel == prev_pixel)
			{
				len = 0;
				get_run_len = 1;
				continue;
			}

			prev_pixel = pixel;
			do_match = (hdr.rle_type != TYPE_RAW);
		}
	}
	if (get_run_len) bytes += writeRunLength(len);
	return bytes;
}



uint32_t writeRunLength(uint32_t len)
{
	uint8_t len1;
	uint16_t len2;

	switch(hdr.rle_type)
	{
	case TYPE_RLE1:
		len1 = (uint8_t)len;
		writeBytes(&len1,1);
		return 1;
	case TYPE_RLE2:
		len2 = htons((uint16_t)len);
		writeBytes(&len2,2);
		return 2;
	default:
		assert(0);
	}
	return 0;
}




void writePixel(uint32_t pixel)
{
	u_char p[3];
	p[0] = pixel & 0xFF;
	p[1] = (pixel >> 8) & 0xFF;
	p[2] = (pixel >> 16) & 0xFF;
	writeBytes(p,3);
}




void writeBytes(void *ptr, int size)
{
	if (check_mode) return;

	int len = write(fd,ptr,size);
	if (len != size) throw ERR_WRITE_FAIL;
}


//////////////////////////////////// MISC ////////////////////////////////////

st_picture *createPicture(XImage *img, int width, int height)
{
	char id[30];

	snprintf(id,sizeof(id),"pic_%d",img_counter);
	st_picture *pic = new st_picture(id,img,width,height);
	pictures[id] = shared_ptr<st_picture>(pic);
	++img_counter;
	return pic;
}




void setPicture(st_picture *pic)
{
	winpic = pictures[pic->id];
	setGlobalVarValue("$winpic",pic->id);
	turtle->drawAll();
}

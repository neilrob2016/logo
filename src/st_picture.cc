#include "globals.h"

st_picture::st_picture(char *_id, XImage *_img, int _width, int _height)
{
	id = _id;
	img = _img; 
	width = _width; 
	height = _height;
}




st_picture::~st_picture()
{
	if (img) XDestroyImage(img);
}



void st_picture::draw()
{
	if (img)
	{
		XPutImage(
			display,win,gc[COL_WHITE],
			img,0,0,0,0,width,height);
        	XFlush(display);
	}
}

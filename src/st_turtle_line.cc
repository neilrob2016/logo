#include "globals.h"

st_turtle_line::st_turtle_line(
	int col, int wd, int st, double xf, double yf, double xt, double yt):
	colour(col), width(wd), style(st)
{
	from.x = (int)round(xf);
	from.y = (int)round(yf);
	to.x = (int)round(xt);
	to.y = (int)round(yt);
}




void st_turtle_line::draw()
{
	xSetLineWidth(colour,width);
	xSetLineStyle(colour,style);

	if (turtle->win_edge != WIN_WRAPPED)
	{
		xDrawLine(colour,from.x,from.y,to.x,to.y);
		return;
	}

	/* If turtle drawing is wrapped then draw the seperate wrapped lines.
	   Easier to do it iteratively than to try and work them all out
	   beforehand. */
	double x = from.x;
	double y = from.y;
	double start_x = x;
	double start_y = y;
	double xd = to.x - x;
	double yd = to.y - y;
	double axd = abs(xd);
	double ayd = abs(yd);
	double xa;
	double ya;
	int cnt;

	if (axd > ayd)
	{
		xa = SGN(xd);
		ya = yd / axd;
		cnt = axd;
	}
	else
	{
		ya = SGN(yd);
		xa = xd / ayd;
		cnt = ayd;
	}
	for(int i=0;i < cnt;++i)
	{
		// Wrap
		if (x < 0)
		{
			xDrawLine(colour,start_x,start_y,x,y);
			x += win_width;
			start_x = x;
			start_y = y;
		}
		else if (x >= win_width)
		{
			xDrawLine(colour,start_x,start_y,x,y);
			x -= win_width;
			start_x = x;
			start_y = y;
		}

		if (y < 0)
		{
			xDrawLine(colour,start_x,start_y,x,y);
			y += win_height;
			start_x = x;
			start_y = y;
		}
		else if (y >= win_height)
		{
			xDrawLine(colour,start_x,start_y,x,y);
			y -= win_height;
			start_x = x;
			start_y = y;
		}
		x += xa;
		y += ya;
	}
	xDrawLine(colour,start_x,start_y,x,y);
}

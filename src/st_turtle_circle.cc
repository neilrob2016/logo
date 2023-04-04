#include "globals.h"

st_turtle_circle::st_turtle_circle(
	int col,
	int sty, int lw, double xdm, double ydm, double _x, double _y, bool fl):
	colour(col),
	style(sty),
	line_width(lw),
	fill(fl)
{
	x = (int)round(_x - xdm / 2);
	y = (int)round(_y - ydm / 2);
	x_diam = (int)round(xdm);
	y_diam = (int)round(ydm);
}




void st_turtle_circle::draw()
{
	xSetLineWidth(colour,line_width);
	xSetLineStyle(colour,style);
	xDrawCircle(colour,x_diam,y_diam,x,y,fill);
}

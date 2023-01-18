#include "globals.h"

#define TURTLE_SIZE 20
#define SIN(A)      sin((double)(A) / DEGS_PER_RADIAN)
#define COS(A)      cos((double)(A) / DEGS_PER_RADIAN)
#define TAN(A)      tan((double)(A) / DEGS_PER_RADIAN)


st_turtle::st_turtle()
{
	reset();
}



///////////////////////////////// BASIC OPS ///////////////////////////////////

/*** Reset everything ***/
void st_turtle::reset()
{
	visible = true;
	pen_down = true;
	pen_colour = COL_WHITE;
	store_fill = false;
	size = TURTLE_SIZE;
	win_edge = WIN_UNBOUNDED;
	setBackground(WIN_DEFAULT_COL);
	setLineWidth(1);
	setLineStyle("SOLID");
	clear();
	home();
}




/*** Clear the window and everything drawn but don't reset the turtle ***/
void st_turtle::clear()
{
	draw_lines.clear();
	fill_polys.clear();
	dots.clear();
	xWindowClear();
	draw();
}




void st_turtle::home()
{
	x = (double)win_width / 2;
	y = (double)win_height / 2;
	angle = 0;
	prev_x = x;
	prev_y = y;
	if (visible) drawAll();
}




void st_turtle::move(int dist)
{
	setXY(x + SIN(angle) * dist,y - COS(angle) * dist);
}




void st_turtle::rotate(double ang)
{
	setAngle(angle + ang);
}


////////////////////////////////// SETTERS ///////////////////////////////////

void st_turtle::setXY(double _x, double _y)
{
	if (_x == x && _y == y) return;

	if (win_edge == WIN_FENCED &&
	    (_x < 0 || _y < 0 || _x >= win_width || _y >= win_height))
	{
		throw t_error({ ERR_TURTLE_OUT_OF_BOUNDS, "" });
	}
	prev_x = x;
	prev_y = y;
	x = _x;
	y = _y;

	if (pen_down)
	{
		// Draw line from old position to new
		st_turtle_line tline(
			pen_colour,line_width,line_style,prev_x,prev_y,x,y);
		tline.draw();
		draw_lines.push_back(tline);
		if (store_fill) get<2>(fill_polys.back()).push_back(tline);

		XFlush(display);
	}

	// Do this after lines have been drawn or draw routine in st_turtle_line
	// will go wrong
	if (win_edge == WIN_WRAPPED)
	{
		if (x < 0) x += win_width;
		else if (x >= win_width) x -= win_width;

		if (y < 0) y += win_height;
		else if (y >= win_height) y -= win_height;
	}

	// Have to clear window and redraw everything because its the only way
	// to undraw the turtle at its current position 
	if (visible) drawAll();
}




void st_turtle::setX(double _x)
{
	setXY(_x,y);
}




void st_turtle::setY(double _y)
{
	setXY(x,_y);
}




void st_turtle::setAngle(double ang)
{
	if (ang != angle)
	{
		angle = ang;
		while(angle >= 360) angle -= 360;
		while(angle < 0) angle += 360;
		if (visible) drawAll();
	}
}




void st_turtle::setVisible(bool vis)
{
	visible = vis;
	drawAll();
}




void st_turtle::setColour(int col)
{
	if (col < 0 || col >= NUM_COLOURS)
		throw t_error({ ERR_INVALID_COLOUR, "" });
	pen_colour = col;
	draw();
	XFlush(display);
}




void st_turtle::setPenDown(bool down)
{
	pen_down = down;
	if (down) 
		draw();
	else if (visible)
		drawAll(); // Turtle is an outline so erase and redraw
}




void st_turtle::setUnbounded()
{
	puts("Turtle not fenced.");
	win_edge = WIN_UNBOUNDED;
}




void st_turtle::setFence()
{
	puts("Turtle fenced.");
	win_edge = WIN_FENCED;
}




void st_turtle::setWrap()
{
	puts("Turtle wrapped.");
	win_edge = WIN_WRAPPED;
}




void st_turtle::setBackground(int col)
{
	if (col < 0 || col >= NUM_COLOURS)
		throw t_error({ ERR_INVALID_COLOUR, "" });

	xSetWindowBackground(col);

	win_colour = col;
	setGlobalVarValue("$win_colour",win_colour);

	// Have to clear the window before the background colour is updated
	drawAll();
}




void st_turtle::setLineWidth(int width)
{
	if (width < 1) throw t_error({ ERR_INVALID_ARG, "" });
	line_width = width;
}




void st_turtle::setLineStyle(string style)
{
	for(char &c: style) c = toupper(c);

	if (style == "SOLID") line_style = LineSolid;
	else if (style == "DASH") line_style = LineOnOffDash;
	else if (style == "DOUBLE_DASH") line_style = LineDoubleDash;
	else throw t_error({ ERR_INVALID_ARG, style });
	line_style_str = style;
}




void st_turtle::setFill()
{
	// X windows fill algo won't work if polygon is in seperate parts
	if (win_edge == WIN_WRAPPED) throw t_error({ ERR_CANT_FILL, "" });

	store_fill = true;

	// Get rid of unfillable polygon - need at least a triangle
	if (fill_polys.size() && get<2>(fill_polys.back()).size() < 3)
		fill_polys.pop_back();

	// Add empty vector if not there or have a used one
	if (!fill_polys.size() || get<2>(fill_polys.back()).size())
		fill_polys.push_back(make_tuple(false,pen_colour,t_shape()));
}




void st_turtle::setSize(int _size)
{
	if (_size < 0) throw t_error({ ERR_INVALID_ARG, "" });
	size = _size;
	if (visible) drawAll();
}




/*** Set the angle of the turtle to point in the direction of the co-ords.
     0,0 is top left in the window ***/
void st_turtle::setTowards(double _x, double _y)
{
	double xd = _x - x;
	double yd = y - _y;

	if (!xd) angle = (yd > 0 ? 0 : 180);
	else
	if (!yd) angle = (xd > 0 ? 90 : 270);
	else
	{
		angle = atan(xd / yd) * DEGS_PER_RADIAN;
		if (yd < 0) angle += 180;
		else 
		if (xd < 0) angle += 360;
	}
	if (visible) drawAll();
}



////////////////////////////////// GRAPHICS ///////////////////////////////////

/*** Technically this is not a turtle operation since the turtle doesn't
     move but simpler to put it here for redraw purposes ***/
void st_turtle::drawDot(short dx, short dy)
{
	xDrawPoint(pen_colour,dx,dy);
	dots.push_back(make_pair(pen_colour,XPoint{dx,dy}));
	XFlush(display);
}




/*** Draw the turtle itself which is an arrow shape ***/
void st_turtle::draw()
{
	if (!visible) return;
	XPoint pnt[5];

	pnt[0].x = (int)roundf(x + SIN(angle) * size);
	pnt[0].y = (int)roundf(y - COS(angle) * size);
	pnt[1].x = (int)roundf(x + SIN(angle+225) * size);
	pnt[1].y = (int)roundf(y - COS(angle+225) * size);
	pnt[2].x = (int)roundf(x);
	pnt[2].y = (int)roundf(y);
	pnt[3].x = (int)roundf(x + SIN(angle+135) * size);
	pnt[3].y = (int)roundf(y - COS(angle+135) * size);
	pnt[4].x = pnt[0].x;
	pnt[4].y = pnt[0].y;

	if (line_width != 1) xSetLineWidth(pen_colour,1);
	xDrawPolygon(pen_colour,pnt,5,pen_down);
	if (line_width != 1) xSetLineWidth(pen_colour,line_width);

	XFlush(display);
}




/*** Draw all lines plus the turtle. Have to do this simply to properly erase
     the turtle as GXor in a GC doesn't bloody work ***/
void st_turtle::drawAll()
{
	xWindowClear();

	// Draw all the lines
	for(auto &tline: draw_lines) tline.draw();

	// Draw any filled polygons
	for(auto &tup: fill_polys)
		if (get<0>(tup)) fillPolygon(get<1>(tup),get<2>(tup));

	// Draw dots
	for(auto &pr: dots) xDrawPoint(pr.first,pr.second.x,pr.second.y);

	// Draw the turtle
	draw();

	XFlush(display);
}




/*** Fill in the area bounded by the latest stored lines in the current pen 
     colour ***/
void st_turtle::fill()
{
	// X windows fill algo won't work if polygon is in seperate parts
	if (win_edge == WIN_WRAPPED) throw t_error({ ERR_CANT_FILL, "" });
	if (!fill_polys.size()) return;

	auto &tup = fill_polys.back();
	auto &polygon = get<2>(tup);

	// Set this particular polygon to be filled
	get<0>(tup) = true;

	// Don't store any more polygons until SETFILL called again
	store_fill = false;

	// Need at least a triangle
	if (polygon.size() < 3)
	{
		fill_polys.pop_back();
		return;
	}
	fillPolygon(pen_colour,polygon);

	XFlush(display);
}




void st_turtle::fillPolygon(int col, t_shape &polygon)
{
	XPoint *pnts = new XPoint[polygon.size() + 1];
	int i = 0;

	for(auto &tline: polygon)
	{
		pnts[i].x = tline.from.x;
		pnts[i].y = tline.from.y;
		++i;
		pnts[i].x = tline.to.x;
		pnts[i].y = tline.to.y;
	}
	xDrawPolygon(col,pnts,polygon.size()+1,true);

	delete[] pnts;
}

////////////////////////////////// MISC ///////////////////////////////////

/*** Output format:
     [<xpos> <ypos> <heading> <turtle size> <visible 1/0> <pen down 1/0> 
      <pen colour>] ***/
shared_ptr<st_line> st_turtle::facts()
{
	shared_ptr<st_line> line = make_shared<st_line>(true);
	line->tokens.push_back(st_token(x));
	line->tokens.push_back(st_token(y));
	line->tokens.push_back(st_token(angle));
	line->tokens.push_back(st_token(size));
	line->tokens.push_back(st_token(line_width));
	line->tokens.push_back(st_token(pen_colour));
	line->tokens.push_back(st_token(pen_down ? "DOWN" : "UP"));
	line->tokens.push_back(st_token(visible ? "VISIBLE" : "HIDDEN"));
	line->tokens.push_back(st_token(store_fill ? "FILL" : "NO_FILL"));

	string edge;
	switch(win_edge)
	{
	case WIN_UNBOUNDED:
		edge = "UNBOUNDED";
		break;
	case WIN_FENCED:
		edge = "FENCED";
		break;
	case WIN_WRAPPED:
		edge = "WRAPPED";
		break;
	default:
		assert(0);
	}
	line->tokens.push_back(st_token(edge));
	line->tokens.push_back(st_token(line_style_str));
	line->tokens.push_back(st_token(flags.window_mapped ? "WIN_VISIBLE" : "WIN_HIDDEN"));

	return line;
}

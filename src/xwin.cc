#include "globals.h"

void xWindowResized(XEvent &event);

/*** Set up X except for turtle GC which is done in st_turtle::st_turtle() ***/
bool xConnect()
{
	Atom delete_notify;
	XGCValues gcvals;
	XColor colour;
	XColor unused;
	Colormap cmap;
	int screen;
	int white;
	int black;
	int stage;
	int col;
	u_char r,g,b;
	char colstr[5];

	printf("Connecting to X server \"%s\"...\n",XDisplayName(xdisp));
	if (!(display = XOpenDisplay(xdisp)))
	{
		puts("ERROR: Can't connect to X display.");
		return false;
	}

	screen = DefaultScreen(display);
	black = BlackPixel(display,screen);
	white = WhitePixel(display,screen);
	cmap = DefaultColormap(display,screen);
	win_colour = WIN_DEFAULT_COL;

	win = XCreateSimpleWindow(
		display,
		RootWindow(display,screen),
		0,0,win_width,win_height,0,white,black);

	XSetWindowBackground(display,win,black);
	xSetWindowTitle();

	r = 0;
	g = 0xF;
	b = 0;
	stage = 1;

	gcvals.foreground = white;
	gcvals.line_width = 1;

	for(col=0;col < NUM_COLOURS;++col)
	{
		sprintf(colstr,"#%01X%01X%01X",r,g,b);

		if (!XAllocNamedColor(display,cmap,colstr,&colour,&unused))
		{
			printf("WARNING: Can't allocate colour %s\n",colstr);
			gcvals.foreground = white;
		}
		else gcvals.foreground = colour.pixel;

		gcvals.line_width = 1;
		gc[col] = XCreateGC(display,win,GCForeground | GCLineWidth,&gcvals);

		switch(stage)
		{
		case 1:
			// Green to turquoise 
			if (++b == 0xF) stage = 2;
			break;

		case 2:
			// Turquoise to blue 
			if (!--g) stage = 3;
			break;

		case 3:
			// Blue to mauve 
			if (++r == 0xF) stage = 4;
			break;

		case 4:
			// Mauve to red
			if (!--b) stage = 5;
			break;

		case 5:
			// Red to yellow 
			if (++g == 0xF) stage = 6;
			break;

		case 6:
			// Yellow to black 
			if (!--r) stage = 7;
			--g;
			break;
		case 7:
			// Black to white
			++r;
			++g;
			++b;
		}
	}

	x_sock = ConnectionNumber(display);

	XSelectInput(display,win,ExposureMask | StructureNotifyMask);

	// Don't kill process when window closed, send ClientMessage instead
	delete_notify = XInternAtom(display,"WM_DELETE_WINDOW",True);
	XSetWMProtocols(display,win,&delete_notify,1);

	if (flags.map_window)
	{
		XMapWindow(display,win);
		flags.window_mapped = true;
	}

	// Create the turtle
	turtle = new st_turtle;
	puts("Graphics enabled.");

	return true;
}




void xDisconnect()
{
	if (flags.graphics_enabled)
	{
		delete turtle;
		turtle = NULL;

		assert(display);
		XCloseDisplay(display);

		puts("Graphics disabled.");
	}
}




void xParseEvent()
{
	XEvent event;
	XEvent conf_event;
	bool redraw = false;
	bool configure = false;

	while(XPending(display))
	{
		XNextEvent(display,&event);

		switch(event.type)
		{
		case Expose:
		case MapNotify:
			redraw = true;
			break;
		case ConfigureNotify:
			conf_event = event;
			configure = true;
			break;
		case ClientMessage:
			// Window closed, exit gracefully
			cout << "\n*** Turtle window closed ***\n";
			goodbye();
			break;
		}
	}
	if (redraw) turtle->drawAll();
	if (configure) xWindowResized(conf_event);
}




void xWindowResized(XEvent &event)
{
	win_width = event.xconfigure.width;
	win_height = event.xconfigure.height;
	xSetWindowTitle();
	setWindowSystemVars();
}




void xSetWindowTitle()
{
	XTextProperty title;
	char title_str[100];
	char *tmp = title_str;
	
	sprintf(title_str,"%s (%dx%d)",LOGO_INTERPRETER,win_width,win_height);
	XStringListToTextProperty(&tmp,1,&title);
	XSetWMProperties(display,win,&title,NULL,NULL,0,NULL,NULL,NULL);
}




void xSetWindowBackground(int col)
{
        XGCValues gcvals;
        XGetGCValues(display,gc[col],GCForeground,&gcvals);
        XSetWindowBackground(display,win,gcvals.foreground);
}




void xSetLineWidth(int col, int width)
{
	XGCValues gcvals;
	gcvals.line_width = width;
	XChangeGC(display,gc[col],GCLineWidth,&gcvals);
}




void xSetLineStyle(int col, int style)
{
	XGCValues gcvals;
	gcvals.line_style = style;
	XChangeGC(display,gc[col],GCLineStyle,&gcvals);
}




void xWindowClear()
{
	XClearWindow(display,win);
	XFlush(display);
}




void xWindowMap()
{
	if (flags.graphics_enabled)
	{
		XMapWindow(display,win);
		XFlush(display);
		flags.window_mapped = true;
	}
	else throw t_error({ ERR_NO_GRAPHICS, "" });
}




void xWindowUnmap()
{
	if (flags.graphics_enabled)
	{
		XUnmapWindow(display,win);
		XFlush(display);
		flags.window_mapped = false;
	}
	else throw t_error({ ERR_NO_GRAPHICS, "" });
}




/*** This will cause X to fire off a ConfigureNotify which we'll get and
     xWindowResized() will get called so no need to set anything here ***/
void xResizeWindow(int width, int height)
{
	XResizeWindow(display,win,width,height);
	XFlush(display);
}




void xDrawPoint(int col, int x, int y)
{
	XDrawPoint(display,win,gc[col],x,y);
}




void xDrawLine(int col, int xf, int yf, int xt, int yt)
{
	XDrawLine(display,win,gc[col],xf,yf,xt,yt);
}




void xDrawLine(int col, double xf, double yf, double xt, double yt)
{
	XDrawLine(
		display,win,gc[col],
		(int)round(xf),
		(int)round(yf),
		(int)round(xt),
		(int)round(yt));
}




void xDrawPolygon(int col, XPoint *pnts, int cnt, bool fill)
{
	if (fill)
		XFillPolygon(display,win,gc[col],pnts,cnt,Nonconvex,CoordModeOrigin);
	else
		XDrawLines(display,win,gc[col],pnts,cnt,CoordModeOrigin);
}




void xDrawCircle(int col, int x_diam, int y_diam, int x, int y, bool fill)
{
	static const int circle = 23040;
	if (fill)
		XFillArc(display,win,gc[col],x,y,x_diam,y_diam,0,circle);
	else
		XDrawArc(display,win,gc[col],x,y,x_diam,y_diam,0,circle);
}

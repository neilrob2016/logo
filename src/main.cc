/*****************************************************************************
 NRJ-LOGO

 A LOGO interpreter with Turtle graphics mainly written to keep my C++ skills
 up to date but also in the hope it will help Natasha learn programming.

 Copyright (C) Neil Robertson 2020
 *****************************************************************************/

#define MAINFILE
#include "globals.h"
#include "build_date.h"

static string loadfile;
static string runtext;

static void parseCmdLine(int argc, char **argv);
static void version(bool short_ver);
static void mainloop();
static void init(bool startup);
static void sigHandler(int sig);


int main(int argc, char **argv)
{
	parseCmdLine(argc,argv);
	version(true);
	io.kbRawMode();
	mainloop();
	return 0;
}




void parseCmdLine(int argc, char **argv)
{
	bzero(&flags,sizeof(flags));
	flags.do_graphics = true;
	flags.map_window = true;

	win_width = WIN_WIDTH;
	win_height = WIN_HEIGHT;
	disp = NULL;
	max_history_lines = MAX_HISTORY_LINES;

	for(int i=1;i < argc;++i)
	{
		string opt = argv[i];
		if (opt[0] != '-') goto USAGE;

		if (opt == "-ver")
		{
			version(false);
			exit(0);
		}
		if (opt == "-con")
		{
			flags.do_graphics = false;
			continue;
		}
		if (opt.length() != 2) goto USAGE;

		switch(opt[1])
		{
		case 'i':
			flags.indent_label_blocks = true;
			continue;
		case 'u':
			flags.map_window = false;
			continue;
		}
		if (++i == argc) goto USAGE;

		switch(opt[1])
		{
		case 'd':
			disp = argv[i];
			break;
		case 'w':
			if ((win_width = atoi(argv[i])) < 1) goto USAGE;
			break;
		case 'h':
			if ((win_height = atoi(argv[i])) < 1) goto USAGE;
			break;
		case 'c':
			if ((max_history_lines = atoi(argv[i])) < 1) goto USAGE;
			break;
		case 'l':
			loadfile = argv[i];
			break;
		case 'r':
			runtext = argv[i];
			break;
		default:
			goto USAGE;
		}
	}
	return;

	USAGE:
	printf("Usage: %s\n"
	       "       -d <X display>  : Default = NULL\n"
	       "       -w <win width>  : Default = %d\n"
	       "       -h <win height> : Default = %d\n"
	       "       -l <filename>   : Procedure file to load at startup.\n"
	       "       -r <text>       : Code or procedure to run immediately.\n"
	       "       -c <lines>      : Max number of console history lines. Default = %d\n"
	       "       -i              : Indent procedure listings between LABEL and GO.\n"
	       "       -u              : Start with the graphics window hidden (unmapped).\n"
	       "       -con            : Console only, no turtle graphics.\n"
	       "       -ver            : Display version then exit.\n"
	       "Note: All parameters are optional.\n",
		argv[0],WIN_WIDTH,WIN_HEIGHT,MAX_HISTORY_LINES);
	exit(1);
}




void version(bool short_ver)
{
	if (short_ver)
	{
		printf("\n%s %s (%s), pid: %d\n%s\n\n",
			LOGO_INTERPRETER,
			LOGO_VERSION,BUILD_DATE,getpid(),LOGO_COPYRIGHT);
	}
	else
	{
		printf("\n%s, %s\n\n",LOGO_INTERPRETER,LOGO_COPYRIGHT);
		printf("Version   : %s\n",LOGO_VERSION);
		printf("Build date: %s\n\n",BUILD_DATE);
	}
}




void mainloop()
{
	bool startup = true;
	t_error err;
	fd_set mask;
	int rr;

	RESTART:
	init(startup);
	startup = false;
	ready();
	prompt();

	while(1)
	{
		FD_ZERO(&mask);
		FD_SET(STDIN,&mask);
		if (flags.do_graphics) FD_SET(x_sock,&mask);

		switch(select(FD_SETSIZE,&mask,0,0,0))
		{
		case -1:
			if (errno == EINTR) continue;
			perror("ERROR: select()");
			doExit(errno);
		case 0:
			// Timeout - should never happen
			assert(0);
		}
		if (flags.do_graphics && FD_ISSET(x_sock,&mask)) xParseEvent();

		if (FD_ISSET(STDIN,&mask) && 
		    io.readInput(STDIN,true,false,true,rr))
		{
			if (rr == -1) exit(1);

			loadproc = "";
			try
			{
				io.parseInput();
			}
			catch(t_interrupt &inter)
			{
				if (inter.first == INT_RESTART)
				{
					printStopMesg("RESTART");
					goto RESTART;
				}
			}
		}
	}
}




/*** This can be called after startup if RESTART command called ***/
void init(bool startup)
{
	flags.angle_in_degs = true;
	tracing_mode = TRACING_OFF;
	logo_state = STATE_CMD;
	stop_proc = NULL;
	curr_proc_inst = NULL;
	nest_depth = 0;
	srandom(time(0));
	
	if (startup)
	{
		setSystemVars();
		if (flags.do_graphics)
		{
			xInit();
			turtle = new st_turtle;
		}
		else turtle = NULL;

		signal(SIGINT,sigHandler);
	}
	else
	{
		clearGlobalVariables();
		user_procs.clear();
		watch_vars.clear();
		io.reset();
		if (turtle) turtle->reset();
	}

	if (loadfile != "")
	{
		try
		{
			loadProcFile(loadfile,"");
		}
		catch(t_error &err)
		{
			if (err.second == "") err.second = loadfile;
			printRunError(err.first,err.second.c_str());
		}
		catch(t_interrupt &inter)
		{
			// If we restarted we'd just get into an infinite loop
			assert(inter.first == INT_RESTART);
			printRunError(ERR_CANT_RESTART,"RESTART");
		}
	}
	if (runtext != "")
	{
		cout << "Running code...\n";
		flags.suppress_prompt = true; 
		io.execText(runtext);
		flags.suppress_prompt = false;
	}
}




void sigHandler(int sig)
{
	if (flags.executing) flags.do_break = true;
	else
	{
		if (logo_state == STATE_DEF_PROC)
		{
			printf("*** BREAK in procedure \"%s\" definition ***\n",
				def_proc->name.c_str());
			def_proc.reset();
			logo_state = STATE_CMD;
		}
		else cout << "*** BREAK ***\n";

		// Clear anything in keyboard buffer
		io.reset();
		prompt();
	}
}

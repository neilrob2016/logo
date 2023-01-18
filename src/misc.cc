#include "globals.h"


void printRunError(int errnum, const char *tokstr)
{
	if (stop_proc)
	{
		printf("ERROR %d: %s at \"%s\" in procedure \"%s\" on line %d",
			errnum,
			error_str[errnum],
			tokstr,stop_proc->name.c_str(),stop_proc->exec_linenum);
		stop_proc = NULL;
	}
	else
	{
		printf("ERROR %d: %s at \"%s\"",
			errnum,error_str[errnum],tokstr);
	}

	// Errors that also have an OS error 
	switch(errnum)
	{
	case ERR_OPEN_FAIL:
	case ERR_READ_FAIL:
	case ERR_WRITE_FAIL:
	case ERR_STAT_FAIL:
	case ERR_CD_FAIL:
		printf(": %s\n",strerror(errno));
		break;
	default:
		putchar('\n');
	}
}




void printStopMesg(const char *stopword)
{
	if (stop_proc)
	{
		printf("*** %s in procedure \"%s\" on line %d ***\n",
			stopword,
			stop_proc->name.c_str(),
			stop_proc->exec_linenum);
		stop_proc = NULL;
	}
	else printf("*** %s ***\n",stopword);
}




void printWatch(char type, string &name, st_value &val)
{
	cout << "{" << type << "," << name << "=" << val.dump(true) << "}\n";
}




void ready()
{
	puts("READY");
}




void prompt()
{
	if (flags.suppress_prompt) return;

	switch(logo_state)
	{
	case STATE_CMD:
		cout << "? " << flush;
		break;
	case STATE_DEF_PROC:
		cout << "> " << flush;
		break;
	default:
		assert(0);
	}
}




void goodbye()
{
	cout << "*** GOODBYE ***\n";
	doExit(0);
}




void doExit(int code)
{
	io.kbSaneMode();
	exit(code);
}

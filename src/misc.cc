#include "globals.h"


void loadProgram(string filename)
{
	int fd;

	do
	{
		if ((fd = open(filename.c_str(),O_RDONLY)) == -1)
		{
			// If we don't have .lg on end append then try again
			if (filename.size() < 3 ||
			    filename.substr(filename.size()-3,3) != LOGO_FILE_EXT)
				filename += LOGO_FILE_EXT;
			else
				throw t_error({ ERR_OPEN_FAIL, "" });
		}
	} while(fd == -1);

	size_t proc_cnt = user_procs.size();
	st_io file;
	bool ret;
	int rr;

	try
	{
		suppress_prompt = true;
		do
		{
			if (file.readInput(fd,false,false,false,rr))
				ret = file.parseInput();
		} while(rr > 0 && ret);
	}
	catch(...)
	{
		close(fd);
		suppress_prompt = false;
		throw;
	}
	close(fd);
	suppress_prompt = false;
	if (ret)
		printf("Loaded %ld procedure(s).\n",user_procs.size() - proc_cnt);
}




bool isNumber(string str)
{
	bool point = false;
	for(char c: str)
	{
		if (c == '.')
		{
			if (point) return false;
			point = true;
		}
		else if (!isdigit(c)) return false;
	}
	return true;
}




/*** Yes I could use stringstream, but I want easier control of formatting ***/
string numToString(double num)
{
	char s[50];
	if (num == (long)num)
		snprintf(s,sizeof(s),"%ld",(long)num);
	else
		snprintf(s,sizeof(s),"%f",num);
	return s;
}




/*** Returns true if the string matches the pattern, else false. Supports 
     wildcard patterns containing '*' and '?' ***/
bool wildMatch(const char *str, const char *pat, bool case_sensitive)
{
	char *s;
	char *s2;
	char *p;

	for(s=(char *)str,p=(char *)pat;*s && *p;++s,++p)
	{
		switch(*p)
		{
		case '?':
			continue;

		case '*':
			if (!*(p+1)) return true;

			for(s2=s;*s2;++s2)
			{
				if (wildMatch(s2,p+1,case_sensitive))
					return true;
			}
			return false;
		}
		if (case_sensitive)
		{
			if (*s != *p) return false;
		}
		else if (toupper(*s) != toupper(*p)) return false;
	}

	// Could have '*' leftover in the pattern which can match nothing.
	// eg: "abc*" should match "abc" and "*" should match ""
	if (!*s)
	{
		// Could have multiple *'s on the end which should all match ""
		for(;*p && *p == '*';++p);
		if (!*p) return true;
	}
	return false;
}




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
	if (suppress_prompt) return;

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

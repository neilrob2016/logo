#include "globals.h"


void loadProcFile(string filepath, string procname)
{
	string matchpath;
        en_error err;
	int fd;

	loadproc = procname;
	err = matchLoadPath(filepath,matchpath,LOGO_PROC_FILE_EXT);
	if (err != OK) throw t_error({ err, filepath });

	cout << "Loading file \"" << matchpath << "\"...\n";

	if ((fd = open(matchpath.c_str(),O_RDONLY)) == -1)
		throw t_error({ ERR_OPEN_FAIL, matchpath });

	size_t proc_cnt = user_procs.size();
	st_io file;
	bool ret;
	int rr;

	try
	{
		flags.suppress_prompt = true;
		do
		{
			if (file.readInput(fd,false,false,false,rr))
				ret = file.parseInput();
		} while(rr > 0 && ret);
	}
	catch(...)
	{
		close(fd);
		flags.suppress_prompt = false;
		loadproc = "";
		throw;
	}
	close(fd);
	flags.suppress_prompt = false;
	loadproc = "";
	if (logo_state == STATE_DEF_PROC) logo_state = STATE_CMD;

	if (ret)
		printf("Loaded %ld procedure(s).\n",user_procs.size() - proc_cnt);
}




void saveProcFile(string filepath, string &procname, bool psave)
{
	// If we have a path match it for wildcards
	FILE *fp;
	string matchpath;
	en_error err;
	int cnt;

	cnt = count(filepath.begin(),filepath.end(),'/');
	err = matchSavePath(filepath,matchpath,LOGO_PROC_FILE_EXT);
	if (err != OK) throw t_error({ err, filepath });
	
	cout << "Saving to file \"" << matchpath << "\"...\n";
	fp = fopen(matchpath.c_str(),"w");

	if (!fp) throw t_error({ ERR_OPEN_FAIL, "" });
	cnt = 0;

	try
	{
		for(auto &[name,proc]: user_procs)
		{
			if (procname == "" || procname == name)
			{
				proc->dump(fp,true,false);
				++cnt;
				if (psave) break;
			}
			fputc('\n',fp);
		}
	}
	catch(t_error &err)
	{
		fclose(fp);
		throw;
	}
	fclose(fp);
	printf("Saved %d procedures.\n",cnt);
}

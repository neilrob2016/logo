#include "globals.h"


void loadProcFile(string filepath, string procname)
{
	string matchpath;
        en_error err;
	char *str;
	int fd;

	loadproc = procname;

	// Expand the path
	for(int i=0;i < 2;++i)
	{
		// Can usually modify c_str() directly these days but best not
		// just to be safe.
		assert((str = strdup(filepath.c_str())));
        	err = matchPath(S_IFREG,str,matchpath);
		free(str);

		if (err != OK)
		{
			// If we don't have .lg on end append then try again
			if (filepath.size() < 3 ||
			    filepath.substr(filepath.size()-3,3) != LOGO_FILE_EXT)
			{
				filepath += LOGO_FILE_EXT;
			}
			else throw t_error({ err, filepath });
		}
		else break;
	}	
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

	if (ret)
		printf("Loaded %ld procedure(s).\n",user_procs.size() - proc_cnt);
}




void saveProcFile(string filepath, string &procname, bool psave)
{
	// If no .lg on end then add it
	if (filepath.size() < 3 ||
	    filepath.substr(filepath.size()-3,3) != LOGO_FILE_EXT)
	{
		filepath += LOGO_FILE_EXT;
	}

	// If we have a path match it for wildcards
	FILE *fp;
	string matchpath;
	en_error err;
	char *str;
	size_t slashpos;
	int cnt;

	cnt = count(filepath.begin(),filepath.end(),'/');

	// Expand directory name
	if ((filepath[0] == '/' && cnt > 1) ||
	    (filepath[0] != '/' && cnt))
	{
		// Get rid of section after final slash.
		// eg: /a/b/c/myfile -> /a/b/c
		slashpos = filepath.rfind('/');
		string dirname = filepath.substr(0,slashpos);

		// matchPoint() modifies str so can't pass c_str() direct
		assert((str = strdup(dirname.c_str())));
		err = matchPath(S_IFDIR,str,matchpath);
		free(str);
		if (err != OK) throw t_error({ err, "" });

		matchpath += "/";
		matchpath += filepath.substr(slashpos+1);
		filepath = matchpath;
	}

	// Expand filename if it has wildcards
	if (pathHasWildCards(filepath))
	{
		assert((str = strdup(filepath.c_str())));
		err = matchPath(S_IFREG,str,matchpath);
		free(str);
		if (err == OK) filepath = matchpath;

		// Don't want to create filenames with '*' or '?' in them even 
		// though fopen() is happy to do it. ~ is ok though.
		if (filepath.find_first_of("*?") != string::npos)
			throw t_error({ ERR_INVALID_PATH, "" });
	}

	cout << "Saving to file \"" << filepath << "\"...\n";
	fp = fopen(filepath.c_str(),"w");

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

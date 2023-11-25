#include "globals.h"


/*** Get the path of a given user ***/
static inline char *getUserDir(char *name, string &matchpath)
{
	struct passwd *pwd;
	char *ptr;

	if ((ptr = strchr(name,'/')))
		*ptr = 0;
	else
		ptr = name + strlen(name) - 1;
	if (!(pwd = getpwnam(name))) return NULL;
	matchpath = pwd->pw_dir;
	return ptr + 1;
}



/*** Attempt to match the file and if not found add the TLE and try again ***/
en_error matchLoadPath(string &filepath, string &matchpath, const char *tle)
{
	en_error err;
	char *str;

	matchpath = "";

	for(int i=0;i < 2;++i)
	{
		assert((str = strdup(filepath.c_str())));
		err = matchPath(S_IFREG,str,matchpath);
		free(str);
		if (err != OK)
		{
			if (filepath.size() < 3 ||
			    filepath.substr(filepath.size()-3,3) != tle)
			{
				filepath += tle;
			}
			else return err;
		}
		else break;
	}
	return OK;
}



/*** The difference between this and the load path is that the load path file
     at the end of the path must exist. That is not the case for saving a
     new file ***/
en_error matchSavePath(string &filepath, string &matchpath, const char *tle)
{
	en_error err;
	char *str;
	size_t slashpos;
	int cnt;

	matchpath = "";
	cnt = count(filepath.begin(),filepath.end(),'/');

	// Expand directory name
	if ((filepath[0] == '/' && cnt > 1) ||
	    (filepath[0] != '/' && cnt))
	{
		// Get rid of section after final slash.
		// eg: /a/b/c/myfile -> /a/b/c
		slashpos = filepath.rfind('/');
		string dirname = filepath.substr(0,slashpos);

		assert((str = strdup(dirname.c_str())));
		err = matchPath(S_IFDIR,str,matchpath);
		free(str);
		if (err != OK) return err;

		matchpath += "/";
		matchpath += filepath.substr(slashpos+1);
		filepath = matchpath;
	}
	else matchpath = filepath;

	bool add_tle = true;

	// Expand filename if it has wildcards
	if (pathHasWildCards(filepath))
	{
		assert((str = strdup(filepath.c_str())));
		err = matchPath(S_IFREG,str,matchpath);
		free(str);

		if (err != OK) return ERR_INVALID_PATH;

		// We've matched a pre-existing file so don't add TLE
		add_tle = false;
	}
	if (add_tle &&
	    (matchpath.size() < 3 || 
	     matchpath.substr(matchpath.size()-3,3) != tle))
	{
		matchpath += tle;
	}
	return OK;
}



/*** Goes through a particular directory and looks for an object type that
     matches type and pattern. If type is zero then match any type. Simpler
     to have "pat" as a char* than std::string which would require lots of
     substr()'ing ***/
en_error matchPath(int type, char *pat, string &matchpath, bool toplevel)
{
	if (matchpath.length() >= PATH_MAX) return ERR_PATH_TOO_LONG;

	string path;

	// Set up initial dir to read 
	if (toplevel)
	{
		matchpath = "";

		if (*pat == '/')
		{
			matchpath = "/";
			++pat;
		}
		else if (*pat == '~')
		{
			++pat;
			if (!*pat) matchpath = getenv("HOME");
			else if (*pat == '/')
			{
				++pat;
				matchpath = getenv("HOME");
			}
			else if (!(pat = getUserDir(pat,matchpath)))
				return ERR_INVALID_PATH;
			matchpath += "/";
		}
		else if (!strncmp(pat,"../",3))
		{
			matchpath = "../";
			pat += 3;
		}
		else
		{
			matchpath = "./";
			if (pat[0] == '.' && pat[1] == '/') pat += 2;
		}

		// If no pattern left then we've got path already 
		if (!*pat) return OK;
	}

	DIR *dir;

	if (!(dir = opendir(matchpath.c_str()))) return ERR_OPEN_FAIL;

	struct dirent *de;
	struct stat fs;
	en_error err;
	int stat_type;
	char *ptr;

	// Get section of pattern to match 
	if ((ptr = strchr(pat,'/'))) *ptr = 0;

	while((de = readdir(dir)))
	{
		// Only match on . and .. if specifically requested 
		if ((!strcmp(de->d_name,".") && strcmp(pat,".")) ||
		    (!strcmp(de->d_name,"..") && strcmp(pat,".."))) continue;

		// Match directory entry to pattern section 
		if (!wildMatch(de->d_name,pat,true)) continue;

		/* Name matches the pattern. Stat it and make sure its
		   the correct type */
		if (path.length() + matchpath.length() > PATH_MAX) continue;
		path = matchpath;

		if (path[path.length()-1] != '/') path += "/";
		path += de->d_name;
		if (stat(path.c_str(),&fs) == -1)
		{
			err = ERR_STAT_FAIL;
			goto DONE;
		}
		stat_type = fs.st_mode & S_IFMT;

		/* If more pattern then recurse. First check the matchpath
		   so far is a directory */
		if (ptr && *(ptr+1))
		{
			if (stat_type == S_IFDIR)
			{
				matchpath = path;
				if ((err = matchPath(type,ptr+1,matchpath,false)) != OK)
				{
					/* Get rid of the bit added by the 
					   recursive call and go around again */
					matchpath = matchpath.substr(0,matchpath.rfind("/"));
					continue;
				}

				// If OK or another error return 
				goto DONE;
			}
			continue;
		}

		/* At end of pattern. Check final file for type. Treat links
		   as files. */
		if (stat_type == type || 
		    (type == S_IFREG && stat_type == S_IFLNK))
		{
			matchpath = path;
			err = OK;
			goto DONE;
		}
	}
	err = ERR_INVALID_PATH;

	DONE:
	closedir(dir);
	if (ptr) *ptr = '/';
	return err;
}




bool pathHasWildCards(string &path)
{
	return path.find_first_of("*?~") != string::npos;
}

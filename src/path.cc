#include "globals.h"

// Prevents endless looping 
#define MAX_DEPTH 100

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




/*** Goes through a particular directory and looks for an object type that
     matches type and pattern. If type is zero then match any type. Simpler
     to have "pat" as a char* than std::string which would require lots of
     substr()'ing ***/
en_error matchPath(int type, char *pat, string &matchpath, bool toplevel)
{
	DIR *dir;
	struct dirent *de;
	struct stat fs;
	string path;
	en_error err;
	char *ptr;
	int stat_type;

	if (matchpath.length() >= PATH_MAX) return ERR_PATH_TOO_LONG;

	// Set up initial dir to read 
	if (toplevel)
	{
		matchpath = "";

		/* First check to see if there's any wildcards in the pattern.
		   If not then just return it as result */
		for(ptr=pat;
		    *ptr && *ptr != '*' && *ptr != '?' && *ptr != '~';
		    ++ptr);
		if (!*ptr)
		{
			matchpath = pat;
			return OK;
		}
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

	if (!(dir = opendir(matchpath.c_str()))) return ERR_OPEN_FAIL;

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
		if (!type || stat_type == type || 
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

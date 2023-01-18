#include "globals.h"

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

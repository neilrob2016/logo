#include "globals.h"


/*** Convert expressions in the list into values ***/
t_result procEval(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);

	if (result.first.type != TYPE_LIST)
		throw t_error({ ERR_CANT_EVAL, line->tokens[tokpos].toString() });

	return { st_value(result.first.listline->evalList()), result.second };
}




/*** Return the first element ***/
t_result procFirstLast(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;
	t_result result = line->evalExpression(++tokpos);
	st_value val;

	switch(result.first.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	case TYPE_STR:
		val.set(string(1,(sproc == SPROC_FIRST ?
			result.first.str.front() : result.first.str.back())));
		break;
	case TYPE_LIST:
		val.set(result.first.listline->getListElement(
			(sproc == SPROC_FIRST) ? 1 : 
			result.first.listline->tokens.size()));
		break;
	default:
		assert(0);
	}
	return { val, result.second };
}




/*** But First - return everything except the first element (ie tail)
     But Last  - return everything except the last element ***/
t_result procBFL(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;
	t_result result = line->evalExpression(++tokpos);
	st_value val;

	switch(result.first.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	case TYPE_STR:
		{
		auto len = result.first.str.length();
		if (len < 2)
			val.set("");
		else
			val.set(result.first.str.substr((sproc == SPROC_BF ? 1 : 0),len-1));
		}
		break;
	case TYPE_LIST:
		{
		st_line *rline = result.first.listline.get();
		auto len = rline->tokens.size();
		if (len < 2) val.set(make_shared<st_line>(true));
		else
		{
			val.set(sproc == SPROC_BF ? 
				rline->getListPiece(2,len) : 
				rline->getListPiece(1,len-1));
		}
		}
		break;
	default:
		assert(0);
	}
	return { val, result.second };
}




/*** Returns a sub string or sub list. Format: PIECE <from> <to> <expr> ***/
t_result procPiece(st_line *line, size_t tokpos)
{
	// Get from
	t_result from = line->evalExpression(++tokpos);
	if (from.first.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	size_t from_pos = (size_t)from.first.num;
	if ((int)from_pos < 1)
		throw t_error({ ERR_OUT_OF_BOUNDS, line->tokens[tokpos].toString() });

	// Get to
	size_t pos = from.second;
	if (pos == line->tokens.size()) throw t_error({ ERR_MISSING_ARG, "" });

	t_result to = line->evalExpression(pos);
	size_t to_pos = (size_t)to.first.num;
	if (to.first.type != TYPE_NUM || to_pos < from_pos)
		throw t_error({ ERR_INVALID_ARG, line->tokens[pos].toString() });
	if ((int)to_pos < 1)
		throw t_error({ ERR_OUT_OF_BOUNDS, line->tokens[pos].toString() });

	// Get string or list
	pos = to.second;
	t_result result = line->evalExpression(pos);
	st_value &rval = result.first;
	st_value oval;

	switch(rval.type)
	{
	case TYPE_STR:
		if (from_pos > rval.str.length())
			oval.set("");
		else
			oval.set(rval.str.substr(from_pos-1,to_pos-from_pos+1));
		break;
	case TYPE_LIST:
		{
		st_line *rline = result.first.listline.get();
		if (from_pos > rline->tokens.size())
			oval.set(make_shared<st_line>(true));
		else
			oval.set(rline->getListPiece(from_pos,to_pos));
		}
		break;
	default:
		throw t_error({ ERR_INVALID_ARG, line->tokens[pos].toString() });
	}
	return { oval, result.second };
}




t_result procCount(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value val;

	switch(result.first.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	case TYPE_STR:
		val.set(result.first.str.length());
		break;
	case TYPE_LIST:
		val.set(result.first.listline->tokens.size());
		break;
	default:
		assert(0);
	}
	return { val, result.second };
}




/*** Returns 1 if the argument is a number ***/
t_result procNump(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	return { st_value((result.first.type == TYPE_NUM)), result.second };
}




/*** Returns 1 if the argument is a string ***/
t_result procStrp(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	return { st_value((result.first.type == TYPE_STR)), result.second };
}




/*** Returns 1 if the argument is a list ***/
t_result procListp(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	return { st_value((result.first.type == TYPE_LIST)), result.second };
}




/*** Put as first or last element ***/
t_result procFLput(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;

	// Get element to insert
	t_result element = line->evalExpression(++tokpos);
	if (line->isExprEnd(element.second))
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get list or string to insert it into
	t_result into = line->evalExpression(element.second);
	st_value &eval = element.first;
	st_value &ival = into.first;
	st_value rval;

	switch(ival.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG, line->tokens[element.second].toString() });
	case TYPE_STR:
		if (eval.type != TYPE_STR || eval.str == "")
		{
			throw t_error({ ERR_INVALID_ARG,
			                line->tokens[tokpos].toString() });
		}
		rval.set(sproc == SPROC_FPUT ?
			(eval.str + ival.str) : (ival.str + eval.str));
		break;
	case TYPE_LIST:
		rval.set(sproc == SPROC_FPUT ? 
			ival.listline->setListFirst(eval) : 
			ival.listline->setListLast(eval));
		break;
	default:
		assert(0);
	}
	return { rval, into.second };
}




/*** Return the element at the given index ***/
t_result procItem(st_line *line, size_t tokpos)
{
	// Get index
	t_result index = line->evalExpression(++tokpos);
	if (line->isExprEnd(index.second))
		throw t_error({ ERR_MISSING_ARG, "" });

	if (index.first.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	int pos = (int)index.first.num;
	if (pos < 1) 
		throw t_error({ ERR_OUT_OF_BOUNDS, line->tokens[tokpos].toString() });

	// Get list or string
	t_result index_of = line->evalExpression(index.second);
	st_value &ival = index_of.first;
	st_value rval;

	switch(ival.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG, line->tokens[index.second].toString() });
	case TYPE_STR:
		if (pos > (int)ival.str.length())
			rval.set("");
		else
			rval.set(ival.str.substr(pos-1,1));
		break;
	case TYPE_LIST:
		if (pos > (int)ival.listline->tokens.size())
			rval.set(make_shared<st_line>(true));
		else
			rval.set(ival.listline->getListElement(pos));
		break;
	default:
		assert(0);
	}
	return { rval, index_of.second };
}




/*** Return 1 if the element is in the 2nd argument ***/
t_result procMemberp(st_line *line, size_t tokpos)
{
	// Get element to search for
	t_result search_for = line->evalExpression(++tokpos);
	if (line->isExprEnd(search_for.second))
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get list or string to search in
	t_result search_in = line->evalExpression(search_for.second);
	st_value &forval = search_for.first;
	st_value &inval = search_in.first;
	st_value rval;

	switch(inval.type)
	{
	case TYPE_NUM:
		throw t_error({ ERR_INVALID_ARG,line->tokens[tokpos].toString() });
	case TYPE_STR:
		if (forval.type != TYPE_STR || forval.str == "")
		{
			throw t_error({ ERR_INVALID_ARG,
			                line->tokens[tokpos].toString() });
		}
		{
		size_t pos = inval.str.find(forval.str);
		rval.set(pos != string::npos ? pos+1 : 0);
		}
		break;
	case TYPE_LIST:
		rval.set(inval.listline->listMemberp(forval));
		break;
	default:
		assert(0);
	}
	return { rval, search_in.second };
}




t_result procUpperCase(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	for(char &c: val.str) c = toupper(c);

	return { val, result.second };
}




t_result procLowerCase(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	for(char &c: val.str) c = tolower(c);
	return { val, result.second };
}




t_result procAscii(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_STR || val.str.length() != 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	val.set((int)val.str[0]);
	return { val, result.second };
}




t_result procChar(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_NUM || val.num < 0 || val.num > 255)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	char s[2] = { (char)val.num, 0 };
	val.set(s);
	return { val, result.second };
}




/*** Read a single character from the keyboard ***/
t_result procRC(st_line *line, size_t tokpos)
{
	// The argument is a 1 or 0 for whether to echo or not
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_NUM || val.num < 0 || val.num > 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	int rr;
	st_io rio;
	rio.readInput(STDIN,(bool)val.num,true,false,rr);
	return { st_value(rio.rdline), result.second };
}




/*** Read a line from the keyboard ***/
t_result procRL(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_NUM || val.num < 0 || val.num > 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	int rr;
	st_io rio;
	while(!rio.readInput(STDIN,(bool)val.num,false,false,rr));
	return { st_value(rio.rdline), result.second };
}




/*** Turtle Facts. Format: 
     [<xpos> <ypos> <heading> <visible 1/0> <pen down 1/0> <pen colour> ***/
t_result procTF(st_line *line, size_t tokpos)
{
	if (!flags.graphics_enabled) throw t_error({ ERR_NO_GRAPHICS, "" });
	return { st_value(turtle->facts()), tokpos + 1 };
}




/*** Convert a string to a number ***/
t_result procNum(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;

	// If its already a number just return it 
	if (val.type == TYPE_NUM) return { val, result.second };

	// If its a string thats numeric convert to a number
	if (val.type == TYPE_STR && isNumber(val.str))
		return { st_value(atof(val.str.c_str())), result.second };

	throw t_error({ ERR_CANT_CONVERT,line->tokens[tokpos].toString() });
	return { };
}




/*** Convert a number or list to a string ***/
t_result procStr(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;

	if (val.type == TYPE_LIST)
	{
		// SPROC_STR:  [1 2 [3 4]] -> "[1 2 [3 4]]" or
		// SPROC_SSTR: [1 2 [3 4]] -> "1 2 3 4"
		return { st_value(sproc == SPROC_STR ? 
			val.listline->listToString() :
			val.listline->toSimpleString()
			), result.second };
	}

	// Anything else
	return { st_value(val.str), result.second };
}




t_result procSplit(st_line *line, size_t tokpos)
{
	// Get seperator
	t_result res = line->evalExpression(++tokpos);
	string sep = res.first.str;
	if (res.first.type != TYPE_STR || !sep.length())
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	tokpos = res.second;

	// Get string to split
	if (line->isExprEnd(tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	res = line->evalExpression(tokpos);
	if (res.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });
	string &str = res.first.str;

	shared_ptr<st_line> listline = make_shared<st_line>(true);
	size_t pos = 0;
	size_t epos;
	size_t len = str.length();
	string ss;

	while(true)
	{
		epos = str.find(sep,pos);
		if (epos == string::npos) 
		{
			ss = str.substr(pos,len - pos);
			listline->addToken(TYPE_STR,ss);
			break;
		}
		ss = str.substr(pos,epos-pos);
		listline->addToken(TYPE_STR,ss);
		pos = epos + 1;
	}
	return { st_value(listline), res.second };
}




/*** Randomly shuffle a list or string. Not sure how much use it is but it was 
     in Dr Logo so I'm including it here. ***/
t_result procShuffle(st_line *line, size_t tokpos)
{
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;

	switch(val.type)
	{
	case TYPE_LIST:
		val.listline->listShuffle();
		break;
	case TYPE_STR:
		{
		size_t cnt = val.str.size();
		string &str = val.str;
		for(size_t i=0;i < cnt;++i) swap(str[i],str[random() % cnt]);
		}
		break;
	default:
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	}
	return { val, result.second };
}




/*** Various single numeric argument mathematical functions ***/
t_result procMaths(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;
	t_result result = line->evalExpression(++tokpos);
	st_value &val = result.first;
	double trigval;

	st_token &tok = line->tokens[tokpos];
	if (val.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, tok.toString() });

	switch(sproc)
	{
	case SPROC_RANDOM:
		if (val.num < 1)
			throw t_error({ ERR_INVALID_ARG, tok.toString() });
		return { st_value(random() % (int)(val.num + 1)), result.second };
	case SPROC_INT:
		return { st_value((int)val.num), result.second };
	case SPROC_ROUND:
		return { st_value(roundf(val.num)), result.second };
	case SPROC_SIN:
		trigval = sin(val.num / (flags.angle_in_degs ? DEGS_PER_RADIAN : 1));
		return { st_value(trigval), result.second };
	case SPROC_COS:
		trigval = cos(val.num / (flags.angle_in_degs ? DEGS_PER_RADIAN : 1));
		return { st_value(trigval), result.second };
	case SPROC_TAN:
		trigval = tan(val.num / (flags.angle_in_degs ? DEGS_PER_RADIAN : 1));
		return { st_value(trigval), result.second };
	case SPROC_ASIN:
		if (val.num < -1 || val.num > 1)
			throw t_error({ ERR_VALUE_OUT_OF_RANGE, tok.toString() });
		trigval = asin(val.num) * (flags.angle_in_degs ? DEGS_PER_RADIAN : 1);
		return { st_value(trigval), result.second };
	case SPROC_ACOS:
		if (val.num < -1 || val.num > 1)
			throw t_error({ ERR_VALUE_OUT_OF_RANGE, tok.toString() });
		trigval = acos(val.num) * (flags.angle_in_degs ? DEGS_PER_RADIAN : 1);
		return { st_value(trigval), result.second };
	case SPROC_ATAN:
		trigval = atan(val.num) * (flags.angle_in_degs ? DEGS_PER_RADIAN : 1);
		return { st_value(trigval), result.second };
	case SPROC_LOG:
		return { st_value(logf(val.num)), result.second };
	case SPROC_LOG2:
		return { st_value(log2f(val.num)), result.second };
	case SPROC_LOG10:
		return { st_value(log10f(val.num)), result.second };
	case SPROC_SQRT:
		return { st_value(sqrtf(val.num)), result.second };
	case SPROC_ABS:
		return { st_value(abs(val.num)), result.second };
	case SPROC_SGN:
		return { st_value(SGN(val.num)), result.second };
	}
	assert(0);
	return { };
}




/*** Match a string to the pattern. Format: MATCH/MATCHC <string> <pattern>
     where pattern can contain '?' and '*'. MATCHC matches case too. ***/
t_result procMatch(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;

	// Get string
	t_result rstr = line->evalExpression(++tokpos);
	if (rstr.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	// Get pattern
	size_t pos = rstr.second;
	t_result rpat = line->evalExpression(pos);
	if (rpat.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	const char *str = rstr.first.str.c_str();
	const char *pat = rpat.first.str.c_str();
	return { st_value(wildMatch(str,pat,sproc == SPROC_MATCHC)), rpat.second };
}




t_result procDir(st_line *line, size_t tokpos)
{
	string dirname;
	string matchpath;
	size_t endpos;

        // Get dirname. If there isn't one default to "."
	if (line->isExprEnd(++tokpos))
	{
		dirname = ".";
		endpos = tokpos;
	}
	else
	{
		t_result result = line->evalExpression(tokpos);
		if (result.first.type != TYPE_STR || !result.first.str.length())
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		dirname = result.first.str;
		endpos = result.second;
	}
	en_error err;
	char *str;
	assert((str = strdup(dirname.c_str())));
	err = matchPath(S_IFDIR,(char *)dirname.c_str(),matchpath);
	free(str);
	if (err != OK) throw t_error({ err, line->tokens[tokpos].toString() });

	DIR *dir;
	struct dirent *de;
	struct stat fs;
	shared_ptr<st_line> listline = make_shared<st_line>(true);

	if (!(dir = opendir(matchpath.c_str())))
		throw t_error({ ERR_OPEN_FAIL, matchpath });
	string file;
	string entry;
	string path;

	while((de = readdir(dir)))
	{
		path = dirname + "/" + de->d_name;
		if (stat(path.c_str(),&fs) == -1 || 
		    (fs.st_mode & S_IFMT) == S_IFREG)
		{
			// Only include LOGO files
			file = de->d_name;
			if (file.size() > 2 &&
			    file.substr(file.size() - 3,3) == LOGO_FILE_EXT)
			{
				entry = file.substr(0,file.size() - 3);
				listline->addToken(TYPE_STR,entry);
			}
		}
	}
	closedir(dir);
	return { st_value(listline), endpos };
}




t_result procGetDir(st_line *line, size_t tokpos)
{
	char *dir = getcwd(NULL,0);
	string sdir = dir;
	free(dir);
	return { st_value(sdir), tokpos + 1 };
}




t_result procGetSecs(st_line *line, size_t tokpos)
{
	return { st_value(time(0)), tokpos + 1};
}




// Format: GETDATE <epoch secs> <output format> <utc 0/1>
t_result procGetDate(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos))
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get epoch seconds
	t_result rsecs = line->evalExpression(tokpos);
	if (rsecs.first.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });

	tokpos = rsecs.second;
	if (line->isExprEnd(tokpos))
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get datetime format
	t_result rformat = line->evalExpression(tokpos);
	st_value &vformat = rformat.first;
	if (vformat.type != TYPE_STR || vformat.str == "")
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });

	// Create date string
	const char *format = vformat.str.c_str();
	time_t tmsecs = (time_t)rsecs.first.num;
	struct tm *tms;
	char dstr[200];

	// 'U' at the start of the format means get UTC date
	if (format[0] == 'U')
	{
		 ++format;
		tms = gmtime(&tmsecs);
	}
	else tms = localtime(&tmsecs);

	if (!strftime(dstr,sizeof(dstr),format,tms))
		throw t_error({ ERR_INVALID_ARG, format });
	
	return { st_value(dstr), rformat.second };
}




/* Simple number formatting. Returns a string. Eg:
   FMT 12.3 ".#"      -> 12.3
   FMT 12.34 "#.###"  -> 2.340  
   FMT 12.34 "###."   -> 0123.34
 */
t_result procFmt(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos))
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get number to format
	t_result rnum = line->evalExpression(tokpos);
	if (rnum.first.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_FMT, line->tokens[tokpos].strval });
	tokpos = rnum.second;

	// Get format
	if (line->isExprEnd(tokpos))
		throw t_error({ ERR_MISSING_ARG, "" });

	t_result rfmt = line->evalExpression(tokpos);
	if (rfmt.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_FMT, line->tokens[tokpos].strval });
	st_value &vfmt = rfmt.first;

	// Validate the formatting
	bool rpad = false;
	bool lpad = false;
	const char *fdotptr = NULL;

	for(const char *ptr=vfmt.str.c_str();*ptr;++ptr)
	{
		switch(*ptr)
		{
		case '#':
			if (fdotptr)
				rpad = true;
			else
				lpad = true;
			break;
		case '.':
			if (!fdotptr)
			{
				fdotptr = ptr;
				break;
			}
			// Fall through
		default:
			throw t_error({ ERR_INVALID_FMT, vfmt.str });
		}
	}
	if (!fdotptr) throw t_error({ ERR_INVALID_FMT, vfmt.str });

	char numstr[50];
	char *ndotptr;
	string outstr;

	// Not using the sprintf padding as it rounds up
	sprintf(numstr,"%f",rnum.first.num);
	if (!(ndotptr = strchr(numstr,'.'))) ndotptr = numstr + strlen(numstr);

	// Pad the integer part
	if (lpad)
	{
		char *nptr = ndotptr;
		for(const char *fptr=fdotptr-1;fptr >= vfmt.str.c_str();--fptr)
		{
			if (--nptr < numstr)
				outstr += '0';
			else
				outstr += *nptr;
		}
		reverse(outstr.begin(),outstr.end());
	}
	else
	{
		*ndotptr = 0;
		outstr += numstr;
	}

	// Pad the floating point part
	if (rpad)
	{
		outstr += ".";
		char *nptr = ndotptr;
		for(const char *fptr=fdotptr+1;*fptr;++fptr)
		{
			if (*++nptr)
				outstr += *nptr;
			else
				outstr += '0';
		}
	}
	else if (ndotptr) outstr = outstr + "." + ++ndotptr;
	
	return { st_value(outstr), rfmt.second };
}




/*** Left and right pad. It doesn't chop if the string is longer than the
     padding. Format: LPAD <string to pad> <pad char> <pad count> ***/
t_result procPad(st_line *line, size_t tokpos)
{
	int sproc = line->tokens[tokpos].subtype;

	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	
	// Get string to pad
	t_result rstr = line->evalExpression(tokpos);
	if (rstr.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });
	tokpos = rstr.second;
	string str = rstr.first.str;

	// Get what to pad with 
	if (line->isExprEnd(tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	rstr = line->evalExpression(tokpos);
	string &padstr = rstr.first.str;
	if (rstr.first.type != TYPE_STR || padstr.size() != 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });
	tokpos = rstr.second;

	// Get pad length
	if (line->isExprEnd(tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	t_result rnum = line->evalExpression(tokpos);
	if (rnum.first.type != TYPE_NUM || rnum.first.num < 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });
	long padlen = (long)rnum.first.num - str.size();
	
	if (padlen > 0)
	{
		if (sproc == SPROC_LPAD)
			str = string(padlen,padstr[0]) + str;
		else
			str += string(padlen,padstr[0]);
	}
	return { st_value(str), rnum.second };
}




t_result procList(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	
	// Get string to pad
	t_result rstr = line->evalExpression(tokpos);
	if (rstr.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });

	shared_ptr<st_line> listline = make_shared<st_line>(true);
	listline->tokenise(rstr.first.str);

	return { st_value(listline), rstr.second };
}




t_result procPath(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	
	// Get path to expand
	t_result path = line->evalExpression(tokpos);
	if (path.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].strval });

	string matchpath;
	en_error err;
	char *str;

	assert((str = strdup(path.first.str.c_str())));
	err = matchPath(S_IFDIR,str,matchpath);
	free(str);

	if (err == OK) return { st_value(matchpath), path.second };
	return { st_value(""), path.second };
}

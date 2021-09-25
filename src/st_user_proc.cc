#include "globals.h"

st_user_proc::st_user_proc(string &_name, st_line *line, size_t &tokpos):
	name(_name)
{
	exec_linenum = 0;
	next_linenum = 10;

	// Go through parameters. Format is TO <proc> [:<var>] * N
	for(++tokpos;tokpos < line->tokens.size();++tokpos)
	{
		st_token &tok = line->tokens[tokpos];

		// Only want var types. If its not then assume its an inline
		// command for the proc and return;
		string &param = line->tokens[tokpos].strval;
		if (tok.type != TYPE_VAR) return;

		const char *cparam = param.c_str()+1;
		if (find(params.begin(),params.end(),cparam) != params.end())
			throw t_error({ ERR_DUP_DECLARATION, param });

		params.push_back(cparam);
	}
}




/*** Add a line onto the end of the lines vector ***/
void st_user_proc::addLine(st_line *line)
{
	assert(logo_state == STATE_DEF_PROC && def_proc);

	// This will only execute an END which will change the state back
	// to STATE_CMD
	size_t tokpos = line->execute();

	switch(logo_state)
	{
	case STATE_DEF_PROC:
		// Add line to procedure
		line->setLineNum(next_linenum);
		line->parent_proc = def_proc.get();
		next_linenum += 10;
		lines.push_back(shared_ptr<st_line>(new st_line(line)));
		break;
	case STATE_CMD:
		// If END wasn't the first command on the line then add
		// what came before
		if (line->tokens.size() > 1)
		{
			assert(tokpos > 1);
			lines.push_back(shared_ptr<st_line>(new st_line(
				def_proc.get(),next_linenum,line,0,tokpos-2)));
			next_linenum += 10;
		}
		break;
	default:
		assert(0);
	}
}




/*** Add a new line in the given position or replace one based on line num ***/
void st_user_proc::addReplaceDeleteLine(int linenum, st_line *line, size_t from)
{
	st_line *newline = NULL;

	// If no tokens then we want to delete the line
	if (from < line->tokens.size())
	{
		newline = new st_line(
			this,linenum,line,from,line->tokens.size()-1);
	}

	// Go through lines and see if we have the number
	for(auto vit=lines.begin();vit != lines.end();++vit)
	{
		auto &pline = *vit;
		if (pline->linenum < linenum) continue;
		if (pline->linenum == linenum)
		{
			// Replace or delete
			if (newline)
				vit->reset(newline);
			else
				lines.erase(vit);
			return;
		}
		// Insert between
		if (newline) lines.insert(vit,shared_ptr<st_line>(newline));
		return;	
	}
	// Just add to the end
	if (newline)
	{
		next_linenum = linenum + 10;
		lines.push_back(shared_ptr<st_line>(newline));
	}
}




/*** If a line has the label return its index ***/
size_t st_user_proc::labelLineIndex(const char *name)
{
	for(size_t pos=0;pos < lines.size();++pos)
		if ((int)lines[pos]->labelIndex(name) != -1) return pos;
	return -1;
}




/*** Renumber all lines starting at 10 ***/
void st_user_proc::renumber()
{
	int linenum = 10;
	for(auto &line: lines)
	{
		line->setLineNum(linenum);
		linenum += 10;
	}
}




/*** Run each line ***/
void st_user_proc::execute()
{
	size_t exec_from = 0;

	for(size_t lpos=0;lpos < lines.size();)
	{
		st_line *line = lines[lpos].get();

		// exec_linenum used in error messages
		exec_linenum = line->linenum;

		try
		{
			line->execute(exec_from);
			exec_from = 0;
			++lpos;
		}
		catch(t_interrupt &inter)
		{
			if (inter.first != INT_GOTO) throw;

			size_t index = labelLineIndex(inter.second.c_str());
			if ((int)index == -1)
				throw t_error({ ERR_UNDEFINED_LABEL, inter.second });
			lpos = index;
			exec_from = lines[lpos]->labelIndex(inter.second.c_str());
			assert((int)exec_from != -1);
		}
	}
}




void st_user_proc::dump(FILE *fp, bool full_dump, bool show_linenums)
{
	fprintf(fp,"TO %s ",name.c_str());
	for(string &str: params)
	{
		if (fprintf(fp,":%s ",str.c_str()) == -1)
			throw t_error({ ERR_WRITE_FAIL, "" });
	}

	if (full_dump)
	{
		int indent = 0;

		// Don't need to check every write for a fail
		fputc('\n',fp);
		for(auto &pline: lines)
		{
			pline->dump(fp,indent,show_linenums);
			fputc('\n',fp);
		}
		if (fprintf(fp,"END\n") == -1)
			throw t_error({ ERR_WRITE_FAIL, "" });
	}
}

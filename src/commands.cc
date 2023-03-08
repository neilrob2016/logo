#include "globals.h"


////////////////////////////// PROGRAMMING COMMANDS ///////////////////////////

size_t comRem(st_line *line, size_t tokpos)
{
	// Just skip to the end of the line
	return line->tokens.size();
}




/*** Format is: ED <proc name> <linenum> <line>.
     If the line exists it will be replaced else a new line is added unless
     the line portion is empty in which case it will be deleted. ***/
size_t comEd(st_line *line, size_t tokpos)
{
	if (curr_proc_inst) throw t_error({ ERR_UNEXPECTED_ED, "" });

	if (line->tokens.size() - tokpos < 3)
		throw t_error({ ERR_MISSING_ARG, "" });

	// Don't allow expressions for the proc name and line number, just
	// raw values
	if (line->tokens[++tokpos].type != TYPE_UPROC)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	string &procname = line->tokens[tokpos].strval;

	int linenum = line->tokens[++tokpos].numval;
	if (line->tokens[tokpos].type != TYPE_NUM || linenum < 1)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	// Add line to the procedure
	auto mit = user_procs.find(procname);
	if (mit == user_procs.end())
		throw t_error({ ERR_UNDEFINED_UPROC, procname });
	mit->second->addReplaceDeleteLine(linenum,line,++tokpos);

	// ED command takes up whole line
	return line->tokens.size();
}




size_t comTo(st_line *line, size_t tokpos)
{
	st_token &tok = line->tokens[++tokpos];

	if (logo_state == STATE_DEF_PROC || logo_state == STATE_IGN_PROC)
		throw t_error({ ERR_UNEXPECTED_TO, "" });
	if (tok.type != TYPE_UPROC)
		throw t_error({ ERR_INVALID_ARG, tok.toString() });

	string &name = tok.strval;

	if (loadproc != "" && name != loadproc)
	{
		logo_state = STATE_IGN_PROC;
		return line->tokens.size();
	}

	// See if procedure is already defined 
	if (user_procs.find(name) != user_procs.end())
		throw t_error({ ERR_DUP_DECLARATION, name });

	// Constructor updates tokpos. Don't add to user_procs yet as Control^C
	// (BREAK) can stop proc entry
	def_proc.reset(new st_user_proc(name,line,tokpos));
	logo_state = STATE_DEF_PROC;

	// If extra stuff on line following proc header then add as a new
	// line to procedure
	if (tokpos < line->tokens.size())
	{
		st_line *tmpline = NULL;

		// Create temp line, bit of a hack but efficiency not 
		// important here.
		try
		{
			tmpline = new st_line(
				def_proc.get(),
				0,line,tokpos,line->tokens.size()-1);
			def_proc->addLine(tmpline);
		}
		catch(...)
		{
			logo_state = STATE_CMD;
			delete tmpline;
			throw;
		}
		delete tmpline;
	}
	return line->tokens.size();
}




size_t comEnd(st_line *line, size_t tokpos)
{
	if (logo_state != STATE_DEF_PROC && logo_state != STATE_IGN_PROC)
		throw t_error({ ERR_UNEXPECTED_END, "" });

	// Can't have anything following
	if (tokpos < line->tokens.size() - 1)
		throw t_error({ ERR_UNEXPECTED_ARG, line->tokens[tokpos+1].toString() });
	if (logo_state == STATE_DEF_PROC)
	{
		user_procs[def_proc->name] = def_proc;
		cout << "Procedure \"" << def_proc->name << "\" defined.\n";
	}
	logo_state = STATE_CMD;
	return tokpos + 1;
}




/*** Restart the interpreter ***/
size_t comRestart(st_line *line, size_t tokpos)
{
	// Caught in mainloop()
	throw t_interrupt({ INT_RESTART, "" });
}




/*** Erase a user procedure or a variable ***/
size_t comEr(st_line *line, size_t tokpos)
{
	size_t pos;

	// Can't use isExprEnd() because looking for TYPE_UPROC/VAR
	if (tokpos >= line->tokens.size() - 1)
		throw t_error({ ERR_MISSING_ARG, "" });

	for(pos=++tokpos;pos < line->tokens.size();++pos)
	{
		st_token &tok = line->tokens[pos];
		switch(tok.type)
		{
		case TYPE_VAR:
			// We only bother with globals, don't go hunting for
			// locals with the name
			{
			string varname = tok.strval.substr(1);
			auto mit = global_vars.find(varname);
			if (mit == global_vars.end())
				throw t_error({ ERR_UNDEFINED_VAR, varname });
			global_vars.erase(mit);
			cout << "Global variable \"" << varname << "\" erased.\n";
			}
			break;
		case TYPE_UPROC:
			{
			auto mit = user_procs.find(tok.strval);
			if (mit == user_procs.end())
				throw t_error({ ERR_UNDEFINED_UPROC, tok.toString() });

			user_procs.erase(mit);
			cout << "User procedure \"" << tok.strval 
			     << "\" erased.\n";
			}
			break;
		default:
			if (pos > tokpos) break;
			throw t_error({ ERR_INVALID_ARG, tok.toString() });
		}
	}
	return pos;
}




/*** Erase stuff ***/
size_t comErall(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos].subtype;

	// Can't erase a procedure while we're in it
	if (curr_proc_inst && (com == COM_ERP || com == COM_ERALL))
		throw t_error({ ERR_UNEXPECTED_ERASE, "" });

	if (com == COM_ERV || com == COM_ERALL)
	{
		if (com == COM_ERALL) flags.angle_in_degs = true;
		clearGlobalVariables();
		puts("Global variables deleted and system variables reset.");
	}
	if (com == COM_ERP || com == COM_ERALL)
	{
		if (user_procs.size())
		{
			user_procs.clear();
			puts("User procedures deleted.");
		}
	}
	if (com == COM_ERALL)
	{
		if (watch_vars.size())
		{
			watch_vars.clear();
			puts("Watch variables list cleared.");
		}
	}
	ready();
	return tokpos + 1;
}




/*** These all take a user proc as their argument ***/
size_t comPoRenum(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos].subtype;
	size_t pos;

	// Can't use isExprEnd() because looking for TYPE_UPROC
	if (tokpos >= line->tokens.size() - 1)
		throw t_error({ ERR_MISSING_ARG, "" });

	for(pos=++tokpos;pos < line->tokens.size();++pos)
	{
		st_token &tok = line->tokens[pos];
		if (tok.type != TYPE_UPROC)
		{
			if (pos > tokpos) break;
			throw t_error({ ERR_INVALID_ARG, tok.toString() });
		}
		auto mit = user_procs.find(tok.strval);
		if (mit == user_procs.end())
			throw t_error({ ERR_UNDEFINED_UPROC, tok.toString() });

		switch(com)
		{
		case COM_PO:
			mit->second->dump(stdout,true,false);
			break;
		case COM_POL:
			mit->second->dump(stdout,true,true);
			break;
		case COM_RENUM:
			mit->second->renumber();
			cout << "User procedure \"" << tok.strval 
			     << "\" renumbered.\n";
			break;
		default:
			assert(0);
		}
	}
	return pos;
}




/*** Print out all procedures ***/
size_t comPops(st_line *line, size_t tokpos)
{
	bool full_dump = true;
	bool show_linenums = false;

	switch(line->tokens[tokpos].subtype)
	{
	case COM_POPSL:
		show_linenums = true;
		break;
	case COM_POTS:
		full_dump = false;
	}

	if (user_procs.empty()) cout << "No user procedures defined.\n";
	else
	{
		puts("User procedures");
		puts("---------------");
		for(auto &[name,proc]: user_procs)
		{
			proc->dump(stdout,full_dump,show_linenums);
			putchar('\n');
		}
	}
	return tokpos + 1;
}




/*** Print Out NameS, ie dump global variables ***/
size_t comPons(st_line *line, size_t tokpos)
{
	puts("Global variables");
	puts("----------------");

	// Print out system vars first then user defined ones
	for(int i=0;i < 2;++i)
	{
		for(auto &[name,value]: global_vars)
		{
			if ((!i && name[0] == '$') || (i && name[0] != '$'))
				cout << name << " is " << value.dump(true) << endl;
		}
	}
	return tokpos + 1;
}




/*** Print some system flags ***/
size_t comPosf(st_line *line, size_t tokpos)
{
	puts("System flags");
	puts("------------");
	printf("Indentation is %s.\n",flags.indent_label_blocks ? "on" : "off");
	printf("Fill is %s.\n",
		flags.graphics_enabled && turtle->store_fill ? "on" : "off");
	
	return tokpos + 1;
}




/*** Dump everything ***/
size_t comPoall(st_line *line, size_t tokpos)
{
	comPops(line,tokpos);
	comPons(line,tokpos);
	putchar('\n');
	comPosf(line,tokpos);

	return tokpos + 1;
}




/*** PR and WR ***/
size_t comPrWr(st_line *line, size_t tokpos)
{
	// The token won't be a command if we're being called as a default
	// command from st_line::execute()
	int com;
	if (line->tokens[tokpos].type == TYPE_COM)
	{
		com = line->tokens[tokpos++].subtype;
		if (line->isExprEnd(tokpos))
		{
			if (com == COM_PR) putchar('\n');
			return tokpos;
		}
	}
	else com = COM_PR;

	t_result result;

	// Eval any following expressions until we hit another command or EOL
	do
	{
		result = line->evalExpression(tokpos);
		// If result value is undefined don't print anything
		if (result.first.type != TYPE_UNDEF)
			cout << result.first.dump(false) << flush;
		tokpos = result.second;
	} while(!line->isExprEnd(tokpos));

	if (result.first.type != TYPE_UNDEF && com == COM_PR) putchar('\n');

	return result.second;
}




/*** Allows a procedure to be executed without the result appearing in the
     console  ***/
size_t comEat(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	t_result result = line->evalExpression(tokpos);
	return result.second;
}




/*** MAKE and MAKELOC variable create/set commands ***/
size_t comMake(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos++].subtype;
	if (line->tokens.size() - tokpos < 2)
		throw t_error({ ERR_MISSING_ARG, "" });

	if (com == COM_MAKELOC && !curr_proc_inst)
		throw t_error({ ERR_NOT_IN_UPROC, "" });

	// Get variable name as a string
	t_result result = line->evalExpression(tokpos);
	if (result.first.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	string varname = result.first.str;
 	if (varname[0] == '$') 
		throw t_error({ ERR_READ_ONLY_VAR, varname });

	// Check for valid name
	if (varname == "" || isNumber(varname))
		throw t_error({ ERR_INVALID_VAR_NAME, varname });
	for(char &c: varname)
	{
		if (c < 33 || strchr(OP_LIST_STR,c))
			throw t_error({ ERR_INVALID_VAR_NAME, varname });
	}

	tokpos = result.second;
	result = line->evalExpression(tokpos);
	if (result.first.type == TYPE_UNDEF)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });


	if (com == COM_MAKE)
		setGlobalVarValue(varname,result.first);
	else if (curr_proc_inst)
		curr_proc_inst->setLocalVar(varname,result.first);
	else throw t_error({ ERR_NOT_IN_UPROC, "" });

	return result.second;
}




/*** INCrement and DECrement are short cuts for the common actions of adding or
     subtracting by 1 instead of doing MAKE "var" :var + 1 ***/
size_t comIncDec(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos++].subtype;
	if (line->tokens.size() - tokpos < 1)
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get the variable name
	st_token &tok = line->tokens[tokpos];
	if (tok.type != TYPE_VAR)
		throw t_error({ ERR_INVALID_ARG, tok.toString() });

	string varname = tok.strval.substr(1);
	st_value val;

	// See if its global
	auto mit = global_vars.find(varname);
	if (mit == global_vars.end())
	{
		// Try local
		if (!curr_proc_inst || 
		    !curr_proc_inst->getLocalVarValue(varname,val))
		{
			throw t_error({ ERR_UNDEFINED_VAR, varname });
		}
		if (val.type != TYPE_NUM)
			throw t_error({ ERR_INVALID_VAR_TYPE, varname });
		val.num += (com == COM_INC ? 1 : -1);
		curr_proc_inst->setLocalVar(varname,val);
	}
	else
	{
		// Global
		val = mit->second;
		if (val.type != TYPE_NUM)
			throw t_error({ ERR_INVALID_VAR_TYPE, varname });
		val.num += (com == COM_INC ? 1 : -1);
		setGlobalVarValue(varname,val);
	}
	return tokpos + 1;
}




/*** Runs the contents of the given lists or string. The list version was in 
     Dr LOGO so including it here though I think string is more useful***/
size_t comRun(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	t_result result;

	// Exec every list or string we find
	do
	{
		result = line->evalExpression(tokpos);

		switch(result.first.type)
		{
		case TYPE_LIST:
			result.first.listline->execute();
			break;
		case TYPE_STR:
			{
			bool tmp = flags.suppress_prompt;
			flags.suppress_prompt = true;
			st_io tmpio;
			tmpio.execText(result.first.str);
			flags.suppress_prompt = tmp;
			}
			break;
		default:
			throw t_error({ ERR_CANT_RUN,
			                line->tokens[tokpos].toString() });
		}

		tokpos = result.second;
	} while(!line->isExprEnd(tokpos));

	return result.second;
}




size_t comStop(st_line *line, size_t tokpos)
{
	throw t_interrupt({ INT_STOP, "" });
}




size_t comWait(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	// Get number of times to repeat
	t_result result = line->evalExpression(tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_NUM || val.num < 0)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	usleep(val.num * 1000000);

	return result.second;
}




size_t comBye(st_line *line, size_t tokpos)
{
	goodbye();
	return 0;
}




size_t comRepeat(st_line *line, size_t tokpos)
{
	++tokpos;
	if (line->tokens.size() - tokpos < 2)
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get number of times to repeat
	t_result result = line->evalExpression(tokpos);
	if (result.second >= line->tokens.size())
		throw t_error({ ERR_MISSING_ARG, "" });
	if (result.first.type != TYPE_NUM || result.first.num < 0)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	int cnt = result.first.num;

	// Get list to repeat
	tokpos = result.second;
	result = line->evalExpression(tokpos);
	if (result.first.type != TYPE_LIST)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	// Execute the listline the required number of times
	for(int i=0;i < cnt;++i)
	{
		// A STOP inside a REPEAT only stops the looping, not the whole
		// program execution
		try
		{
			result.first.listline->execute();
		}
		catch(t_interrupt &inter)
		{
			if (inter.first == INT_STOP) break;
			throw;
		}
	}

	return result.second;
}




size_t comIf(st_line *line, size_t tokpos)
{
	++tokpos;
	if (line->tokens.size() - tokpos < 2)
		throw t_error({ ERR_MISSING_ARG, "" });

	// Get condition
	t_result cond = line->evalExpression(tokpos);
	if (cond.second >= line->tokens.size())
		throw t_error({ ERR_IF_MISSING_BLOCK, "" });
	st_value &cond_val = cond.first;
	
	// True and false blocks must be lists, not expressions. It simply
	// gets too complex with the latter. Can always do [exec <expr>]
	tokpos = cond.second;
	st_token &true_tok = line->tokens[tokpos];
	if (true_tok.type != TYPE_LIST)
		throw t_error({ ERR_IF_REQ_LIST, true_tok.toString() });
	++tokpos;

	// See if we have a FALSE condition list 
	bool have_false = (tokpos < line->tokens.size() && 
	                   line->tokens[tokpos].type == TYPE_LIST);

	// If value non zero or a non empty string or list execute TRUE list
	if (cond_val.isSet())
	{
		true_tok.listline->execute();
		if (have_false) return tokpos + 1;
	}
	// Execute FALSE list. If there is no list do nothing.
	else if (have_false)
	{
		line->tokens[tokpos].listline->execute();
		return tokpos + 1;
	}
	return tokpos;
}




/*** Already error checked and set up in st_line::setLabels() so just skip ***/
size_t comLabel(st_line *line, size_t tokpos)
{
	return tokpos + (line->tokens[tokpos].subtype == COM_DLABEL ? 1 : 2);
}




/*** GO occurs through an exception which is caught in st_line::execute() where
     the program counter can be updated. We can't simply set the counter as a
     return value here because tokpos is valid ONLY in the current line, not 
     any user procedure that may be running. ***/
size_t comGo(st_line *line, size_t tokpos)
{
	// If no argument given default to ""
	if (line->isExprEnd(++tokpos)) throw t_interrupt({ INT_GOTO, "" });

	t_result result = line->evalExpression(tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_STR)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	throw t_interrupt({ INT_GOTO, val.str });
	return -1;
}




/*** OP - returns from a procedure with an optional value ***/
size_t comOp(st_line *line, size_t tokpos)
{
	if (!curr_proc_inst) throw t_error({ ERR_NOT_IN_UPROC, "" });
	if (line->isExprEnd(++tokpos))
		curr_proc_inst->ret_set = false;
	else
	{
		curr_proc_inst->retval = line->evalExpression(tokpos);
		curr_proc_inst->ret_set = true;
	}
	throw t_interrupt({ INT_RETURN, "" });
	return 0;
}



/////////////////////////// GRAPHICS/TURTLE COMMANDS ///////////////////////////

/*** Graphics commands with no arguments. The slight reduction in speed from the
     switch is worth the huge reduction in duplicated code ***/
size_t comGraphics0Args(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos].subtype;

	if (com != COM_TG && !flags.graphics_enabled)
		throw t_error({ ERR_NO_GRAPHICS, "" });

	switch(line->tokens[tokpos].subtype)
	{ 
	case COM_HOME:
		turtle->home();
		break;
	case COM_CLEAR:
		// Clears the screen but doesn't reset the turtle
		turtle->clear();
		break;
	case COM_HT:
		turtle->setVisible(false);
		break;
	case COM_ST:
		turtle->setVisible(true);
		break;
	case COM_HW:
		xWindowUnmap();
		break;
	case COM_SW:
		xWindowMap();
		break;
	case COM_CS:
		// Clears the screen and resets the turtle
		turtle->reset();
		break;
	case COM_PU:
		turtle->setPenDown(false);
		break;
	case COM_PD:
		turtle->setPenDown(true);
		break;
	case COM_WINDOW:
		turtle->setUnbounded();
		break;
	case COM_FENCE:
		turtle->setFence();
		break;
	case COM_WRAP:
		turtle->setWrap();
		break;
	case COM_SETFILL:
		turtle->setFill();
		break;
	case COM_FILL:
		turtle->fill();
		break;
	case COM_TG:
		// Toggles between graphics enabled and disabled
		if (flags.graphics_enabled)
			xDisconnect();
		else if (!xConnect())
			throw t_error({ ERR_GRAPHICS_INIT_FAIL, "" });

		// The X display and flags.map_window remains as set in 
		// parseCmdLine(). Perhaps in the future these might be options
		flags.graphics_enabled = !flags.graphics_enabled;
		break;
	default:
		assert(0);
	}
	return tokpos + 1;
}




/*** Graphics commands with 1 argument ***/
size_t comGraphics1Arg(st_line *line, size_t tokpos)
{
	if (!flags.graphics_enabled) throw t_error({ ERR_NO_GRAPHICS, "" });

	int com = line->tokens[tokpos].subtype;
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });
	t_result result = line->evalExpression(tokpos);
	st_value &val = result.first;
	if (com != COM_SETLS && val.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	switch(com)
	{
	case COM_FD:
		turtle->move(val.num);
		break;
	case COM_BK:
		turtle->move(-val.num);
		break;
	case COM_LT:
		if (!flags.angle_in_degs) val.num *= DEGS_PER_RADIAN;
		turtle->rotate(-val.num);
		break;
	case COM_RT:
		if (!flags.angle_in_degs) val.num *= DEGS_PER_RADIAN;
		turtle->rotate(val.num);
		break;
	case COM_SETPC:
		turtle->setColour((int)val.num);
		break;
	case COM_SETBG:
		turtle->setBackground((int)val.num);
		break;
	case COM_SETX:
		turtle->setX(val.num);
		break;
	case COM_SETY:
		turtle->setY(val.num);
		break;
	case COM_SETH:
		turtle->setAngle(val.num);
		break;
	case COM_SETSZ:
		turtle->setSize(val.num);
		break;
	case COM_SETLW:
		turtle->setLineWidth((int)val.num);
		break;
	case COM_SETLS:
		if (val.type != TYPE_STR)
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		turtle->setLineStyle(val.str);
		break;
	default:
		assert(0);
	}
	return result.second;
}




/*** In Dr Logo this commands take a 2 argument list but that seems a bit 
     silly to me so here it takes 2 normal arguments ***/
size_t comGraphics2Args(st_line *line, size_t tokpos)
{
	if (!flags.graphics_enabled) throw t_error({ ERR_NO_GRAPHICS, "" });
	if (line->tokens.size() - tokpos < 2)
		throw t_error({ ERR_MISSING_ARG, "" });

	int com = line->tokens[tokpos].subtype;

	// Get X location
	t_result xres = line->evalExpression(++tokpos);
	size_t ypos = xres.second;
	if (line->isExprEnd(ypos))
		throw t_error({ ERR_MISSING_ARG, "" });
	st_value &xval = xres.first;
	if (xval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	// Get Y location
	t_result yres = line->evalExpression(ypos);
	st_value &yval = yres.first;
	if (yval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, line->tokens[ypos].toString() });

	switch(com)
	{
	case COM_SETPOS:
		turtle->setXY(xval.num,yval.num);
		break;
	case COM_SETWINSZ:
		if (xval.num < 1)
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		if (yval.num < 1)
			throw t_error({ ERR_INVALID_ARG, line->tokens[ypos].toString() });
		xResizeWindow((int)xval.num,(int)yval.num);
		break;
	case COM_TOWARDS:
		turtle->setTowards(xval.num,yval.num);
		break;
	case COM_DOT:
		turtle->drawDot((short)xval.num,(short)yval.num);
		break;
	default:
		assert(0);
	}
	return yres.second;;
}


///////////////////////////////////////////////////////////////////////////////


/*** Toggle indenting of label blocks in procedure listings on and off ***/
size_t comSetInd(st_line *line, size_t tokpos)
{
	flags.indent_label_blocks = !flags.indent_label_blocks;
	printf("User procedure label block indenting %s.\n",
		flags.indent_label_blocks ? "on" : "off");
	return tokpos + 1;
}




/*** Save 1 or all procedures to a file ***/
size_t comSave(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	// Get filepath
	t_result result = line->evalExpression(tokpos);
	if (result.first.type != TYPE_STR || result.first.str == "")
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	string &filepath = result.first.str;
	string procname;
	size_t endpos = result.second;
	bool psave = false;

	// See if we have an optional procedure name 
	if (!line->isExprEnd(endpos))
	{
		t_result result2 = line->evalExpression(endpos);
		if (result2.first.type != TYPE_STR)
			throw t_error({ ERR_INVALID_ARG, line->tokens[endpos].toString() });
		endpos = result2.second;
		if ((procname = result2.first.str) != "")
		{
			if (user_procs.find(procname) == user_procs.end())
				throw t_error({ ERR_UNDEFINED_UPROC, procname });
			psave = true;
		}
	}
	else if (user_procs.empty())
	{
		cout << "No user procedures to save.\n";
		return endpos;
	}
	try
	{
		saveProcFile(filepath,procname,psave);
	}
	catch(t_error &err)
	{
		err.second = line->tokens[tokpos].strval;
		throw;
	}
	return endpos;
}




size_t comLoad(st_line *line, size_t tokpos)
{
	string filename;
	string procname;
	t_result result;

	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	// Get filepath
	result = line->evalExpression(tokpos);
	if (result.first.type != TYPE_STR || result.first.str == "")
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	filename = result.first.str;

	// Get optional procedure name
	if (!line->isExprEnd(++tokpos))
	{
		result = line->evalExpression(tokpos);
		// Allow "" so can be used as string value for all procs
		if (result.first.type != TYPE_STR)
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		procname = result.first.str;
	}

	// Wildcard path matching done in loadProcFile()
	try
	{
		loadProcFile(filename,procname);
	}
	catch(...)
	{
		loadproc = "";
		if (logo_state == STATE_IGN_PROC) logo_state = STATE_CMD;
		throw;
	}
	loadproc = "";
	if (logo_state == STATE_IGN_PROC) logo_state = STATE_CMD;
	return result.second;
}




size_t comCD(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	// Get dirname
	t_result result = line->evalExpression(tokpos);
	if (result.first.type != TYPE_STR || result.first.str == "")
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });

	string &matchpath = result.first.str;
	if (pathHasWildCards(matchpath))
	{
		char *dirname;
		assert((dirname = strdup(matchpath.c_str())));
		en_error err = matchPath(S_IFDIR,dirname,matchpath);
		free(dirname);
		if (err != OK) throw t_error({ err, "" });
	}

	if (chdir(matchpath.c_str()) == -1)
		throw t_error({ ERR_CD_FAIL, "" });
	return result.second;
}




/*** HELP dumps the commands and system procs as they come in the array.
     SHELP returns them in alphabetic order ***/
size_t comHelp(st_line *line, size_t tokpos)
{
	int com = line->tokens[tokpos].subtype;
	set<string> sorted;
	t_result result;
	const char *pattern;
	const char *str;
	int cnt;
	int i;

	// If we have an argument it should be a wildcard match pattern
	if (!line->isExprEnd(++tokpos))
	{
		result = line->evalExpression(tokpos);
		if (result.first.type != TYPE_STR)
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		pattern = result.first.str.c_str();
		tokpos = result.second;
	}
	else pattern = NULL;

	puts("Commands");
	puts("--------");
	for(i=cnt=0;i < NUM_COMS;++i)
	{
		str = commands[i].first;
		if (!pattern || wildMatch(str,pattern,false))
		{
			if (com == COM_HELP)
			{
				printf("   %-8s",str);
				if (!((++cnt) % 7)) putchar('\n');
			}
			else sorted.insert(str);
		}
	}
	if (com == COM_SHELP)
	{
		cnt = 0;
		for(auto &s: sorted)
		{
			printf("   %-8s",s.c_str());
			if (!((++cnt) % 7)) putchar('\n');
		}
		sorted.clear();
	}
	if (cnt % 7) putchar('\n');

	puts("\nSystem procedures");
	puts("-----------------");
	for(i=cnt=0;i < NUM_SPROCS;++i)
	{
		str = sysprocs[i].first;
		if (!pattern || wildMatch(str,pattern,false))
		{
			if (com == COM_HELP)
			{
				printf("   %-8s",str);
				if (!((++cnt) % 7)) putchar('\n');
			}
			else sorted.insert(str);
		}
	}
	if (com == COM_SHELP)
	{
		cnt = 0;
		for(auto &s: sorted)
		{
			printf("   %-8s",s.c_str());
			if (!((++cnt) % 7)) putchar('\n');
		}
		sorted.clear();
	}

	if (cnt % 7) putchar('\n');
	putchar('\n');

	return tokpos;
}




size_t comHist(st_line *line, size_t tokpos)
{
	int cnt;

	if (!line->isExprEnd(++tokpos))
	{
		// Get number of lines to display
		t_result result = line->evalExpression(tokpos);
		st_value &val = result.first;
		if (val.type != TYPE_NUM || val.num < 1)
			throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
		cnt = val.num;
		tokpos = result.second;
	}
	else
	{
		cnt = 0;
		++tokpos;
	}

	io.printHistory(cnt);
	return tokpos;
}




size_t comChist(st_line *line, size_t tokpos)
{
	io.clearHistory();
	return tokpos + 1;
}




size_t comTrace(st_line *line, size_t tokpos)
{
	switch(line->tokens[tokpos].subtype)
	{
	case COM_TRON:
		tracing_mode = TRACING_NOSTEP;
		puts("Tracing on.");
		break;
	case COM_TRONS:
		tracing_mode = TRACING_STEP;
		puts("Tracing on with stepping.");
		break;
	case COM_TROFF:
		tracing_mode = TRACING_OFF;
		puts("Tracing off.");
		break;
	default:
		assert(0);
	}
	return tokpos + 1;
}




/*** Watch variables. Format is WATCH [:a :b ...] ***/
size_t comWatch(st_line *line, size_t tokpos)
{
	// If no argument given list vars being watched
	if (line->isExprEnd(++tokpos))
	{
		if (watch_vars.size())
		{
			bool comma = false;

			printf("Watched variables:");
			for(const string &name: watch_vars)
			{
				if (comma)
					putchar(',');
				else
					comma = true;
				printf(" %s",name.c_str());
			}
			putchar('\n');
		}
		else puts("There are no variables being watched.");
		return tokpos;
	}

	// Loop until we run out of variables. They don't have to currently
	// exist as we just store the name.
	for(;tokpos < line->tokens.size();++tokpos)
	{
		st_token &tok = line->tokens[tokpos];
		if (tok.type != TYPE_VAR) break;
		watch_vars.insert(tok.strval.substr(1,tok.strval.size()-1));
	}
	printf("Watching %ld variables.\n",watch_vars.size());
	return tokpos;
}




size_t comUnWatch(st_line *line, size_t tokpos)
{
	// If no argument clear the entire list
	if (line->isExprEnd(++tokpos))
	{
		puts("All watch variables cleared.");
		watch_vars.clear();
		return tokpos;
	}
	for(;tokpos < line->tokens.size();++tokpos)
	{
		st_token &tok = line->tokens[tokpos];
		if (tok.type != TYPE_VAR) break;

		string varname = tok.strval.substr(1,tok.strval.size()-1);
		auto it = watch_vars.find(varname);
		if (it == watch_vars.end())
			throw t_error({ ERR_UNWATCHED_VAR, varname });
		watch_vars.erase(it);
	}
	if (watch_vars.size())
		printf("Now watching %ld variable(s).\n",watch_vars.size());
	else
		puts("All watch variables cleared.");
	return tokpos;
}




/*** Sets the random seed ***/
size_t comSeed(st_line *line, size_t tokpos)
{
	if (line->isExprEnd(++tokpos)) throw t_error({ ERR_MISSING_ARG, "" });

	t_result result = line->evalExpression(tokpos);
	st_value &val = result.first;
	if (val.type != TYPE_NUM || val.num < 0)
		throw t_error({ ERR_INVALID_ARG, line->tokens[tokpos].toString() });
	srandom((int)val.num);
	return result.second;
}




/*** Sets the angle mode to degrees or radians ***/
size_t comAngleMode(st_line *line, size_t tokpos)
{
	flags.angle_in_degs = (line->tokens[tokpos].subtype == COM_DEG);
	setGlobalVarValue("$angle_mode",flags.angle_in_degs ? "DEG" : "RAD");
	return tokpos + 1;
}




/*** Clears the text terminal window ***/
size_t comCT(st_line *line, size_t tokpos)
{
	cout << "\033[2J\033[H" << flush;
	return tokpos + 1;
}

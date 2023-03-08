#include "globals.h"

#define IS_OP_TYPE(TOK,TYPE) (TOK.type == TYPE_OP && TOK.subtype == TYPE)

void addToken(int type, string &strval);
void addOpToken(char c, int opcode);

///////////////////////////////// CONSTRUCT /////////////////////////////////

/*** Default to program line ***/
st_line::st_line()
{
	type = LINE_PROG;
	linenum = 0;
	parent_proc = NULL;
}




/*** Unlikely is_list will ever be false ***/
st_line::st_line(bool is_list)
{
	type = is_list ? LINE_LIST : LINE_PROG;
	linenum = 0;
	parent_proc = NULL;
}




st_line::st_line(st_line *rhs)
{
	type = rhs->type;
	linenum = rhs->linenum;
	tokens = rhs->tokens;
	labels = rhs->labels;
	parent_proc = rhs->parent_proc;
}




st_line::st_line(shared_ptr<st_line> &rhs)
{
	assert(rhs->type == LINE_LIST);
	type = LINE_LIST;
	linenum = rhs->linenum;
	tokens = rhs->tokens;
	labels = rhs->labels;
	parent_proc = rhs->parent_proc;
}




/*** Construct a list line. A list is a line object owned by a token that
     in turn is owned by another line. ie:
     st_line -> st_token -> st_line -> [st_token * N]
     ^                      ^           ^
     Top level line         List line   List elements (may include sublists)
***/
st_line::st_line(st_line *parent, size_t from)
{
	int cnt = 1;

	type = LINE_LIST;
	linenum = 0;

	// Insert tokens from parent up until matching ]
	for(size_t pos=from;pos < parent->tokens.size();++pos)
	{
		st_token &tok = parent->tokens[pos];
		if (tok.type == TYPE_OP)
		{
			if (tok.subtype == OP_L_SQR_BRACKET) ++cnt;
			else if (tok.subtype == OP_R_SQR_BRACKET)
			{
				if (!--cnt) break;
			}
		}
		tokens.emplace_back(tok);
	}
	assert(!cnt);
	parent_proc = parent->parent_proc;

	// Can't just copy everything as positions are different. Easier to
	// just redo everything
	createSubLists();
	matchBrackets();
	setLabels();
}




// Used in st_user_proc::addReplaceDeleteLine() and st_user_proc::addLine() 
st_line::st_line(
	st_user_proc *proc,
	int _linenum, st_line *parent, size_t from, size_t to)
{
	type = LINE_PROG;
	linenum = _linenum;
	parent_proc = proc;

	for(size_t pos=from;pos <= to;++pos)
		tokens.emplace_back(parent->tokens[pos]);
	createSubLists();
	matchBrackets();
	setLabels();
}


/////////////////////////////// INPUT PARSING /////////////////////////////////

/*** We have a string in rdline so parse it and exec if appropriate ***/
void st_line::parseAndExec(string &rdline)
{
	clear();
	flags.do_break = false;

	if (tokenise(rdline))
	{
		setLabels();

		switch(logo_state)
		{
		case STATE_CMD:
			execute();
			break;
		case STATE_DEF_PROC:
			// Add this to the user proc being defined
			assert(def_proc);
			def_proc->addLine(this);
			break;
		case STATE_IGN_PROC:
			execute();
			break;
		default:
			assert(0);
		}
	}
}




/*** Returns true if the line was non empty ***/
bool st_line::tokenise(string &rdline)
{
	string strval;
	bool in_quotes;
	bool escaped;
	int round_cnt;
	int square_cnt;

	if (rdline == "") return false;

	tokens.clear();
	round_cnt = 0;
	square_cnt = 0;
	in_quotes = false;
	escaped = false;

	// Go through the input line character by character and create tokens
	for(char c: rdline)
	{
		// Escape is partially processed in st_io::readInput() in order
		// to catch lines continued over a newline
		if (escaped)
		{
			if (strchr("bnrt\"",c))
			{
				strval.pop_back();
				switch(c)
				{
				case 'b':
					c = '\b';
					break;
				case 'n':
					c = '\n';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case '"':
					c = '"';
				}
			}
			escaped = false;
		}
		switch(c)
		{
		case '\\':
			escaped = true;
			break;
		case '"':
			if (in_quotes)
			{
				addToken(TYPE_STR,strval);
				in_quotes = false;
				strval = "";
			}
			else
			{
				if (strval != "")
				{
					addToken(TYPE_UNDEF,strval);
					strval = "";
				}
				in_quotes = true;
			}
			continue;
		case ';':
			/* Alternative to REM. We don't just exit the loop 
			   and bin the rest because we want everything 
			   following to appear in a procedure listing and be
			   saved */
			if (!escaped && !in_quotes)
			{
				if (strval != "")
				{
					addToken(TYPE_UNDEF,strval);
					strval = "";
				}
				string s(";");
				addToken(TYPE_UNDEF,s);
			}
			continue;
		case '$':
		case ':':
			// Can only have at the start of a variable or in a 
			// string
			if (!in_quotes && strval.size())
			{
				strval += c;
				throw t_error({ ERR_INVALID_CHAR, strval });
			}
		}
		if (in_quotes)
		{
			strval += c;
			continue;
		}

		// Find single char ops
		auto mit = single_char_ops.find(c);
		if (mit != single_char_ops.end())
		{
			switch(c)
			{
			case '[':
				++square_cnt;
				break;
			case ']':
				if (--square_cnt < 0)
					throw t_error({ ERR_UNEXPECTED_BRACKET, "]" });
				break;
			case '(':
				++round_cnt;
				break;
			case ')':
				if (--round_cnt < 0)
					throw t_error({ ERR_UNEXPECTED_BRACKET, ")" });
				break;
			}
			// Add current token if already set
			if (strval != "")
			{
				addToken(TYPE_UNDEF,strval);
				strval = "";
			}

			// Add the operator
			addOpToken(c,mit->second);
		}
		else if (isspace(c))
		{
			if (strval != "")
			{
				addToken(TYPE_UNDEF,strval);
				strval = "";
			}
		}
		else strval += c;
	}
	if (square_cnt || round_cnt)
		throw t_error({ ERR_MISSING_BRACKET, string(1,rdline.back()) });
	if (in_quotes)
		throw t_error({ ERR_MISSING_QUOTES, string(1,rdline.back()) });

	if (strval != "") addToken(TYPE_UNDEF,strval);

	if (tokens.empty()) return false;

	setUndefinedTokens();
	negateTokens();
	createSubLists();
	matchBrackets();
	return true;
}




void st_line::addToken(int type, string &strval)
{
	// Can't use make_shared because using non default constructor
	tokens.emplace_back(st_token(type,strval));
}




void st_line::addOpToken(char c, int opcode)
{
	if (tokens.size())
	{
		// Check for multi char operators
		st_token &prev = tokens.back();
		if (prev.type == TYPE_OP) 
		{
			if (opcode == OP_EQUALS)
			{
				if (prev.subtype == OP_LESS)
				{
					prev.changeOpType(OP_LESS_EQUALS);
					return;
				}
				if (prev.subtype == OP_GREATER)
				{
					prev.changeOpType(OP_GREATER_EQUALS);
					return;
				}
			}
			if (prev.subtype == OP_LESS && opcode == OP_GREATER)
			{
				prev.changeOpType(OP_NOT_EQUALS);
				return;
			}
		}
	}
	// Add single char op
	tokens.emplace_back(st_token(c,opcode));
}




/*** Set the token type if not already set ***/
void st_line::setUndefinedTokens()
{
	vector<pair<const char *,en_op>> word_ops =
	{
		{ "NOT", OP_NOT },
		{ "AND", OP_AND },
		{ "OR",  OP_OR },
		{ "XOR", OP_XOR }
	};
	const char *tokstr;
	bool found;
	int i;

	// Look for undefined types and set them appropriately
	found = false;
	for(auto &tok: tokens)
	{
		if (tok.type != TYPE_UNDEF) continue;

		// ':' is normal var, '$' is read only system var
		if (tok.strval[0] == ':' || tok.strval[0] == '$')
		{
			if (tok.strval.size() == 1)
				throw t_error({ ERR_SYNTAX, tok.toString() });
			tok.changeType(TYPE_VAR);
			continue;
		}
		found = false;
		tokstr = tok.strval.c_str();

		// See if its a number
		if (isNumber(tok.strval))
		{
			tok.setNumber(atof(tokstr));
			continue;
		}

		// See if its a multi character operator
		for(auto &[opname,op]: word_ops)
		{
			if (!strcasecmp(opname,tokstr))
			{
				tok.changeType(TYPE_OP,op);
				found = true;
				break;
			}
		}
		if (found) continue;

		// See if its a command
		for(i=0;i < NUM_COMS;++i)
		{
			if (!strcasecmp(commands[i].first,tokstr))
			{
				tok.changeType(TYPE_COM,i);
				found = true;
				break;
			}
		}
		if (found) continue;

		// See if its a system proc
		for(i=0;i < NUM_SPROCS;++i)
		{
			if (!strcasecmp(sysprocs[i].first,tokstr))
			{
				tok.changeType(TYPE_SPROC,i);
				found = true;
				break;
			}
		}
		// Set to user procedure as last resort
		if (!found) tok.changeType(TYPE_UPROC);
	}
}




/*** Look for any '-'. If a number, var or proc follows and its either the 
     first token or an operator preceeds it then next token set to negative 
     and delete current one ***/
void st_line::negateTokens()
{
	for(size_t pos=0;pos < tokens.size()-1;)
	{
		st_token *prev = pos ? &tokens[pos-1] : NULL;
		st_token &curr = tokens[pos];
		st_token &next = tokens[pos+1];

		if (IS_OP_TYPE(curr,OP_SUB) &&
		    (!pos || 
		     (prev->type == TYPE_OP && 
		      prev->subtype != OP_R_RND_BRACKET) ||
		     prev->type == TYPE_COM ||
		     prev->type == TYPE_SPROC ||
		     prev->type == TYPE_STR ||
		     prev->type == TYPE_LIST) &&

		    (IS_OP_TYPE(next,OP_L_RND_BRACKET) ||
		     next.type == TYPE_NUM || 
		     next.type == TYPE_VAR ||
		     next.type == TYPE_SPROC ||
		     next.type == TYPE_UPROC))
		{
			next.setNegative();	
			tokens.erase(tokens.begin() + pos);
		}
		else ++pos;
	}

}




/*** Create sub list lines ***/
void st_line::createSubLists()
{
	int cnt;
	size_t pos=0;
	for(auto it=tokens.begin();it != tokens.end();++pos)
	{
		st_token &tok = *it;
		if (tok.type != TYPE_OP || tok.subtype != OP_L_SQR_BRACKET)
		{
			++it;
			continue;
		}
		tok.setList(this,pos+1);

		// Skip from [ to matching ]
		cnt = 1;
		for(auto it2=it+1;it2 != tokens.end();++it2)
		{
			st_token &tok2 = *it2;
			if (tok2.type != TYPE_OP) continue;

			if (tok2.subtype == OP_L_SQR_BRACKET) ++cnt;
			else if (tok2.subtype == OP_R_SQR_BRACKET)
			{
				if (!--cnt) 
				{
					// Delete the tokens we no longer need
					it = tokens.erase(it+1,it2+1);
					break;
				}
			}
		}
		assert(!cnt);
	}
}




/*** Set matching bracket positions ***/
void st_line::matchBrackets()
{
	stack<size_t> bpos;
	for(size_t pos=0;pos < tokens.size();++pos)
	{
		st_token &tok = tokens[pos];
		if (tok.type == TYPE_OP)
		{
			if (tok.subtype == OP_L_RND_BRACKET)
				bpos.push(pos);
			else if (tok.subtype == OP_R_RND_BRACKET)
			{
				assert(bpos.size());
				tok.match_pos = bpos.top();
				tokens[bpos.top()].match_pos = pos;
				bpos.pop();
			}
		}
	}
	assert(!bpos.size());
}




/*** Find labels and add them ***/
void st_line::setLabels()
{
	for(size_t pos=0;pos < tokens.size();)
	{
		st_token &tok = tokens[pos];
		if (tok.type == TYPE_COM)
		{
			switch(tok.subtype)
			{
			case COM_LABEL:
				if (isExprEnd(++pos))
					throw t_error({ ERR_MISSING_ARG, tok.toString() });
				addLabel(pos);
				break;
			case COM_DLABEL:
				// DLABEL is the default label that takes
				// no argument
				addDefaultLabel(pos);
				++pos;
				break;
			default:
				++pos;
			}
		}
		else ++pos;
	}
}


////////////////////////////////// EXECUTE /////////////////////////////////

/*** Return value only used in st_user_proc::addLine() ***/
size_t st_line::execute(size_t from)
{
	if (++nest_depth > MAX_NEST_DEPTH)
		throw t_error({ ERR_MAX_NEST_DEPTH, "" });

	size_t pos;

	for(pos=from;pos < tokens.size();)
	{
		if (flags.do_break)
		{
			--nest_depth;	
			throw t_interrupt({ INT_BREAK, "" });
		}

		st_token &tok = tokens[pos];
		try
		{
			switch(tok.type)
			{
			case TYPE_UNDEF:
				assert(0);
			case TYPE_COM:
				if (logo_state == STATE_DEF_PROC ||
				    logo_state == STATE_IGN_PROC)
				{
					// Run command if TO to get error as
					// can have embedded proc definitions
					if (tok.subtype == COM_END || 
					    tok.subtype == COM_TO)
					{
						return commands[tok.subtype].second(this,pos);
					}
					++pos;
					continue;
				}
				if (tracing_mode)
					printTrace('C',commands[tok.subtype].first);
				pos = commands[tok.subtype].second(this,pos);
				break;
			default:
				if (logo_state == STATE_DEF_PROC ||
				    logo_state == STATE_IGN_PROC) ++pos;
				else
				{
					if (tracing_mode) printTrace('C',"PR");
					pos = commands[COM_PR].second(this,pos);
				}
			}
		}
		catch(t_error &err)
		{
			--nest_depth;
			if (err.second == "")
			{
				if (tok.type == TYPE_LIST)
					err.second = tok.listline->listToString();
				else
					err.second = tok.strval;
			}
			throw;
		}
		catch(t_interrupt &inter)
		{
			--nest_depth;
			if (inter.first == INT_GOTO)
			{
				size_t index = labelIndex(inter.second.c_str());
				// Catch in st_user_proc::execute() 
				if ((int)index == -1) throw;
				pos = index;
			}
			else throw;
		}
	}
	--nest_depth;
	return pos;
}




t_result st_line::evalExpression(size_t tokpos)
{
	stack<st_value> valstack;
	stack<int> opstack;
	t_result evalret;
	bool expect_val;
	size_t pos;
	int invert;

	expect_val = true;
	invert = 0;

	for(pos=tokpos;pos < tokens.size();++pos)
	{
		st_token &tok = tokens[pos];

		switch(tok.type)
		{
		case TYPE_NUM:
		case TYPE_STR:
		case TYPE_VAR:
		case TYPE_LIST:
			if (!expect_val) goto DONE;
			valstack.push(tok.getValue(invert));
			invert = 0;
			expect_val = false;
			break;

		case TYPE_COM:
			// A command means we've reached the end of the 
			// expression as they don't return values.
			goto DONE;

		case TYPE_SPROC:
			if (!expect_val) goto DONE;

			switch(tok.subtype)
			{
			case SPROC_TF:
			case SPROC_DIR:
			case SPROC_GETDIR:
			case SPROC_GETSECS:
				// These procs don't take an argument
				break;
			default:
				if (pos+1 == tokens.size())
					throw t_error({ ERR_MISSING_ARG, tok.toString() });
			}
			if (tracing_mode)
				printTrace('S',sysprocs[tok.subtype].first);

			{
			// Pass proc name token pos so can have 1 function
			// for multiple procedures
			t_result result = sysprocs[tok.subtype].second(this,pos);
			if (tok.neg) result.first.negate();
			result.first.invert(invert);
			valstack.push(result.first);
			invert = 0;
			expect_val = false;
			pos = result.second - 1;
			}
			break;

		case TYPE_UPROC:
			if (!expect_val) goto DONE;
			if (tracing_mode) printTrace('U',tok.strval.c_str());
			{
			t_result result = execUserProc(pos);
			if (tok.neg) result.first.negate();
			result.first.invert(invert);
			valstack.push(result.first);
			invert = 0;
			expect_val = false;
			pos = result.second - 1;
			}
			break;
			
		case TYPE_OP:
			switch(tok.subtype)
			{
			case OP_NOT:
				// NOT is a special case op in that it only 
				// works on one operand.
				if (pos != tokpos && !expect_val)
					throw t_error({ ERR_UNEXPECTED_OP, "NOT" });
				++invert;
				expect_val = true;
				break;

			case OP_L_RND_BRACKET:
				if (!expect_val) goto DONE;
				evalret = evalExpression(pos+1);
				if (tok.neg) evalret.first.negate();
				evalret.first.invert(invert);
				valstack.push(evalret.first);
				pos = tok.match_pos;
				expect_val = false;
				invert = 0;
				break;

			case OP_R_RND_BRACKET:
				if (expect_val)
					throw t_error({ ERR_SYNTAX, tok.toString() });
				evalStack(valstack,opstack);
				goto DONE;

			case OP_L_SQR_BRACKET:
			case OP_R_SQR_BRACKET:
				// Should never get here since all sublists
				// should have been created and [] ops removed
				assert(0);

			case OP_AND:
				// Do lazy evaluation - ie if we have a null
				// value on the LHS don't bother with the RHS
				if (expect_val)
					throw t_error({ ERR_SYNTAX, tok.toString() });
				evalStack(valstack,opstack);
				if (valstack.top().isSet()) expect_val = true;
				else
				{
					pos = skipRHSofAND(pos);
					expect_val = false;
				}
				break;

			default:
				if (expect_val)
					throw t_error({ ERR_SYNTAX, tok.toString() });
				if (opstack.size() && 
				    op_prec[tok.subtype] <= op_prec[opstack.top()])
				{
					evalStack(valstack,opstack);
				}
				opstack.push(tok.subtype);
				expect_val = true;
			}
			break;

		default:
			assert(0);
		}
	}
	DONE:
	if (invert)
		throw t_error({ ERR_UNEXPECTED_OP, tokens[tokens.size()-1].strval });
	if (valstack.size())
	{
		try
		{
			evalStack(valstack,opstack);
		}
		catch(t_error &err)
		{
			if (err.second == "")
				err.second = tokens.back().strval;
			throw;
		}
	}
	else throw t_error({ ERR_SYNTAX, tokens[tokpos].strval });

	return { valstack.top(), pos };
}




t_result st_line::execUserProc(size_t tokpos)
{
	string &name = tokens[tokpos].strval;
	st_user_proc_inst *new_inst;
	st_user_proc_inst *prev_inst;
	t_result result;

	// See if proc defined
	auto mit = user_procs.find(name);
	if (mit == user_procs.end())
		throw t_error({ ERR_UNDEFINED_UPROC, name });
	prev_inst = curr_proc_inst;

	// Don't set curr_proc_inst immediately or local variable lookups for 
	// current proc (if set) will fail 
	new_inst = new st_user_proc_inst(mit->second.get());

	// Set parameters
	try
	{
		tokpos = new_inst->setParams(this,tokpos+1);
	}
	catch(t_error &err)
	{
		delete new_inst;
		throw;
	}
	curr_proc_inst = new_inst;

	// Execute
	try
	{
		curr_proc_inst->execute();
	}
	catch(t_error)
	{
		if (!stop_proc) stop_proc = curr_proc_inst->proc;
		delete curr_proc_inst;
		curr_proc_inst = prev_inst;
		throw;
	}
	catch(t_interrupt &inter)
	{
		if (inter.first == INT_RETURN)
		{
			assert(curr_proc_inst);
			if (curr_proc_inst->ret_set)
			{
				result = curr_proc_inst->retval;
				result.second = tokpos;
				delete curr_proc_inst;
				curr_proc_inst = prev_inst;
				return result;
			}
			// No result set, just return with empty result
		}
		else
		{
			if (!stop_proc) stop_proc = curr_proc_inst->proc;
			delete curr_proc_inst;
			curr_proc_inst = prev_inst;
			throw;
		}
	}

	delete curr_proc_inst;
	curr_proc_inst = prev_inst;

	// result.first left unset
	result.second = tokpos;
	return result;
}




void st_line::evalStack(stack<st_value> &valstack, stack<int> &opstack)
{
	st_value rval;

	// If we have no operators make sure we have at least 1 value on the
	// stack. Input could just be a value or NOT <value>
	if (!opstack.size()) 
	{
		assert(valstack.size());
		return;
	}

	// Loop through the stacks 
	do
	{
		rval = valstack.top();
		valstack.pop();

		// Must have a LHS value
		if (!valstack.size())
			throw t_error({ ERR_MISSING_ARG, "" });

		switch(opstack.top())
		{
		case OP_AND:
			valstack.top() = (valstack.top().isSet() && rval.isSet());
			break;
		case OP_OR:
			valstack.top() = (valstack.top().isSet() || rval.isSet());
			break;
		case OP_XOR:
			valstack.top() = (valstack.top().isSet() ^ rval.isSet());
			break;
		case OP_EQUALS:
			valstack.top() = (valstack.top() == rval);
			break;
		case OP_NOT_EQUALS:
			valstack.top() = (valstack.top() != rval);
			break;
		case OP_LESS:
			valstack.top() = (valstack.top() < rval);
			break;
		case OP_GREATER:
			valstack.top() = (valstack.top() > rval);
			break;
		case OP_LESS_EQUALS:
			valstack.top() = (valstack.top() <= rval);
			break;
		case OP_GREATER_EQUALS:
			valstack.top() = (valstack.top() >= rval);
			break;
		case OP_ADD:
			valstack.top() += rval;
			break;
		case OP_SUB:
			valstack.top() -= rval;
			break;
		case OP_MUL:
			valstack.top() *= rval;
			break;
		case OP_DIV:
			valstack.top() /= rval;
			break;
		case OP_MOD:
			valstack.top() %= rval;
			break;
		case OP_PWR:
			valstack.top() ^= rval;
			break;
		default:
			assert(0);
		}
		opstack.pop();
	} while(opstack.size());
}




/*** Find the next valid operator after AND or the end of the expression ***/
size_t st_line::skipRHSofAND(size_t pos)
{
	bool expect_val = true;
	int rb = 0;

	st_token &start_tok = tokens[pos];
	if (start_tok.lazy_jump_pos) return start_tok.lazy_jump_pos;

	for(++pos;pos < tokens.size();++pos)
	{
		st_token &tok = tokens[pos];
		
		if (tok.type == TYPE_OP)
		{
			// Don't check expect_val here because can have 2 or
			// more ops in a row , eg: + ((
			switch(tok.subtype)
			{
			case OP_R_RND_BRACKET:
				--rb;
				break;
			case OP_L_RND_BRACKET:
				++rb;
				break;
			case OP_OR:
			case OP_XOR:
				if (rb < 1) goto DONE;
				break;
			}
			expect_val = true;
		}
		else
		{
			/* If we've got a value but are not expecting one then
			   its the end of the AND clause (or a syntax error but
			   we still need to return regardless) */
			if (rb < 1 && !expect_val) break;
			expect_val = false;
		}
	}

	DONE:
	start_tok.lazy_jump_pos = pos - 1;
	return start_tok.lazy_jump_pos;
}


/////////////////////////////////// LIST ///////////////////////////////////


/////////// OPERATORS ///////////

bool st_line::operator==(st_line &rhs)
{
	assert(type == LINE_LIST && rhs.type == LINE_LIST);

	if (tokens.size() != rhs.tokens.size()) return false;
	size_t pos;

	for(pos=0;pos < tokens.size();++pos)
	{
		st_value val = rhs.tokens[pos].getValue();
		if (tokens[pos].getValue() != val) return false;
	}
	return true;
}




bool st_line::operator!=(st_line &rhs)
{
	assert(type == LINE_LIST);
	return !operator==(rhs);
}




void st_line::operator+=(st_line &rhs)
{
	assert(type == LINE_LIST && rhs.type == LINE_LIST);

	size_t add = tokens.size();
	for(st_token &tok: rhs.tokens)
	{
		tokens.emplace_back(tok);

		// Adjust bracket match positions
		if (tokens.back().type == TYPE_OP && 
		    tokens.back().match_pos)
		{
			tokens.back().match_pos += add;
		}
	}
}




void st_line::operator*=(int cnt)
{
	assert(type == LINE_LIST);

	if (cnt < 1)	
	{
		tokens.clear();
		return;
	}

	size_t size = tokens.size();
	for(int i=1;i < cnt;++i)
	{
		for(size_t j=0;j < size;++j)
		{
			st_token &tok = tokens[j];

			// Adjust bracket match positions
			if (tok.type == TYPE_OP && tok.match_pos)
				tok.match_pos += size * i;

			tokens.emplace_back(tok);
		}
	}
}




/*** Evaluate expressions inside the list. Because Logo doesn't use element
     delimiters expressions must be bounded by () if there is any abiguity.
     Eg: eval [2 -3] -> [-1] but eval [(2) -3] -> [2 -3] ***/
shared_ptr<st_line> st_line::evalList()
{
	assert(type == LINE_LIST);

	/* This avoids endless recursion with something like:
	      make "abc" [:abc]
	      eval :abc */
	if (++nest_depth > MAX_NEST_DEPTH)
		throw t_error({ ERR_MAX_NEST_DEPTH, "" });

	shared_ptr<st_line> line = make_shared<st_line>(true);
	t_result result;

	try
	{
		// Go through our tokens, evaluate any vars and create copies 
		// for the new line
		for(size_t pos=0;pos < tokens.size();)
		{
			st_token &tok = tokens[pos];
	
			switch(tok.type)
			{
			case TYPE_UNDEF:
				assert(0);
			case TYPE_COM:
				line->tokens.emplace_back(st_token(tok));
				++pos;
				break;
			case TYPE_LIST:
				line->tokens.emplace_back(st_token(tok.listline->evalList()));
				++pos;
				break;
			default:
				result = evalExpression(pos);
				line->tokens.emplace_back(st_token(result.first));
				pos = result.second;
			}
		}
	}
	catch(...)
	{
		--nest_depth;
		throw;
	}
	--nest_depth;
	return line;
}




void st_line::listShuffle()
{
	assert(type == LINE_LIST);
	mt19937_64 ran(time(0));
	shuffle(tokens.begin(),tokens.end(),ran);
}

//////////////////////////////////// GETTERS //////////////////////////////////

/*** Get the particular element from the list. Indexing starts at 1 ***/
st_value st_line::getListElement(int index)
{
	assert(type == LINE_LIST);

	st_value val(0.0);
	if (index && index <= (int)tokens.size()) val.set(tokens[index-1]);
	return val;
}




/*** Get a subsection of a list ***/
shared_ptr<st_line> st_line::getListPiece(size_t from, size_t to)
{
	// Cast away unsignedness
	assert(type == LINE_LIST && (long)from > 0 && (long)to >= (long)from);

	shared_ptr<st_line> line = make_shared<st_line>(true);
	if (from <= tokens.size())
	{
		if (to > tokens.size()) to = tokens.size();
		line->tokens.insert(
			line->tokens.begin(),
			tokens.begin()+from-1,tokens.begin()+to);
	}
	return line;
}


////////////////////////////////// SETTERS ////////////////////////////////////

/*** Create a copy of this->listline except with val prepended ***/
shared_ptr<st_line> st_line::setListFirst(st_value &val)
{
	assert(type == LINE_LIST);

	shared_ptr<st_line> line = make_shared<st_line>(true);
	line->tokens.emplace_back(st_token(val));
	line->tokens.insert(line->tokens.end(),tokens.begin(),tokens.end());

	return line;
}




/*** Create a copy of this->listline except with val appended ***/
shared_ptr<st_line> st_line::setListLast(st_value &val)
{
	assert(type == LINE_LIST);

	shared_ptr<st_line> line = make_shared<st_line>(true);
	line->tokens.insert(line->tokens.end(),tokens.begin(),tokens.end());
	line->tokens.emplace_back(st_token(val));

	return line;
}




/*** Also sets the line number of embedded lines for tracing purposes (TRON)
     Otherwise they'd just report 0 ***/
void st_line::setLineNum(int _linenum)
{
	linenum = _linenum;
	for(st_token &tok: tokens)
		if (tok.type == TYPE_LIST) tok.listline->setLineNum(_linenum);
	
}

//////////////////////////////////// OTHER ////////////////////////////////////

/*** Return position of the value is in the list else zero ***/
size_t st_line::listMemberp(st_value &val)
{
	assert(type == LINE_LIST);
	
	size_t pos = 0;
	for(st_token &tok: tokens)
	{
		++pos;
		if (tok.type != val.type) continue;
		switch(val.type)
		{
		case TYPE_NUM:
			if (tok.numval == val.num) return pos;
			break;
		case TYPE_STR:
			if (tok.strval == val.str) return pos;
			break;
		case TYPE_LIST:
			if (*tok.listline == *val.listline) return pos;
			break;
		default:
			assert(0);
		}
	}
	return 0;
}




/*** Convert a list into its string representation ***/
string st_line::listToString()
{
	assert(type == LINE_LIST);
	return string("[") + toString() + "]";
}


/////////////////////////////////// LABELS /////////////////////////////////

/*** tokpos will be pointing at the label name ***/
void st_line::addLabel(size_t tokpos)
{
	assert(tokpos < tokens.size());

	st_token &tok = tokens[tokpos];
	string &name = tok.strval;
	size_t index = labelIndex(name.c_str());

	if ((int)index == -1)
	{
		// Label is a string (so we can have numeric labels) but can't
		// be an expression
		if (name == "" || tok.type != TYPE_STR)
			throw t_error({ ERR_INVALID_ARG, name });

		// See if its a dup
		if (parent_proc && (int)parent_proc->labelLineIndex(name.c_str()) != -1)
			throw t_error({ ERR_DUP_DECLARATION, name });
		labels[name] = tokpos - 1;
	}
	else if (index == tokpos - 1) return;  // Already created
	else throw t_error({ ERR_DUP_DECLARATION, name });
}




/*** Doesn't have a name, is the default label if no matching found ***/
void st_line::addDefaultLabel(size_t tokpos)
{
	size_t index = labelIndex("");

	// Has an empty string as its name
	if ((int)index == -1) labels[""] = tokpos;
	else if (index == tokpos) return;
	else throw t_error({ ERR_DUP_DECLARATION, tokens[tokpos].strval });
}




/*** Find the label of the given name. If not found look for DLABEL ***/
size_t st_line::labelIndex(const char *name)
{
	auto mit = labels.find(name);
	if (mit == labels.end()) mit = labels.find("");
	return (mit == labels.end() ? -1 : mit->second);
}


/////////////////////////////////// MISC ///////////////////////////////////

bool st_line::isExprEnd(size_t tokpos)
{
	return (tokpos >= tokens.size() || tokens[tokpos].type == TYPE_COM);
}




void st_line::clear()
{
	tokens.clear();
}




void st_line::dump(FILE *fp, int &indent, bool show_linenums)
{
	if (show_linenums && linenum)
		fprintf(fp,"%-3d\t",linenum);
	else
		fputs("\t",fp);

	// Indentation is a bit flakey as we can't match LABEL with GO since GO
	// can take an expression which is why indentating is not the default
	if (flags.indent_label_blocks)
	{
		indent += getIndentCount(COM_GO);
		for(int i=0;i < indent;++i) fputc('\t',fp);
		indent += getIndentCount(COM_LABEL);
		indent += getIndentCount(COM_DLABEL);
	}
	if (fputs(toString().c_str(),fp) == -1)
		throw t_error({ ERR_WRITE_FAIL, "" });	
}




int st_line::getIndentCount(int com)
{
	int indent = 0;
	int add = (com == COM_GO ? -1 : 1);

	for(auto &tok: tokens)
	{
		if (tok.type == TYPE_COM)
		{
			// Don't indent if we have more than one command on
			// the same line
			if (tok.subtype != com) return 0;
			indent += add;
		}
		else if (tok.type == TYPE_LIST)
			indent += tok.listline->getIndentCount(com);
	}
	return indent;
}




string st_line::toString()
{
	string outstr;

	for(st_token &tok: tokens)
	{
		if (tok.neg) outstr += "-";

		switch(tok.type)
		{
		case TYPE_STR:
			outstr += "\"" + tok.strval + "\" ";
			break;
		case TYPE_OP:
			switch(tok.subtype)
			{
			case OP_L_SQR_BRACKET:
			case OP_L_RND_BRACKET:
				outstr += tok.strval;
				break;
			case OP_R_SQR_BRACKET:
			case OP_R_RND_BRACKET:
				if (outstr.back() == ' ') outstr.pop_back();
				outstr += tok.strval + " ";
				break;
			default:
				outstr += tok.strval + " ";
			}
			break;
		case TYPE_LIST:
			// tok.strval not set to list string to save memory. We 
			// only get the string when we need it.
			outstr += tok.listline->listToString() + " ";
			break;
		case TYPE_COM:
			// Want uppercase so not using tok.strval
			outstr = outstr + commands[tok.subtype].first + " ";
			break;
		case TYPE_SPROC:
			// Want uppercase
			outstr = outstr + sysprocs[tok.subtype].first + " ";
			break;
		default:
			// List lines will be output with their string vals
			outstr += tok.strval + " ";
		}
	}
	// Remove the trailing space
	if (outstr.back() == ' ') outstr.pop_back();
	return outstr;
}




string st_line::toSimpleString()
{
	string str;
	bool space = false;

	for(st_token &tok: tokens)
	{
		if (space) str += " ";
		if (tok.type == TYPE_LIST)
			str += tok.listline->toSimpleString();
		else
			str += tok.strval;
		space = true;
	}
	return str;
}




void st_line::printTrace(char call_type, const char *name)
{
	static st_io tio;

	printf("[%s,%d,%c,%s]",
		curr_proc_inst ? curr_proc_inst->proc->name.c_str() : "-",
		linenum,call_type,name);
	if (tracing_mode == TRACING_STEP)
	{
		printf(": ");
		fflush(stdout);

		// Could just use getchar() but then signals won't be 
		// processed properly so use io class.
		int ret;
		tio.reset();
		tio.readInput(STDIN,false,true,true,ret);		
	}
	putchar('\n');
}

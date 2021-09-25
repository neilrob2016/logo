#include "globals.h"

st_token::st_token(int _type, string &_strval)
{
	init();
	type = _type;
	strval = _strval;
}




st_token::st_token(char op, int opcode)
{
	init();
	type = TYPE_OP;
	subtype = opcode;
	strval = op;
}




// Used in st_turtle::facts()
st_token::st_token(double num)
{
	setNumber(num);
}




// Used in st_line::evalList() hence list evaluation
st_token::st_token(shared_ptr<st_line> _listline)
{
	init();
	type = TYPE_LIST;
	listline = _listline->evalList();
	// List could be very long and this will waste memory, need to call 
	// listToString() direct
	strval = DEF_LIST_STR;
}




// Used in st_line::evalList() hence list evaluation
st_token::st_token(st_value val)
{
	init();
	type = val.type;

	switch(type)
	{
	case TYPE_NUM:
		setNumber(val.num);
		break;
	case TYPE_STR:
		strval = val.str;
		break;
	case TYPE_LIST:
		listline = val.listline->evalList();
		strval = val.str;
		break;
	default:
		assert(0);
	}
}




void st_token::init()
{
	type = TYPE_UNDEF;
	subtype = 0;
	neg = false;
	numval = 0;
	match_pos = 0;
	lazy_jump_pos = 0;
}




void st_token::changeType(int _type, int _subtype)
{
	init();
	type = _type;
	subtype = _subtype;
}




void st_token::changeOpType(int opcode)
{
	assert(type == TYPE_OP);
	subtype = opcode;

	switch(opcode)
	{
	case OP_NOT_EQUALS:
		strval = "<>";
		break;
	case OP_LESS_EQUALS:
		strval = "<=";
		break;
	case OP_GREATER_EQUALS:
		strval = ">=";
		break;
	default:
		assert(0);
	}
}




void st_token::setNumber(double val)
{
	init();
	type = TYPE_NUM;
	subtype = 0;
	numval = val;
	strval = numToString(numval);
}




void st_token::setList(st_line *line, size_t pos)
{
	init();
	type = TYPE_LIST;
	listline.reset(new st_line(line,pos));
	strval = DEF_LIST_STR;
}




void st_token::setNegative()
{
	switch(type)
	{
	case TYPE_NUM:
		strval = string("-") + strval;
		numval = -numval;
		break;
	case TYPE_OP:
	case TYPE_VAR:
	case TYPE_SPROC:
	case TYPE_UPROC:
		neg = true;
		break;
	default:
		assert(0);
	}
}




st_value st_token::getValue(int invert)
{
	st_value val;

	switch(type)
	{
	case TYPE_NUM:
		val.set(numval);
		break;
	case TYPE_STR:
		assert(!neg);
		val.set(strval);
		break;
	case TYPE_LIST:
		assert(!neg);
		val.set(listline);
		break;
	case TYPE_VAR:
		val = getVarValue(strval);
		if (neg) val.negate();
		break;
	default:
		// Should never get called for any other types
		assert(0);
	}
	val.invert(invert);
	return val;
}




string st_token::toString()
{
	 return (type == TYPE_LIST ? listline->listToString() : strval);
}

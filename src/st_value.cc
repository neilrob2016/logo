#include "globals.h"

///////////////////////////////// CONSTRUCTORS ///////////////////////////////

st_value::st_value()
{
	type = TYPE_UNDEF;
}




st_value::st_value(double _num)
{
	set(_num);
}




st_value::st_value(string _str)
{
	set(_str);
}




st_value::st_value(const char *_str)
{
	set(_str);
}




st_value::st_value(const st_value &rval)
{
	switch(rval.type)
	{
	case TYPE_UNDEF:
		reset();
		break;
	case TYPE_NUM:
		set(rval.num);
		break;
	case TYPE_STR:
		set(rval.str);
		break;
	case TYPE_LIST:
		set(rval.listline);
		break;
	default:
		assert(0);
	}
}



st_value::st_value(st_token &tok)
{
	switch(tok.type)
	{
	case TYPE_NUM:
		type = TYPE_NUM;
		num = tok.numval;
		break;
	case TYPE_LIST:
		type = TYPE_LIST;
		listline = tok.listline;
		break;
	default:
		// Just make everything else a string
		type = TYPE_STR;
		str = tok.strval;
		break;
	}
}




st_value::st_value(shared_ptr<st_line> _listline): listline(_listline)
{
	set(_listline);
}


/////////////////////////////////// SETTERS //////////////////////////////////

void st_value::reset()
{
	type = TYPE_UNDEF;
	num = 0;
	str.clear();
	listline.reset();
}




void st_value::set(double _num)
{
	type = TYPE_NUM;
	num = _num;
	listline.reset();

	// Set for use in error messages
	str = numToString(num);
}




void st_value::set(string _str)
{
	type = TYPE_STR;
	str = _str;
	num = 0;
	listline.reset();
}




void st_value::set(const char *_str)
{
	type = TYPE_STR;
	str = _str;
	num = 0;
	listline.reset();
}




// Using &_listline param caused all sorts of problems
void st_value::set(shared_ptr<st_line> _listline)
{
	assert(_listline->type == LINE_LIST);
	type = TYPE_LIST;
	listline = _listline;
	num = 0;
	str = DEF_LIST_STR;
}




void st_value::set(const st_value &rval)
{
	*this = rval;
}


//////////////////////////////// OPERATORS /////////////////////////////////


void st_value::operator=(st_value rval)
{
	switch(rval.type)
	{
	case TYPE_UNDEF:
		reset();
		break;
	case TYPE_NUM:
		set(rval.num);
		break;
	case TYPE_STR:
		set(rval.str);
		break;
	case TYPE_LIST:
		set(rval.listline);
		break;
	default:
		assert(0);
	}
}




void st_value::operator=(int num)
{
	set(num);
}




bool st_value::operator==(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM : return num == rval.num;
	case TYPE_STR : return str == rval.str;
	case TYPE_LIST: return *listline == *rval.listline;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




bool st_value::operator!=(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM : return num != rval.num;
	case TYPE_STR : return str != rval.str;
	case TYPE_LIST: return *listline != *rval.listline;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




bool st_value::operator<(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM : return num < rval.num;
	case TYPE_STR : return str < rval.str;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




bool st_value::operator>(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM : return num > rval.num;
	case TYPE_STR : return str > rval.str;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




bool st_value::operator<=(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM : return num <= rval.num;
	case TYPE_STR : return str <= rval.str;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




bool st_value::operator>=(st_value &rval)
{
	if (type != rval.type) throw t_error({ ERR_INVALID_ARG, rval.str });

	switch(type)
	{
	case TYPE_NUM : return num >= rval.num;
	case TYPE_STR : return str >= rval.str;
	default       : throw t_error({ ERR_INVALID_ARG, "" });
	}
	return false;
}




void st_value::operator+=(st_value &rval)
{
	if (type != rval.type)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });

	switch(type)
	{
	case TYPE_NUM:
		num += rval.num;
		break;
	case TYPE_STR:
		str += rval.str;
		break;
	case TYPE_LIST:
		// Don't use original list as it may be part of a proc line 
		// token
		set(make_shared<st_line>(listline));
		*listline += *rval.listline;
		break;
	default:
		throw t_error({ ERR_INVALID_ARG, "" });
	}
}




void st_value::operator-=(st_value &rval)
{
	if (type != TYPE_NUM) throw t_error({ ERR_INVALID_ARG, toString() });
	if (rval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });
	num -= rval.num;
}




void st_value::operator*=(st_value &rval)
{
	switch(type)
	{
	case TYPE_NUM:
		switch(rval.type)
		{
		case TYPE_STR:
			if (num < 0)
				throw t_error({ ERR_INVALID_ARG, rval.str });
			set(multiplyString(rval.str,(int)num));
			break;
		case TYPE_NUM:
			num *= rval.num;
			break;
		case TYPE_LIST:
			{
			if (num < 0)
				throw t_error({ ERR_INVALID_ARG, rval.toString() });
			int cnt = (int)num;
			// Don't use original list
			set(make_shared<st_line>(rval.listline));
			*listline *= cnt;
			}
			break;
		default:
			assert(0);
		}
		break;
	case TYPE_STR:
		if (rval.type != TYPE_NUM)
			throw t_error({ ERR_INVALID_ARG, rval.str });
		if (rval.num < 0) throw t_error({ ERR_INVALID_ARG, rval.toString() });
		set(multiplyString(str,(int)rval.num));
		break;
	case TYPE_LIST:
		{
		if (rval.type != TYPE_NUM)
			throw t_error({ ERR_INVALID_ARG, rval.str });
		if (rval.num < 0) throw t_error({ ERR_INVALID_ARG, rval.toString() });
		set(make_shared<st_line>(listline));
		*listline *= (int)rval.num;
		break;
		}
	default:
		throw t_error({ ERR_INVALID_ARG, rval.toString() });
	}
}




void st_value::operator/=(st_value &rval)
{
	if (type != TYPE_NUM) throw t_error({ ERR_INVALID_ARG, toString() });
	if (rval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });
	if (rval.num)
		num /= rval.num;
	else
		throw t_error({ ERR_DIVIDE_BY_ZERO,"/" });
}




void st_value::operator%=(st_value &rval)
{
	if (type != TYPE_NUM) throw t_error({ ERR_INVALID_ARG, toString() });
	if (rval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });
	if (!rval.num) throw t_error({ ERR_DIVIDE_BY_ZERO,"%" });

	// Keep mantissa 
	num = ((int)num % (int)rval.num) + (num - (int)num);
}




void st_value::operator^=(st_value &rval)
{
	if (type != TYPE_NUM) throw t_error({ ERR_INVALID_ARG, toString() });
	if (rval.type != TYPE_NUM)
		throw t_error({ ERR_INVALID_ARG, rval.toString() });
	num = pow(num,rval.num);
}


/////////////////////////////////// MISC /////////////////////////////////////

/*** Returns true if non empty string, list or non zero numeric ***/
bool st_value::isSet()
{
	switch(type)
	{
	case TYPE_NUM : return (num != 0);
	case TYPE_STR : return (str != "");
	case TYPE_LIST: return (listline && listline->tokens.size());
	}
	assert(0);
	return false;
}




void st_value::invert(int cnt)
{
	if (cnt)
	{
		if (type != TYPE_NUM)
			throw t_error({ ERR_CANT_INVERT, "" });
		while(cnt--) num = !num;
	}
}




void st_value::negate()
{
	if (type != TYPE_NUM)
		throw t_error({ ERR_CANT_NEGATE, "" });
	num = -num;
}




string st_value::toString()
{
	return (type == TYPE_LIST ? listline->listToString() : str);
}




string st_value::multiplyString(string _str, int cnt)
{
	if (cnt < 1) return "";
	string s = _str;
	for(int i=1;i < cnt;++i) _str += s;
	return _str;
}




string st_value::dump(bool quotes)
{
	switch(type)
	{
	case TYPE_UNDEF:
		return "";
	case TYPE_NUM:
		return numToString(num);
	case TYPE_STR:
		if (quotes) return string("\"") + str + "\"";
		return str;
	case TYPE_LIST:
		// str won't be set if this is a stack temporary updated by
		// += or *=
		return listline->listToString();
	}
	assert(0);
	return "";
}

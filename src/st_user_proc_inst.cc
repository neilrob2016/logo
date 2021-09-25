#include "globals.h"


size_t st_user_proc_inst::setParams(st_line *line, size_t tokpos)
{
	t_result result;
	size_t pnum;

	for(pnum=0;pnum < proc->params.size() && !line->isExprEnd(tokpos);++pnum)
	{
		result = line->evalExpression(tokpos);
		string &pname = proc->params[pnum];

		// Make sure we don't have a global of the same name
		if (global_vars.find(pname) != global_vars.end())
			throw t_error({ ERR_DUP_DECLARATION, pname });

		if (watch_vars.find(pname) != watch_vars.end())
			printWatch('P',pname,result.first);
		local_vars[pname] = result.first;
		tokpos = result.second;
	}
	if (pnum < proc->params.size())
		throw t_error({ ERR_MISSING_ARG, proc->name });
	return tokpos;
}




void st_user_proc_inst::setLocalVar(string &name, st_value &val)
{
	if (watch_vars.find(name) != watch_vars.end()) printWatch('L',name,val);
	local_vars[name].set(val);
}




t_var_map::iterator st_user_proc_inst::getLocalVar(const char *name)
{
	return local_vars.find(name[0] == ':' ? name + 1 : name);
}




bool st_user_proc_inst::getLocalVarValue(string &name, st_value &val)
{
	auto mit = getLocalVar(name.c_str());
	if (mit != local_vars.end())
	{
		val = mit->second;
		return true;
	}
	return false;
}




void st_user_proc_inst::execute()
{
	proc->execute();
}

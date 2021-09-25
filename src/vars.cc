#include "globals.h"
#include "build_date.h"

void setSystemVars()
{
	setGlobalVarValue("$interpreter",LOGO_INTERPRETER);
	setGlobalVarValue("$copyright",LOGO_COPYRIGHT);
	setGlobalVarValue("$version",LOGO_VERSION);
	setGlobalVarValue("$pi",3.14159265358979323846);
	setGlobalVarValue("$e",2.71828182845904523536);
	setGlobalVarValue("$degs_per_rad",DEGS_PER_RADIAN);
	setGlobalVarValue("$build_date",BUILD_DATE);
	setGlobalVarValue("$num_colours",NUM_COLOURS);
	setGlobalVarValue("$file_extension",LOGO_FILE_EXT);
	setGlobalVarValue("$angle_mode",angle_in_degs ? "DEG" : "RAD");

	// LOGO isn't a system programming language so just provide the
	// minimum of system info.
	setGlobalVarValue("$home",getenv("HOME"));
	setGlobalVarValue("$username",getenv("USER"));
	setGlobalVarValue("$pid",getpid());

	setWindowSystemVars();
}




void setWindowSystemVars()
{
	setGlobalVarValue("$win_width",win_width);
	setGlobalVarValue("$win_height",win_height);
	setGlobalVarValue("$win_colour",win_colour);
}




void clearGlobalVariables()
{
	// Easier to clear everything and recreate system vars than iterate 
	// through the map
	global_vars.clear();
	setSystemVars();
}




/*** Look for proc params & locals first then globals ***/
st_value getVarValue(string &name)
{
	string sname;
	st_value val;

	// Check local
	if (curr_proc_inst && curr_proc_inst->getLocalVarValue(name,val))
		return val;

	// Check global
	auto mit = getGlobalVar(name);
	if (mit == global_vars.end())
		throw t_error({ ERR_UNDEFINED_VAR, name });
	return mit->second;
}




t_var_map::iterator getGlobalVar(string &name)
{
	return global_vars.find(name[0] == ':' ? name.c_str() + 1 : name);
}

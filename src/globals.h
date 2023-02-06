#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <stack>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <functional>

using namespace std;

#ifdef MAINFILE
#define EXTERN
#else
#define EXTERN extern
#endif

// System
#define LOGO_INTERPRETER "NRJ-LOGO"
#define LOGO_COPYRIGHT   "Copyright (C) Neil Robertson 2020-2023"
#define LOGO_VERSION     "1.5.0"
#define LOGO_FILE_EXT    ".lg"

// Maths
#define DEGS_PER_RADIAN 57.29578
#define SGN(N)          ((N) < 0 ? -1 : ((N) > 0 ? 1 : 0))

// X window
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

// X colours
#define NUM_COLOURS       106
#define COL_GREEN         0
#define COL_LIGHT_GREEN   10
#define COL_TURQUOISE     15
#define COL_LIGHT_BLUE    20
#define COL_BLUE          25
#define COL_PURPLE        38
#define COL_MAUVE         45
#define COL_PINKY_RED     52
#define COL_RED           60
#define COL_ORANGE        68
#define COL_YELLOW        75
#define COL_MEDIUM_YELLOW 78
#define COL_KHAKI         82
#define COL_BLACK         90
#define COL_WHITE         105
#define WIN_DEFAULT_COL   COL_BLACK

// Misc
#define STDIN             0
#define MAX_NEST_DEPTH    100
#define MAX_HISTORY_LINES 100
#define DEF_LIST_STR      "[<LIST>]" // Should never see in output
#define OP_LIST_STR       ":;[]()=<>+-*/%^"


/*********************************** GLOBALS *********************************/

enum en_state
{
	STATE_CMD,
	STATE_DEF_PROC,
	STATE_IGN_PROC,
	STATE_READ_CHAR,
	STATE_READ_LINE
};

enum en_token
{
	TYPE_UNDEF,
	TYPE_STR, 
	TYPE_VAR,
	TYPE_NUM,
	TYPE_LIST,
	TYPE_OP,
	TYPE_COM,
	TYPE_SPROC,
	TYPE_UPROC,

	NUM_TYPES
};

// Update OP_LIST_STR and perhaps single_char_ops if anything added to list
enum en_op
{
	// 0
	OP_L_SQR_BRACKET,
	OP_R_SQR_BRACKET,
	OP_L_RND_BRACKET,
	OP_R_RND_BRACKET,
	OP_NOT,

	// 5 
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_EQUALS,
	OP_NOT_EQUALS,

	// 10
	OP_LESS,
	OP_GREATER,
	OP_LESS_EQUALS,
	OP_GREATER_EQUALS,
	OP_ADD,

	// 15 
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_PWR,

	NUM_OPS
};


enum en_line
{
	LINE_PROG,
	LINE_LIST
};


enum en_com
{
	// 0
	COM_SEMI,
	COM_REM2,
	COM_ED,
	COM_TO,
	COM_END,

	// 5
	COM_RESTART,
	COM_ER,
	COM_ERV,
	COM_ERP,
	COM_ERALL,

	// 10
	COM_PO,
	COM_POL,
	COM_POPS,
	COM_POPSL,
	COM_POTS,

	// 15
	COM_PONS,
	COM_POSF,
	COM_POALL,
	COM_RENUM,
	COM_PR,

	// 20
	COM_WR,
	COM_EAT,
	COM_MAKE,
	COM_MAKELOC,
	COM_INC,

	// 25
	COM_DEC,
	COM_RUN,
	COM_STOP,
	COM_WAIT,
	COM_BYE,

	// 30
	COM_REPEAT,
	COM_IF,
	COM_LABEL,
	COM_DLABEL,
	COM_GO,

	// 35
	COM_OP,
	COM_HOME,
	COM_CLEAR,
	COM_HT,
	COM_ST,

	// 40
	COM_HW,
	COM_SW,
	COM_CS,
	COM_PU,
	COM_PD,

	// 45
	COM_TG,
	COM_FD,
	COM_BK,
	COM_LT,
	COM_RT,

	// 50
	COM_SETPC,
	COM_SETBG,
	COM_SETX,
	COM_SETY,
	COM_SETH,

	// 55
	COM_SETSZ,
	COM_SETLW,
	COM_SETLS,
	COM_SETPOS,
	COM_TOWARDS,

	// 60
	COM_DOT,
	COM_WINDOW,
	COM_FENCE,
	COM_WRAP,
	COM_SETIND,

	// 65
	COM_SETFILL,
	COM_SETWINSZ,
	COM_FILL,
	COM_SAVE,
	COM_LOAD,

	// 70
	COM_CD,
	COM_HELP,
	COM_SHELP,
	COM_HIST,
	COM_CHIST,

	// 75
	COM_TRON,
	COM_TRONS,
	COM_TROFF,
	COM_WATCH,
	COM_UNWATCH,

	// 80
	COM_SEED,
	COM_DEG,
	COM_RAD,
	COM_CT,

	NUM_COMS
};


enum en_sproc
{
	// 0
	SPROC_EVAL,
	SPROC_FIRST,
	SPROC_LAST,
	SPROC_BF,
	SPROC_BL,

	// 5
	SPROC_PIECE,
	SPROC_COUNT,
	SPROC_NUMP,
	SPROC_STRP,
	SPROC_LISTP,

	// 10
	SPROC_FPUT,
	SPROC_LPUT,
	SPROC_MEMBER,
	SPROC_MEMBERP,
	SPROC_UC,

	// 15
	SPROC_LC,
	SPROC_ASCII,
	SPROC_CHAR,
	SPROC_RC,
	SPROC_RL,

	// 20
	SPROC_TF,
	SPROC_NUM,
	SPROC_STR,
	SPROC_SSTR,
	SPROC_SPLIT,

	// 25
	SPROC_RANDOM,
	SPROC_INT,
	SPROC_ROUND,
	SPROC_SIN,
	SPROC_COS,

	// 30
	SPROC_TAN,
	SPROC_ASIN,
	SPROC_ACOS,
	SPROC_ATAN,
	SPROC_LOG,

	// 35
	SPROC_LOG2,
	SPROC_LOG10,
	SPROC_SQRT,
	SPROC_ABS,
	SPROC_SGN,

	// 40
	SPROC_SHUFFLE,
	SPROC_MATCH,
	SPROC_MATCHC,
	SPROC_DIR,
	SPROC_GETDIR,

	// 45
	SPROC_GETSECS,
	SPROC_GETDATE,
	SPROC_FMT,
	SPROC_LPAD,
	SPROC_RPAD,

	// 50
	SPROC_LIST,
	SPROC_PATH,

	NUM_SPROCS
};


enum en_error
{
	// 0
	OK,
	ERR_SYNTAX,
	ERR_MISSING_QUOTES,
	ERR_MISSING_BRACKET,
	ERR_UNEXPECTED_BRACKET,

	// 5
	ERR_UNEXPECTED_OP,
	ERR_DIVIDE_BY_ZERO,
	ERR_MISSING_ARG,
	ERR_INVALID_ARG,
	ERR_INVALID_CHAR,

	// 10
	ERR_CANT_INVERT,
	ERR_CANT_NEGATE,
	ERR_CANT_CONVERT,
	ERR_UNDEFINED_VAR,
	ERR_UNWATCHED_VAR,

	// 15
	ERR_INVALID_VAR_NAME,
	ERR_INVALID_VAR_TYPE,
	ERR_READ_ONLY_VAR,
	ERR_CANT_EVAL,
	ERR_CANT_RUN,

	// 20
	ERR_OUT_OF_BOUNDS,
	ERR_VALUE_OUT_OF_RANGE,
	ERR_IF_REQ_LIST,
	ERR_IF_MISSING_BLOCK,
	ERR_DUP_DECLARATION,

	// 25
	ERR_UNEXPECTED_TO,
	ERR_UNEXPECTED_END,
	ERR_UNEXPECTED_ED,
	ERR_UNEXPECTED_ERASE,
	ERR_UNEXPECTED_ARG,

	// 30
	ERR_UNDEFINED_UPROC,
	ERR_NOT_IN_UPROC,
	ERR_MAX_NEST_DEPTH,
	ERR_UNDEFINED_LABEL,
	ERR_NO_GRAPHICS,

	// 35
	ERR_GRAPHICS_INIT_FAIL,
	ERR_INVALID_COLOUR,
	ERR_OPEN_FAIL,
	ERR_READ_FAIL,
	ERR_WRITE_FAIL,

	// 40
	ERR_STAT_FAIL,
	ERR_CD_FAIL,
	ERR_INVALID_HIST_CMD,
	ERR_TURTLE_OUT_OF_BOUNDS,
	ERR_CANT_FILL,

	// 45
	ERR_CANT_RESTART,
	ERR_INVALID_FMT,
	ERR_INVALID_PATH,
	ERR_PATH_TOO_LONG,

	NUM_ERRORS
};


enum en_interrupt
{
	INT_BREAK,
	INT_STOP,
	INT_GOTO,
	INT_RETURN,
	INT_RESTART
};


enum en_read
{
	READ_MORE,
	READ_EOL,
	READ_EOF
};


enum en_win_edge
{
	WIN_UNBOUNDED,
	WIN_FENCED,
	WIN_WRAPPED
};


enum en_tracing_mode
{
	TRACING_OFF,
	TRACING_NOSTEP,
	TRACING_STEP
};

struct st_line;
struct st_value;
struct st_token;
struct st_user_proc;
struct st_turtle_line;

// Typedefs
typedef pair<st_value,size_t> t_result;
typedef pair<en_error,string> t_error;
typedef pair<en_interrupt,string> t_interrupt; 
typedef unordered_map<string,st_value> t_var_map;
typedef vector<st_turtle_line> t_shape;

struct st_io
{
	deque<string> history;
	struct termios saved_tio;
	string rdline;
	string esc_code;
	int history_pos;
	int cursor_pos;
	bool tio_saved;
	bool insert;

	st_io();

	void reset();
	void kbRawMode();
	void kbSaneMode();

	bool readInput(
		int fd,
		bool do_echo, bool single_char, bool control_d, int &rr);
	void processEscSeq(int seq, bool do_echo);
	void clearLineAndWrite(size_t len, string write_line, bool reset_cursor);
	void deleteChar(bool do_echo);
	void addCharToReadLine(char c, bool do_echo);

	bool parseInput();
	void execText(string text);
	bool execLine();

	void addHistoryLine();
	void printHistory(int num_lines);
	void clearHistory();
};


struct st_value
{
	int type;
	double num;
	string str;
	shared_ptr<st_line> listline;

	st_value();
	st_value(double _num);
	st_value(string _str);
	st_value(const char *_str);
	st_value(const st_value &rval);
	st_value(st_token &tok);
	st_value(shared_ptr<st_line> _listline);

	void parse();
	void reset();
	void set(double _num);
	void set(string _str);
	void set(const char *_str);
	void set(shared_ptr<st_line> _listline);
	void set(const st_value &rval);
	string setListString();

	void operator=(st_value rval);
	void operator=(int num);
	bool operator==(st_value &rval);
	bool operator!=(st_value &rval);
	bool operator<(st_value &rval);
	bool operator>(st_value &rval);
	bool operator<=(st_value &rval);
	bool operator>=(st_value &rval);

	void operator+=(st_value &rval);
	void operator-=(st_value &rval);
	void operator*=(st_value &rval);
	void operator/=(st_value &rval);
	void operator%=(st_value &rval);
	void operator^=(st_value &rval);

	bool isSet();
	void invert(int cnt);
	void negate();
	string toString();
	string multiplyString(string _str, int cnt);

	string dump(bool quotes);
};



struct st_token
{
	int type;
	int subtype;
	bool neg;
	size_t match_pos;
	size_t lazy_jump_pos;

	string strval;
	double numval;
	shared_ptr<st_line> listline;

	st_token(int _type, string &_word);
	st_token(char op, int opcode);
	st_token(double num);
	st_token(shared_ptr<st_line> _listline);
	st_token(st_value val);

	void init();
	void changeType(int _type, int _subtype=0);
	void changeOpType(int opcode);
	void setNumber(double val);
	void setList(st_line *line, size_t pos);
	void setNegative();
	st_value getValue(int invert=0);
	string toString();
};


struct st_line
{
	int type;
	int linenum;
	vector<st_token> tokens;
	map<string,size_t> labels;
	st_user_proc *parent_proc;

	st_line();
	st_line(bool is_list);
	st_line(st_line *line);
	st_line(shared_ptr<st_line> &rhs);
	st_line(st_line *line, size_t from);
	st_line(
		st_user_proc *proc,
		int _linenum, st_line *parent, size_t from, size_t to);

	void parseAndExec(string &rdline);
	bool tokenise(string &rdline);
	void addToken(int type, string &strval);
	void addOpToken(char c, int opcode);
	void setUndefinedTokens();
	void negateTokens();
	void createSubLists();
	void matchBrackets();
	void setLabels();

	size_t execute(size_t from=0);
	t_result evalExpression(size_t tokpos);
	t_result execUserProc(size_t tokpos);
	void evalStack(stack<st_value> &valstack, stack<int> &opstack);
	size_t skipRHSofAND(size_t pos);

	bool operator==(st_line &rhs);
	bool operator!=(st_line &rhs);
	void operator+=(st_line &rhs);
	void operator*=(int cnt);

	shared_ptr<st_line> evalList();
	void listShuffle();

	st_value getListElement(int index);
	shared_ptr<st_line> getListPiece(size_t from, size_t to);

	shared_ptr<st_line> setListFirst(st_value &val);
	shared_ptr<st_line> setListLast(st_value &val);
	void setLineNum(int _linenum);

	size_t listMemberp(st_value &val);
	string listToString();

	void   addLabel(size_t tokpos);
	void   addDefaultLabel(size_t tokpos);
	size_t labelIndex(const char *name);

	bool   isExprEnd(size_t tokpos);
	void   clear();
	void   dump(FILE *fp, int &indent, bool show_linenum);
	int    getIndentCount(int com);
	string toString();
	string toSimpleString();
	void   printTrace(char call_type, const char *name);
};


// Procedure definitions
struct st_user_proc
{
	string name;
	int exec_linenum;
	int next_linenum;

	// Just the parameter names. The actual vars are created in
	// st_user_proc_inst
	vector<string> params;

	// Using a list here would be better for line insertion and deletion
	// but that would make gotos a pain.
	vector<shared_ptr<st_line>> lines;

	st_user_proc(string &_procname, st_line *line, size_t &tokpos);

	void   addLine(st_line *line);
	void   addReplaceDeleteLine(int linenum, st_line *line, size_t from);
	size_t labelLineIndex(const char *name);
	void   renumber();
	void   execute();
	void   dump(FILE *fp, bool full_dump, bool show_linenums);
};


// Procedure instances on stack
struct st_user_proc_inst
{
	st_user_proc *proc;
	// Includes parameters
	t_var_map local_vars;
	t_result retval;
	bool ret_set;

	st_user_proc_inst(st_user_proc *_proc): proc(_proc), ret_set(false) { }

	size_t setParams(st_line *line, size_t tokpos);
	void   setLocalVar(string &name, st_value &val);
	t_var_map::iterator getLocalVar(const char *name);
	bool   getLocalVarValue(string &name, st_value &val);
	void   execute();
};


struct st_turtle_line
{
	int colour;
	int width;
	int style;
	XPoint from;
	XPoint to;

	st_turtle_line(
		int col, int wd, int st,
		double xf, double yf, double xt, double yt);
	void draw();
};


// Class for turtle 
struct st_turtle
{
	double x;
	double y;
	double prev_x;
	double prev_y;
	double angle;
	double size;
	int pen_colour;
	int line_width;
	int line_style;
	int win_edge;
	bool pen_down;
	bool visible;
	bool store_fill;
	string line_style_str;
	t_shape draw_lines;

	// Need seperate groups of fill lines that bound the shapes.
	// bool = fill yet?, int = colour, t_shape = polygon lines
	vector<tuple<bool,int,t_shape>> fill_polys;

	// int = col, XPoint = x,y
	vector<pair<int,XPoint>> dots;

	st_turtle();

	void reset();
	void clear();
	void home();
	void move(int dist);
	void rotate(double ang);

	void setXY(double _x, double _y);
	void setX(double _x);
	void setY(double _y);
	void setVisible(bool vis);
	void setAngle(double ang);
	void setColour(int col);
	void setPenDown(bool down);
	void setUnbounded();
	void setFence();
	void setWrap();
	void setBackground(int col);
	void setLineWidth(int width);
	void setLineStyle(string style);
	void setFill();
	void setSize(int _size);
	void setTowards(double _x, double _y);

	void drawDot(short dx, short dy);
	void draw();
	void drawAll();
	void fill();
	void fillPolygon(int col, t_shape &polygon);

	shared_ptr<st_line> facts();
};


struct st_flags
{
	// Cmd line
	unsigned do_graphics:1;
	unsigned map_window:1;
	unsigned indent_label_blocks:1;

	// Runtime
	unsigned window_mapped:1;
	unsigned do_break:1;
	unsigned executing:1;
	unsigned suppress_prompt:1;
	unsigned angle_in_degs:1;
};

// Arrays
#ifdef MAINFILE
const char *error_str[NUM_ERRORS] =
{
	// 0
	"OK",
	"Syntax error",
	"Missing quotes",
	"Missing bracket",
	"Unexpected bracket",

	// 5
	"Unexpected operator",
	"Divide by zero",
	"Missing argument(s)",
	"Invalid argument",
	"Invalid character",

	// 10
	"Cannot invert",
	"Cannot negate",
	"Cannot convert",
	"Undefined variable",
	"Unwatched variable",

	// 15
	"Invalid variable name",
	"Invalid variable type",
	"Read only variable",
	"Cannot EVALuate",
	"Cannot RUN",

	// 20
	"Out of bounds",
	"Value out of range",
	"IF requires a list block to execute",
	"IF missing exec list block",
	"Duplicate declaration",

	// 25
	"Unexpected TO",
	"Unexpected END",
	"Unexpected ED",
	"Unexpected ER or ERALL",
	"Unexpected argument",

	// 30
	"Undefined user procedure",
	"Not in a user procedure",
	"Max nesting depth exceeded",
	"Undefined label",
	"Turtle graphics are not available",

	// 35
	"Turtle graphics initialisation failed",
	"Invalid colour",
	"Open failed",
	"Read failed",
	"Write failed",

	// 40
	"Stat failed",
	"Change directory failed",
	"Invalid history command number",
	"Turtle out of bounds",
	"Cannot FILL when turtle is wrapped",

	// 45
	"Cannot RESTART while doing initial load",
	"Invalid format",
	"Invalid path/filename or path not found",
	"Path or filename too long"
};


int op_prec[NUM_OPS] =
{
	// 0
	0,
	0,
	0,
	0,
	6,

	// 5 
	1,
	1,
	1,
	2,
	2,

	// 10
	2,
	2,
	2,
	2,
	3,

	// 15 
	3,
	4,
	4,
	5,
	6
};

// Only used in st_line::tokenise() but better to have op defs in one place
map<char,en_op> single_char_ops =
{
	{ '[', OP_L_SQR_BRACKET },
	{ ']', OP_R_SQR_BRACKET },
	{ '(', OP_L_RND_BRACKET },
	{ ')', OP_R_RND_BRACKET },
	{ '=', OP_EQUALS },
	{ '>', OP_GREATER },
	{ '<', OP_LESS },
	{ '+', OP_ADD },
	{ '-', OP_SUB },
	{ '*', OP_MUL, },
	{ '/', OP_DIV },
	{ '%', OP_MOD },
	{ '^', OP_PWR }
};
#else
extern const char *error_str[NUM_ERRORS];
extern int op_prec[NUM_OPS];
extern map<char,en_op> single_char_ops;
#endif

// Cmd line params not including flags
EXTERN int win_width;
EXTERN int win_height;
EXTERN int max_history_lines;
EXTERN char *xdisp;

// X
EXTERN Display *display;
EXTERN Window win;
EXTERN GC gc[NUM_COLOURS+1];
EXTERN int win_colour;
EXTERN int x_sock;
EXTERN st_turtle *turtle; // Can't be created until X initialised

// Runtime
EXTERN st_flags flags;
EXTERN st_io io;
EXTERN struct termios saved_tio;
EXTERN shared_ptr<st_user_proc> def_proc;
EXTERN unordered_set<string> watch_vars;
EXTERN st_user_proc *stop_proc;
EXTERN st_user_proc_inst *curr_proc_inst;
// Unordered quicker than standard map on lookups, slower on deletions and uses
// more memory
EXTERN unordered_map<string,shared_ptr<st_user_proc>> user_procs;
EXTERN t_var_map global_vars;
EXTERN string loadproc;  // Only used with LOAD command
EXTERN int logo_state;
EXTERN int nest_depth;
EXTERN int tracing_mode;

/**************************** FORWARD DECLARATIONS ***************************/

// xwin.cc
bool xInit();
void xParseEvent();
void xSetWindowTitle();
void xSetWindowBackground(int col);
void xSetLineWidth(int col, int width);
void xSetLineStyle(int col, int style);
void xWindowClear();
void xWindowMap();
void xWindowUnmap();
void xResizeWindow(int width, int height);
void xDrawPoint(int col, int x, int y);
void xDrawLine(int col, int xf, int yf, int xt, int yt);
void xDrawLine(int col, double xf, double yf, double xt, double yt);
void xDrawPolygon(int col, XPoint *pnts, int cnt, bool fill);

// commands.cc
size_t comRem(st_line *line, size_t tokpos);
size_t comEd(st_line *line, size_t tokpos);
size_t comTo(st_line *line, size_t tokpos);
size_t comEnd(st_line *line, size_t tokpos);
size_t comReturn(st_line *line, size_t tokpos);
size_t comRestart(st_line *line, size_t tokpos);
size_t comEr(st_line *line, size_t tokpos);
size_t comErall(st_line *line, size_t tokpos);
size_t comPoRenum(st_line *line, size_t tokpos);
size_t comPops(st_line *line, size_t tokpos);
size_t comPons(st_line *line, size_t tokpos);
size_t comPosf(st_line *line, size_t tokpos);
size_t comPoall(st_line *line, size_t tokpos);
size_t comPrWr(st_line *line, size_t tokpos);
size_t comEat(st_line *line, size_t tokpos);
size_t comMake(st_line *line, size_t tokpos);
size_t comIncDec(st_line *line, size_t tokpos);
size_t comRun(st_line *line, size_t tokpos);
size_t comStop(st_line *line, size_t tokpos);
size_t comWait(st_line *line, size_t tokpos);
size_t comBye(st_line *line, size_t tokpos);
size_t comRepeat(st_line *line, size_t tokpos);
size_t comIf(st_line *line, size_t tokpos);
size_t comLabel(st_line *line, size_t tokpos);
size_t comGo(st_line *line, size_t tokpos);
size_t comOp(st_line *line, size_t tokpos);
size_t comGraphics0Args(st_line *line, size_t tokpos);
size_t comGraphics1Arg(st_line *line, size_t tokpos);
size_t comGraphics2Args(st_line *line, size_t tokpos);
size_t comSetInd(st_line *line, size_t tokpos);
size_t comSave(st_line *line, size_t tokpos);
size_t comLoad(st_line *line, size_t tokpos);
size_t comCD(st_line *line, size_t tokpos);
size_t comHelp(st_line *line, size_t tokpos);
size_t comHist(st_line *line, size_t tokpos);
size_t comChist(st_line *line, size_t tokpos);
size_t comTrace(st_line *line, size_t tokpos);
size_t comWatch(st_line *line, size_t tokpos);
size_t comUnWatch(st_line *line, size_t tokpos);
size_t comSeed(st_line *line, size_t tokpos);
size_t comAngleMode(st_line *line, size_t tokpos);
size_t comCT(st_line *line, size_t tokpos);

// procedures.cc
t_result procEval(st_line *line, size_t tokpos);
t_result procFirstLast(st_line *line, size_t tokpos);
t_result procBFL(st_line *line, size_t tokpos);
t_result procPiece(st_line *line, size_t tokpos);
t_result procCount(st_line *line, size_t tokpos);
t_result procNump(st_line *line, size_t tokpos);
t_result procStrp(st_line *line, size_t tokpos);
t_result procListp(st_line *line, size_t tokpos);
t_result procFLput(st_line *line, size_t tokpos);
t_result procItem(st_line *line, size_t tokpos);
t_result procMemberp(st_line *line, size_t tokpos);
t_result procUpperCase(st_line *line, size_t tokpos);
t_result procLowerCase(st_line *line, size_t tokpos);
t_result procAscii(st_line *line, size_t tokpos);
t_result procChar(st_line *line, size_t tokpos);
t_result procRC(st_line *line, size_t tokpos);
t_result procRL(st_line *line, size_t tokpos);
t_result procTF(st_line *line, size_t tokpos);
t_result procNum(st_line *line, size_t tokpos);
t_result procStr(st_line *line, size_t tokpos);
t_result procRandom(st_line *line, size_t tokpos);
t_result procMaths(st_line *line, size_t tokpos);
t_result procShuffle(st_line *line, size_t tokpos);
t_result procMatch(st_line *line, size_t tokpos);
t_result procDir(st_line *line, size_t tokpos);
t_result procGetDir(st_line *line, size_t tokpos);
t_result procGetSecs(st_line *line, size_t tokpos);
t_result procGetDate(st_line *line, size_t tokpos);
t_result procFmt(st_line *line, size_t tokpos);
t_result procPad(st_line *line, size_t tokpos);
t_result procSplit(st_line *line, size_t tokpos);
t_result procList(st_line *line, size_t tokpos);
t_result procPath(st_line *line, size_t tokpos);

// files.cc
void loadProcFile(string filepath, string procname);
void saveProcFile(string filepath, string &procname, bool psave);

// vars.cc
void     setSystemVars();
void     setWindowSystemVars();
void     clearGlobalVariables();
st_value getVarValue(string &name);
t_var_map::iterator getGlobalVar(string &name);

// path.cc
en_error matchPath(int type, char *pat, string &matchpath, bool toplevel = true);
bool pathHasWildCards(string &path);

// strings.cc
bool   isNumber(string str);
string numToString(double num);
bool   wildMatch(const char *str, const char *pat, bool case_sensitive);

// misc.cc
void   printRunError(int errnum, const char *tokstr);
void   printStopMesg(const char *stopword);
void   printWatch(char type, string &name, st_value &val);
void   ready();
void   prompt();
void   goodbye();
void   doExit(int code);


#ifdef MAINFILE
/* Commands don't return values (other than next position) and are used as
   a demarcater when parsing an expression as Logo has no line terminators.
   An array is used instead of a map since I want to do strcasecmp on the
   name. */
pair<const char *,function<int(st_line *, size_t)>> commands[NUM_COMS] =
{
	// 0
	{ ";",       comRem },
	{ "REM",     comRem },
	{ "ED",      comEd },
	{ "TO",      comTo },
	{ "END",     comEnd },

	// 5
	{ "RESTART", comRestart },
	{ "ER",      comEr },
	{ "ERV",     comErall },
	{ "ERP",     comErall },
	{ "ERALL",   comErall },

	// 10
	{ "PO",      comPoRenum },
	{ "POL",     comPoRenum },
	{ "POPS",    comPops },
	{ "POPSL",   comPops },
	{ "POTS",    comPops },

	// 15
	{ "PONS",    comPons },
	{ "POSF",    comPosf },
	{ "POALL",   comPoall },
	{ "RENUM",   comPoRenum },
	{ "PR",      comPrWr },

	// 20
	{ "WR",      comPrWr },
	{ "EAT",     comEat },
	{ "MAKE",    comMake },
	{ "MAKELOC", comMake },
	{ "INC",     comIncDec },

	// 25
	{ "DEC",     comIncDec },
	{ "RUN",     comRun },
	{ "STOP",    comStop },
	{ "WAIT",    comWait },
	{ "BYE",     comBye },

	// 30
	{ "REPEAT",  comRepeat },
	{ "IF",      comIf },
	{ "LABEL",   comLabel },
	{ "DLABEL",  comLabel },
	{ "GO",      comGo },

	// 35
	{ "OP",      comOp },
	{ "HOME",    comGraphics0Args },
	{ "CLEAR",   comGraphics0Args },
	{ "HT",      comGraphics0Args },
	{ "ST",      comGraphics0Args },

	// 40
	{ "HW",      comGraphics0Args },
	{ "SW",      comGraphics0Args },
	{ "CS",      comGraphics0Args },
	{ "PU",      comGraphics0Args },
	{ "PD",      comGraphics0Args },

	// 45
	{ "TG",      comGraphics0Args },
	{ "FD",      comGraphics1Arg },
	{ "BK",      comGraphics1Arg },
	{ "LT",      comGraphics1Arg },
	{ "RT",      comGraphics1Arg },

	// 50
	{ "SETPC",   comGraphics1Arg },
	{ "SETBG",   comGraphics1Arg },
	{ "SETX",    comGraphics1Arg },
	{ "SETY",    comGraphics1Arg },
	{ "SETH",    comGraphics1Arg },

	// 55
	{ "SETSZ",   comGraphics1Arg },
	{ "SETLW",   comGraphics1Arg },
	{ "SETLS",   comGraphics1Arg },
	{ "SETPOS",  comGraphics2Args },
	{ "TOWARDS", comGraphics2Args },

	// 60
	{ "DOT",     comGraphics2Args },
	{ "WINDOW",  comGraphics0Args },
	{ "FENCE",   comGraphics0Args },
	{ "WRAP",    comGraphics0Args },
	{ "SETIND",  comSetInd },

	// 65
	{ "SETFILL", comGraphics0Args },
	{ "SETWINSZ",comGraphics2Args },
	{ "FILL",    comGraphics0Args },
	{ "SAVE",    comSave },
	{ "LOAD",    comLoad },

	// 70
	{ "CD",      comCD   },
	{ "HELP",    comHelp },
	{ "SHELP",   comHelp },
	{ "HIST",    comHist },
	{ "CHIST",   comChist },

	// 75
	{ "TRON",    comTrace },
	{ "TRONS",   comTrace },
	{ "TROFF",   comTrace },
	{ "WATCH",   comWatch },
	{ "UNWATCH", comUnWatch },

	// 80
	{ "SEED",    comSeed },
	{ "DEG",     comAngleMode },
	{ "RAD",     comAngleMode },
	{ "CT",      comCT }
};

// Built in system procedures that take value(s) and return a result. Array 
// used for same reason as above.
pair<const char *,function<t_result(st_line *, size_t)>> sysprocs[NUM_SPROCS] =
{
	// 0
	{ "EVAL",   procEval },
	{ "FIRST",  procFirstLast },
	{ "LAST",   procFirstLast },
	{ "BF",     procBFL },
	{ "BL",     procBFL },

	// 5
	{ "PIECE",  procPiece },
	{ "COUNT",  procCount },
	{ "NUMP",   procNump },
	{ "STRP",   procStrp },
	{ "LISTP",  procListp },

	// 10
	{ "FPUT",   procFLput },
	{ "LPUT",   procFLput },
	{ "ITEM",   procItem },
	{ "MEMBERP",procMemberp },
	{ "UC",     procUpperCase },

	// 15
	{ "LC",     procLowerCase },
	{ "ASCII",  procAscii },
	{ "CHAR",   procChar },
	{ "RC",     procRC },
	{ "RL",     procRL },

	// 20
	{ "TF",     procTF },
	{ "NUM",    procNum },
	{ "STR",    procStr },
	{ "SSTR",   procStr },
	{ "SPLIT",  procSplit },

	// 25
	{ "RANDOM", procMaths },
	{ "INT",    procMaths },
	{ "ROUND",  procMaths },
	{ "SIN",    procMaths },
	{ "COS",    procMaths },

	// 30
	{ "TAN",    procMaths },
	{ "ASIN",   procMaths },
	{ "ACOS",   procMaths },
	{ "ATAN",   procMaths },
	{ "LOG",    procMaths },

	// 35
	{ "LOG2",   procMaths },
	{ "LOG10",  procMaths },
	{ "SQRT",   procMaths },
	{ "ABS",    procMaths },
	{ "SGN",    procMaths },

	// 40
	{ "SHUFFLE",procShuffle },
	{ "MATCH",  procMatch },
	{ "MATCHC", procMatch },
	{ "DIR",    procDir },
	{ "GETDIR", procGetDir },

	// 45
	{ "GETSECS",procGetSecs },
	{ "GETDATE",procGetDate },
	{ "FMT",    procFmt },
	{ "LPAD",   procPad },
	{ "RPAD",   procPad },

	// 50
	{ "LIST",   procList },
	{ "PATH",   procPath }
};
#else
extern pair<const char *,function<int(st_line *, size_t)>> commands[NUM_COMS];
extern pair<const char *,function<t_result(st_line *, size_t)>> sysprocs[NUM_SPROCS];
#endif

/******************************* TEMPLATE FUNCS ******************************/

// Template because we use raw values setting up system vars, not st_value
template<typename T>
void setGlobalVarValue(const char *name, T newval)
{
	// See if a local variable with the same name already exists
	if (curr_proc_inst)
	{
		auto mit = curr_proc_inst->getLocalVar(name);
		if (mit != curr_proc_inst->local_vars.end())
			throw t_error({ ERR_DUP_DECLARATION, name });
	}
	if (watch_vars.find(name) != watch_vars.end())
	{
		st_value val = st_value(newval);
		string sname = name;
		printWatch('G',sname,val);
		global_vars[name] = val;
	}
	else global_vars[name] = st_value(newval);
}




template<typename T>
void setGlobalVarValue(string &name, T newval)
{
	setGlobalVarValue(name.c_str(),newval);
}


#include "globals.h"

#define CONTROL_D 4
#define DEL1      8
#define DEL2      127
#define ESC       27

// Keyboard escape sequences
enum
{
	/* 0 */
	ESC_K,
	ESC_J,
	ESC_UP_ARROW,
	ESC_DOWN_ARROW,
	ESC_LEFT_ARROW,

	/* 5 */
	ESC_RIGHT_ARROW,
	ESC_INSERT,
	ESC_DELETE,
	ESC_F1, // PC Insert key doesn't work on a MAC so use this instead

	NUM_ESC_SEQS
};

const char *esc_seq[NUM_ESC_SEQS] =
{
	/* 0 */
	"k",
	"j",
	"[A",
	"[B",
	"[D",

	/* 5 */
	"[C",
	"[2~",
	"[3~",
	"OP"
};



st_io::st_io()
{
	history_pos = 0;
	tio_saved = false;
	insert = true;
	reset();
}




void st_io::reset()
{
	rdline = "";
	esc_code = "";
	cursor_pos = 0;
}




void st_io::kbRawMode()
{
	struct termios tio;

	// Get current settings 
	tcgetattr(0,&tio);
	if (!tio_saved)
	{
		tcgetattr(0,&saved_tio);
		tio_saved = true;
	}

	// Echo off 
	tio.c_lflag &= ~ECHO;

	// Disable canonical mode 
	tio.c_lflag &= ~ICANON;

	// Don't strip off 8th bit 
	tio.c_iflag &= ~ISTRIP;

	// Set buffer size to 1 byte and no delay between reads 
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	tcsetattr(0,TCSANOW,&tio);
}




void st_io::kbSaneMode()
{
	tcsetattr(0,TCSANOW,&saved_tio);
}




/*** Reads input and does escape sequence processing and cursor contol.
     Returns true if newline pressed ***/
bool st_io::readInput(
	int fd, bool do_echo, bool single_char, bool control_d, int &rr)
{
	bool kb_escape = false;
	bool str_escape = false;
	bool partial;
	fd_set mask;
	char c;

	// Keyboard escape sequences will all get read at once, they won't be
	// broken up between select() calls
	esc_code = "";

	// Loop reading the descriptor
	while(1)
	{
		// Have to use select so we can respond to signals - read() 
		// ignores them.
		FD_ZERO(&mask);
		FD_SET(fd,&mask);

		switch(select(FD_SETSIZE,&mask,0,0,0))
		{
		case -1:
			if (errno == EINTR)
			{
				if (flags.do_break)
					throw t_interrupt({ INT_BREAK, "" });
			}
			return false;
		case 0:
			// Timeout - should never happen
			assert(0);
		}
	
		if ((rr = read(fd,&c,1)) < 1) break;

		// If we're receiving an escape sequence...
		if (kb_escape)
		{
			esc_code += c;
			partial = false;

			// See if we have a value sequence
			for(int seq=0;seq < NUM_ESC_SEQS;++seq)
			{
				// Check for match
				if (esc_code == esc_seq[seq])
				{
					processEscSeq(seq,do_echo);
					kb_escape = false;
					esc_code = "";
					break;
				}

				// Check for partial match. If no partial match
				// then we'll bin it
				if (!strncmp(
					esc_seq[seq],
					esc_code.c_str(),esc_code.size()))
				{
					partial = true;
				}
			}
			// If we've found a full or partial match loop
			if (!kb_escape || partial) continue;

			// Invalid sequence, treat as normal input
			kb_escape = false;
			esc_code = "";
		}

		// Normal input but check for special characters
		switch(c)
		{
		case '\\':
			// Most escape processing happens in st_line::tokenise()
			// This is just to catch lines continued past a newline
			if (str_escape)
			{
				rdline.pop_back();
				--cursor_pos;
				str_escape = false;
			}
			else str_escape = true;
			break;
		case '\r':
			return false;
		case '\n':
			if (do_echo) putchar('\n');
			if (str_escape)
			{
				// Continuing onto the next line. Remove \ from
				// end of the string first
				rdline.pop_back();
				--cursor_pos;
				if (do_echo) cout << "~ " << flush;
				return false;
			}
			return true;
		case CONTROL_D:
			if (control_d) goodbye();
			break;
		case DEL1:
		case DEL2:
			if (rdline.size()) deleteChar(do_echo);
			return false;
		case ESC:
			kb_escape = true;
			continue;
		default:
			if (str_escape) str_escape = false;
		}

		addCharToReadLine(c,do_echo);
		if (single_char) break;
	}
	if (rr == -1) throw t_error({ ERR_READ_FAIL, "" });

	return false;
}




void st_io::processEscSeq(int seq, bool do_echo)
{
	switch(seq)
	{
	case ESC_F1:
	case ESC_INSERT:
		insert = !insert;
		break;

	case ESC_DELETE:
		deleteChar(do_echo);
		break;

	case ESC_J:
	case ESC_UP_ARROW:
		if (history.size()) 
		{
			clearLineAndWrite(rdline.size(),"",false);
			size_t len = rdline.size();
			if (--history_pos < 0) history_pos = history.size() - 1;
			rdline = history[history_pos];
			clearLineAndWrite(len,rdline,false);
			cursor_pos = rdline.size();
		}
		return;

	case ESC_K:
	case ESC_DOWN_ARROW:
		if (history.size()) 
		{
			clearLineAndWrite(rdline.size(),"",false);
			size_t len = rdline.size();
			history_pos = (history_pos + 1) % history.size();
			rdline = history[history_pos];
			clearLineAndWrite(len,rdline,false);
			cursor_pos = rdline.size();
		}
		return;

	case ESC_LEFT_ARROW:
		if (cursor_pos)
		{
			--cursor_pos;
			cout << "\b" << flush;
		}
		return;

	case ESC_RIGHT_ARROW:
		if (cursor_pos < (int)rdline.size())
		{
			++cursor_pos;
			// Use ANSI escape code to move cursor forward 1 char.
			// There's no ascii code to do this sadly.
			cout << "\033[1C" << flush;
		}
		break;
	}
}




void st_io::clearLineAndWrite(
	size_t len, string write_line, bool reset_cursor)
{
	// +2 for prompt length
	cout << "\r" << string(len+2,' ') << "\r" << flush;
	prompt();
	cout << write_line << flush;
	if (reset_cursor)
	{
		// Move terminal cursor back as it will now be at the end of 
		// the line
		cout << string(write_line.size() - cursor_pos,'\b') << flush;
	}
}




/*** Delete a character from the current cursor position ***/
void st_io::deleteChar(bool do_echo)
{
	if (cursor_pos == (int)rdline.size())
	{
		// Delete from the end
		rdline.pop_back();
		--cursor_pos;
		if (do_echo) cout << "\b \b" << flush;
	}
	else if (cursor_pos)
	{
		// Delete the character just before cursor pos
		--cursor_pos;
		size_t len = rdline.size();
		rdline.erase(cursor_pos,1);
		if (do_echo) clearLineAndWrite(len,rdline,true);
	}
}




/*** Add the character to rdline and update the line in the terminal ***/
void st_io::addCharToReadLine(char c, bool do_echo)
{
	if (cursor_pos != (int)rdline.size())
	{
		if (insert)
		{
			size_t len = rdline.size();
			rdline.insert(cursor_pos,&c,1);
			++cursor_pos;

			// Terminal cursor will have overwritten char so need
			// to redraw string
			if (do_echo) clearLineAndWrite(len,rdline,true);
			return;
		}
		rdline[cursor_pos] = c;
	}
	else rdline += c;

	if (do_echo) cout << c << flush;
	++cursor_pos;
}




/*** Get the line to parse the input ***/
bool st_io::parseInput()
{
	// Check for historic cmd execution. Format is !<num>
	if (rdline[0] == '!')
	{
		string numstr = rdline.substr(1);
		int comnum = atoi(numstr.c_str());
		if (!isNumber(numstr) || 
		    comnum < 1 || comnum > (int)history.size())
		{
			printRunError(ERR_INVALID_HIST_CMD,numstr.c_str());
			reset();
			prompt();
			return false;
		}
		rdline = history[comnum-1];
		cout << rdline << endl;
	}
	return execLine();
}




/*** Called if code given on command line ***/
void st_io::execText(string text)
{
	reset();
	rdline = text;
	execLine();
}




/*** Run code in rdline. Return value is only for when reading files during 
     loading ***/
bool st_io::execLine()
{
	st_line line;
	bool ret = true;

	// Run the LOGO line
	flags.executing = true;
	try
	{
		line.parseAndExec(rdline);
	}
	catch(t_error &err)
	{
		if (curr_proc_inst && !stop_proc)
			stop_proc = curr_proc_inst->proc;
		printRunError(err.first,err.second.c_str());
		ret = false;
	}
	catch(t_interrupt &inter)
	{
		ret = false;
		switch(inter.first)
		{
		case INT_BREAK:
			printStopMesg("BREAK");
			flags.do_break = false;
			break;
		case INT_STOP:
			printStopMesg("STOP");
			break;
		case INT_GOTO:
			printRunError(ERR_UNDEFINED_LABEL,inter.second.c_str());
			break;
		case INT_RESTART:
			throw;
		default:
			assert(0);
		}
	}
	addHistoryLine();
	flags.executing = false;
	reset();
	prompt();
	return ret;
}



/////////////////////////////////// HISTORY //////////////////////////////////

void st_io::addHistoryLine()
{
	if (rdline.size())
	{
		history.push_back(rdline);
		if ((int)history.size() > max_history_lines)
			history.pop_front();
		else
			history_pos = history.size();
	}
}




void st_io::printHistory(int num_lines)
{
	puts("Command history");
	puts("---------------");
	size_t cnt;
	size_t hsize = history.size();

	if (num_lines)
		cnt = (size_t)num_lines < hsize ? (size_t)num_lines : hsize;
	else
		cnt = hsize;

	for(size_t i=hsize - cnt;i < hsize;++i)
		printf("%-3ld %s\n",i+1,history[i].c_str());
}




void st_io::clearHistory()
{
	history.clear();
	puts("History buffer cleared.");
}

CC=c++ -g -std=c++11 -O2
# No-ops assert()
#CC=c++ -g -std=c++11 -O2 -DNDEBUG

LIBPATH=-L/usr/X11R6/lib -L/usr/X11R6/lib64
LIBS=-lX11 -lXext -lm

COMP=$(CC) -Wall -c -I/usr/X11/include
OBJS=\
	main.o \
	st_io.o \
	st_line.o \
	st_token.o \
	st_value.o \
	st_user_proc.o \
	st_user_proc_inst.o \
	st_turtle.o \
	st_turtle_line.o \
	commands.o \
	sys_procs.o \
	vars.o \
	path.o \
	xwin.o \
	misc.o 

DEPS=globals.h Makefile

logo: $(OBJS) Makefile
	$(CC) $(OBJS) $(LIBPATH) $(LIBS) -o logo

build_date:
	echo "#define BUILD_DATE \"`date +'%Y-%m-%d %T %Z'`\"" > build_date.h

main.o: main.cc $(DEPS) build_date
	$(COMP) main.cc

st_io.o: st_io.cc $(DEPS)
	$(COMP) st_io.cc

st_line.o: st_line.cc $(DEPS)
	$(COMP) st_line.cc

st_token.o: st_token.cc $(DEPS)
	$(COMP) st_token.cc

st_value.o: st_value.cc $(DEPS)
	$(COMP) st_value.cc

st_user_proc.o: st_user_proc.cc $(DEPS)
	$(COMP) st_user_proc.cc

st_user_proc_inst.o: st_user_proc_inst.cc $(DEPS)
	$(COMP) st_user_proc_inst.cc

st_turtle.o: st_turtle.cc $(DEPS)
	$(COMP) st_turtle.cc

st_turtle_line.o: st_turtle_line.cc $(DEPS)
	$(COMP) st_turtle_line.cc

commands.o: commands.cc $(DEPS)
	$(COMP) commands.cc

sys_procs.o: sys_procs.cc $(DEPS)
	$(COMP) sys_procs.cc

vars.o: vars.cc $(DEPS) build_date
	$(COMP) vars.cc

path.o: path.cc $(DEPS)
	$(COMP) path.cc

xwin.o: xwin.cc $(DEPS)
	$(COMP) xwin.cc

misc.o: misc.cc $(DEPS)
	$(COMP) misc.cc

clean:
	rm -f logo *.o core* build_date.h

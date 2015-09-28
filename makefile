######################################################################
#	MakeFile for Borland C/C++ Compiler
#

CC  = bcc32
LN  = bcc32
RL  = brc32
RC  = brcc32

CFLAG = -c -O1 -O2 -Oc -Oi -Ov
LFLAG = -tWD -e$(EXE) -O1 -O2
RFLAG = 

EXE = warpsharp.auf
OBJ = warpsharp.obj
RES = warpsharp.res


all: $(EXE)


$(EXE): $(OBJ) $(RES)
	$(LN) $(LFLAG) $(OBJ)
	$(RL) -fe$(EXE) $(RES)

warpsharp.obj: warpsharp.cpp filter.h
	$(CC) $(CFLAG) warpsharp.cpp

$(RES): warpsharp.rc
	$(RC) $(RFLAG) warpsharp.rc

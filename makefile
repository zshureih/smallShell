CXX = gcc
CXXFLAGS = -std=c99
NAME = smallsh
SRCS = main.c builtInFuncs.c
HEADERS = builtInFuncs.h shellLoop.h

smallShell: ${SRCS} ${HEADERS}
		${CXX} ${SRCS} ${CXXFLAGS} -o ${NAME}

clean: 
	-rm ${NAME}
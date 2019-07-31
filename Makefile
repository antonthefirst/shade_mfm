CC:=g++
CFLAGS:=-Wall -std=c++0x -Wno-unknown-pragmas -fpermissive
DEFINES:=-DACKLEYHAX
SRCS:=$(wildcard core/*.cpp)
SRCS+=$(wildcard src/*.cpp)
SRCS+=$(wildcard wrap/*.cpp)
SRCS+=$(wildcard libs/imgui/*.cpp)
SRCS+=$(wildcard libs/GL/*.c)
INCLUDES:=-I./libs -I. -ldl -lGL -lglfw
PROGRAM:=shademfm

$(PROGRAM):	$(SRCS) Makefile
	$(CC) $(CFLAGS) $(DEFINES) $(SRCS) $(INCLUDES) -o $(PROGRAM)

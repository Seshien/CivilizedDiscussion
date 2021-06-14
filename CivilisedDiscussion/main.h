#pragma once
#include <cstdio>
#include <iostream>
#include <thread>
#ifdef _WIN32
	#include <windows.h>
	#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#define DISABLE_NEWLINE_AUTO_RETURN  0x0008
#endif
#include "mpi.h"
#include "MsgStructure.h"
#include "Debater.h"




MPI_Datatype MPI_structure;


void check_thread_support(int provided);

void initStruct();

void finish();


#ifdef _WIN32
void addColours()
{
	DWORD l_mode;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleMode(hStdout, &l_mode);
	SetConsoleMode(hStdout, l_mode |
		ENABLE_VIRTUAL_TERMINAL_PROCESSING |
		DISABLE_NEWLINE_AUTO_RETURN);
}
#endif

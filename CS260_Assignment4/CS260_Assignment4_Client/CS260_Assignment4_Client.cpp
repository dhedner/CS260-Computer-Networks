//---------------------------------------------------------
// file:	CS260_Assignment4_Client.cpp
// author:	Matthew Picioccio, David Hedner
// email:	matthew.picioccio@digipen.edu, david.hedner@digipen.edu
//
// brief:	Entry point
//
// C-Processing documentation link:
// https://inside.digipen.edu/main/GSDP:GAM100/CProcessing
//
// Copyright � 2020 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include "cprocessing.h"
#include "PlayGameState.h"


/// <summary>
/// Enable the console, so we can see useful cout and cerr output.
/// For reference, see one of the newer answers in https://stackoverflow.com/questions/311955/redirecting-cout-to-a-console-in-windows.
/// </summary>
/// <remarks>In modern Windows SDKs, the old freopen method is not sufficient to capture cout/cerr.  This method does work.</remarks>
void ShowConsole()
{
	AllocConsole();
	
	static std::ofstream conout("CONOUT$", std::ios::out);
	std::cout.rdbuf(conout.rdbuf());
	std::cerr.rdbuf(conout.rdbuf());

	// NOTE: CP_Engine_Run will call FreeConsole() for us when the program is exiting.
}


/// <summary>
/// Entry point for the process
/// </summary>
/// <returns>0 if successful, non-zero for process errors</returns>
int main(void)
{
	ShowConsole();

	//TODO: initialize winsock
	// -- if it fails, output to cerr and return 1
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

	// establish the initial window settings
	CP_System_SetWindowTitle("CS 260 Assignment 4");
	CP_System_SetWindowSize(1024, 768);

	// establish the initial game state
	PlayDefaultGame();

	// start the simulation
	CP_Engine_Run();

	return 0;
}
//---------------------------------------------------------
// file:	LockstepConnectingState.cpp
// author:	Matthew Picioccio, David Hedner
// email:	matthew.picioccio@digipen.edu, david.hedner@digipen.edu
//
// brief:	Global functions for the "Connecting to Lockstep" game state
//
// Copyright © 2020 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <cassert>
#include <iostream>
#include <WS2tcpip.h>
#include "cprocessing.h"
#include "LockstepConnectingState.h"
#include "LockstepConfiguringState.h"
#include "LockstepHostingState.h"
#include "PlayGameState.h"
#include "LockstepGame.h"


u_short connecting_port;
SOCKET connecting_socket = INVALID_SOCKET;
float connecting_timeout_secs = 3.0f;
std::string connecting_text;


/// <summary>
/// Handles errors from socket functions, including transitioning to different game states.
/// </summary>
/// <param name="error_text">Text that describes the error.</param>
/// <remarks>WSAGetLastError will be included in the output by this function.</remarks>
/// <returns>True if the error is "fatal" and the calling function should stop; false if it can be ignored.</returns>
bool LockstepConnectingState_HandleSocketError(const char* error_text)
{
	const auto wsa_error = WSAGetLastError();
	
	// ignore WSAEWOULDBLOCK
	if (wsa_error == WSAEWOULDBLOCK)
	{
		return false;
	}
	
	// we expect WSAECONNRESET - it means we should give up and try hosting instead
	if (wsa_error == WSAECONNRESET)
	{
		std::cout << "Received WSAECONNRESET when attempting to connect to a game server on port " << connecting_port << ", attempting to host instead..." << std::endl;
		HostLockstep(connecting_port);
		closesocket(connecting_socket);
		return true;
	}

	// log unexpected errors and return to the default game mode
	std::cerr << error_text << wsa_error << std::endl;
	PlayDefaultGame();
	closesocket(connecting_socket);
	return true;
}


/// <summary>
/// Handles entry into this game state
/// </summary>
void LockstepConnectingState_Init()
{
	// establish the timeout
	connecting_timeout_secs = 3.0f;

	// establish the description text
	connecting_text = "Connecting on ";
	connecting_text += std::to_string(connecting_port);
	connecting_text += ", waiting for response from host...";

	// create a UDP socket for connecting to a lockstep game host
	// -- note the pattern for calling the _HandleSocketError function... you'll want to use that pattern again for all other socket functions!
	connecting_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if ((connecting_socket == INVALID_SOCKET) &&
		LockstepConnectingState_HandleSocketError("Error creating lockstep connection socket: "))
	{
		return;
	}

	//TODO: make the socket non-blocking
	u_long non_blocking = 1;
	ioctlsocket(connecting_socket, FIONBIO, &non_blocking);


	//TODO: set the UDP socket to reference the specified port on the local machine (127.0.0.1)
	// -- which API lets you later use send/recv on a UDP socket?
	sockaddr_in local_address;
	local_address.sin_family = AF_INET;
	local_address.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    local_address.sin_port = htons(0);
	if (bind(connecting_socket, (sockaddr*)&local_address, sizeof(local_address)) == SOCKET_ERROR)
	{
        LockstepConnectingState_HandleSocketError("Error binding lockstep connection socket: ");
        return;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = htons(connecting_port);
	if (connect(connecting_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
	{
		LockstepConnectingState_HandleSocketError("Error connecting lockstep connection socket: ");
		return;
	}

	//TODO: send a buffer containing the word "Lockstep" to the server
    static const std::string message = "Lockstep";
    int sent = send(connecting_socket, message.c_str(), (int)(message.size() + 1), 0);
    if (sent == SOCKET_ERROR)
	{
        LockstepConnectingState_HandleSocketError("Error sending to lockstep connection socket: ");
        return;
    }

	std::cout << "Attempting to connect to a game server on port " << connecting_port
              << " sent=" << sent << std::endl;
}

/// <summary>
/// Per-frame update for this game state.
/// </summary>
void LockstepConnectingState_Update()
{
	// if ESC is pressed, "back up" to the configuring state
	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		ConfigureLockstep();
		closesocket(connecting_socket);
		return;
	}

	// reduce the timeout by CP_System_GetDt(), and if expired, give up and move on to Hosting Lockstep
	// -- in the current scenario, this will never really occur, as you'll get WSAECONNRESET right away from recv if there's nothing bound on that port locally
	// -- however, if the code was modified to support IPs, then you would likely not get that immediate error
	connecting_timeout_secs -= CP_System_GetDt();
	if (connecting_timeout_secs <= 0.0f)
	{
		std::cout << "Timeout waiting for a response from a game server on port " << connecting_port << ", attempting to host instead..." << std::endl;
		HostLockstep(connecting_port);
		closesocket(connecting_socket);
		return;
	}

	//TODO: attempt to receive a response from a hosting server
    char buffer[4096];
    sockaddr_in server_address;
    int server_address_size = sizeof(server_address);
    int res = recvfrom(connecting_socket, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &server_address_size);
    if (res == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            LockstepConnectingState_HandleSocketError(
                "Error receiving from lockstep connection socket: ");
        }
        return;
    }

	// if any bytes are received (don't bother to parse), then assume we're ready to play, and transition to PlayGame, re-using the socket, as non-host
	if (res > 0)
	{
		std::cout << "Received a response from a game server on port " << connecting_port << ", moving on to gameplay..." << std::endl;
		PlayGame(new LockstepGame(connecting_socket, false));
		return;
	}

	// clear the background
	CP_Settings_Background(CP_Color_Create(0, 0, 40, 255));

	// build the description
	auto description = connecting_text;
	description += std::to_string(connecting_timeout_secs);

	// draw the description
	CP_Settings_TextSize(30);
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_TOP);
	CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
	CP_Font_DrawText(description.c_str(), 0.0f, 0.0f);
	CP_Font_DrawText("ESC to go back", 0.0f, 40.0f);
}


/// <summary>
/// Handles departure from this game state.
/// </summary>
void LockstepConnectingState_Exit()
{
	// do not close the socket!
	// -- it might have been handed off to gameplay
	// -- all other exits should have closed the socket (i.e. HandleSocketError)
}


/// <summary>
/// Starts the user experience to attempt to connect to a lockstep-networked game.
/// </summary>
void ConnectLockstep(const u_short port)
{
	connecting_port = port;
	CP_Engine_SetNextGameStateForced(LockstepConnectingState_Init, LockstepConnectingState_Update, LockstepConnectingState_Exit);
}
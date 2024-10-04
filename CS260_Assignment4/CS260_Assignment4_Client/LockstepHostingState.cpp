//---------------------------------------------------------
// file:	LockstepHostingState.cpp
// author:	Matthew Picioccio, David Hedner
// email:	matthew.picioccio@digipen.edu, david.hedner@digipen.edu
//
// brief:	Global functions for the "Hosting Lockstep" game state
//
// Copyright © 2020 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "cprocessing.h"
#include "LockstepHostingState.h"
#include "LockstepConfiguringState.h"
#include "PlayGameState.h"
#include "LockstepGame.h"


u_short hosting_port;
SOCKET hosting_socket;
std::string hosting_text;


/// <summary>
/// Handles errors from socket functions, including transitioning to different game states.
/// </summary>
/// <param name="error_text">Text that describes the error.</param>
/// <remarks>WSAGetLastError will be included in the output by this function.</remarks>
/// <returns>True if the error is "fatal" and the calling function should stop; false if it can be ignored.</returns>
bool LockstepHostingState_HandleSocketError(const char* error_text)
{
	const auto wsa_error = WSAGetLastError();

	// ignore WSAEWOULDBLOCK
	if (wsa_error == WSAEWOULDBLOCK)
	{
		return false;
	}

	// log unexpected errors and return to the default game mode
	std::cerr << error_text << wsa_error << std::endl;
	PlayDefaultGame();
	closesocket(hosting_socket);
	return true;
}


/// <summary>
/// Handles entry into this game state
/// </summary>
void LockstepHostingState_Init()
{
	// establish the description text
	hosting_text = "Hosting on ";
	hosting_text += std::to_string(hosting_port);
	hosting_text += ", waiting for other player...";

	//create the UDP socket for hosting a lockstep game server
	// -- note the pattern from LockstepConnectingState.cpp... and make sure you're handling errors!
	// -- note the pattern for calling the _HandleSocketError function... you'll want to use that pattern again for all other socket functions!
	hosting_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if ((hosting_socket == INVALID_SOCKET) &&
		LockstepHostingState_HandleSocketError("Error creating lockstep hosting socket: "))
	{
		return;
	}
	
	//TODO: set the socket as non-blocking
    u_long non_blocking = 1;
	ioctlsocket(hosting_socket, FIONBIO, &non_blocking);
	
	//TODO: bind the hosting socket to the specified port on the local machine (127.0.0.1)
	sockaddr_in local_address;
	local_address.sin_family = AF_INET;
	local_address.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	local_address.sin_port = htons(hosting_port);

	if (bind(hosting_socket, (sockaddr*)&local_address, sizeof(local_address)) == SOCKET_ERROR)
	{
        LockstepHostingState_HandleSocketError("Error binding lockstep hosting socket: ");
        return;
    }

	std::cout << "Hosting a game server on port " << hosting_port << std::endl;
}


/// <summary>
/// Per-frame update for this game state.
/// </summary>
void LockstepHostingState_Update()
{
	// if ESC is pressed, "back up" to the configuring state
	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		closesocket(hosting_socket);
		ConfigureLockstep();
		return;
	}

	//TODO: attempt to receive a message from a connecting client
	// -- note that this must be recvfrom, since we don't know the identity of the other machine...
	char buffer[4096];
	sockaddr_in remote_address;
	int remote_address_size = sizeof(remote_address);
    int res = recvfrom(
        hosting_socket, buffer, sizeof(buffer), 0, (sockaddr*)&remote_address, &remote_address_size);
	 
	// if any bytes are received (don't bother to parse):
	if (res > 0)
	{
		std::cout << "Received a message from a potential player, acknowledging..." << std::endl;
		
		//TODO: set the hosting socket to reference the address the message was received from
		// -- the same API we used in LockstepConnectingState - the one that lets us use send/recv with a UDP socket...
		//TODO: send an acknowledgement message, "LetUsBegin"
		if (connect(hosting_socket, (sockaddr*)&remote_address, remote_address_size) == SOCKET_ERROR)
		{
			LockstepHostingState_HandleSocketError("Error connecting lockstep hosting socket: ");
			return;
		}

        static const std::string message = "LetUsBegin";
        if (send(hosting_socket, message.c_str(), (int)message.size() + 1, 0) == SOCKET_ERROR)
        {
            LockstepHostingState_HandleSocketError("Error sending to lockstep hosting socket: ");
            return;
        }
		
		// -- move on to lockstep gameplay, using the hosting socket, in host mode
		std::cout << "Successfully hosting a game on port " << hosting_port << " with another user, moving on to gameplay..." << std::endl;
		PlayGame(new LockstepGame(hosting_socket, true));
		return;
	}

	// clear the background
	CP_Settings_Background(CP_Color_Create(0, 0, 40, 255));

	// draw the description text
	CP_Settings_TextSize(30);
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_TOP);
	CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
	CP_Font_DrawText(hosting_text.c_str(), 0.0f, 0.0f);
	CP_Font_DrawText("ESC to go back", 0.0f, 40.0f);
}


/// <summary>
/// Handles departure from this game state.
/// </summary>
void LockstepHostingState_Exit()
{
	// do not close the socket!
	// -- it might have been handed off to gameplay
	// -- all other exits should close the socket (i.e. HandleSocketError)
}


/// <summary>
/// Starts the user experience to attempt to host a lockstep-networked game.
/// </summary>
void HostLockstep(const u_short port)
{
	hosting_port = port;
	CP_Engine_SetNextGameStateForced(LockstepHostingState_Init, LockstepHostingState_Update, LockstepHostingState_Exit);
}
/*****************************************************************
 * @file   main.cpp
 * @brief  Main function for the program
 * @author david.hedner@digipen.edu
 * @date   May 2024
 *
 * @copyright © 2024 DigiPen (USA) Corporation.
 *****************************************************************/

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <array>
#include <iostream>
#include <memory>
#pragma comment(lib, "Ws2_32.lib") // Can be added in the project settings instead

// Helper function to handle errors and print the error message and the error code
void HandleError(const std::string& errorMessage)
{
    std::cerr << errorMessage << ": " << WSAGetLastError() << std::endl;
}

// Class to handle the socket creation and cleanup
class Socket
{
public:
    // Constructor with WSA library initialization and socket creation
    Socket() : _sock(INVALID_SOCKET)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            HandleError("WSAStartup failed");
            throw std::runtime_error("WSAStartup failed");
        }

        _sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (_sock == INVALID_SOCKET)
        {
            HandleError("Socket creation failed");
            WSACleanup();
            throw std::runtime_error("Socket creation failed");
        }
    }

    // Destructor with cleanup and shutdown
    ~Socket()
    {
        if (_sock != INVALID_SOCKET)
        {
            closesocket(_sock);
        }
        WSACleanup();
    }

    SOCKET get() const
    {
        return _sock;
    }

private:
    SOCKET _sock;
};

int main()
{
    try
    {
        // Create a socket and set the address information
        Socket udpSocket;

        sockaddr_in destinationAddr = {};
        destinationAddr.sin_family = AF_INET;
        destinationAddr.sin_port = htons(8888);
        if (inet_pton(AF_INET, "52.12.14.56", &destinationAddr.sin_addr) != 1)
        {
            HandleError("inet_pton failed");
            return 1;
        }

        // Send a message to the server
        const char* message = "Hello, world!";
        int result = sendto(
            udpSocket.get(),
            message,
            (int)strlen(message),
            0,
            (const sockaddr*)&destinationAddr,
            sizeof(destinationAddr));
        if (result == SOCKET_ERROR)
        {
            HandleError("sendto failed");
            return 1;
        }

        // Recieve the data from the socket into a buffer
        std::array<char, 1500> receiveBuffer = {};
        result = recvfrom(
            udpSocket.get(), receiveBuffer.data(), receiveBuffer.size() - 1, 0, nullptr, nullptr);
        if (result == SOCKET_ERROR)
        {
            HandleError("recvfrom failed");
            return 1;
        }

        std::cout << "Received: " << receiveBuffer.data() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

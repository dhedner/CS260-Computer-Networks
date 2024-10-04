/*****************************************************************
 * @file   main.cpp
 * @brief  This is a basic TCP client that connects to a server 
 *         and sends a message. It uses non-blocking sockets to
 *         handle the connection.
 * @author david.hedner@digipen.edu
 * @date   June 2024
 *
 * @copyright © 2024 DigiPen (USA) Corporation.
 *****************************************************************/

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#pragma comment(lib, "Ws2_32.lib") // Can be added in the project settings instead

unsigned long nonBlocking = 1;
unsigned long blocking = 0;

// Helper function to handle errors and print the error message and the error code
void HandleError(const std::string& errorMessage)
{
    std::cerr << errorMessage << ": " << WSAGetLastError() << std::endl;
}

SOCKET CreateSocket(int protocol)
{
    return socket(AF_INET, protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM, protocol);
}

int SetAddress(const char* address, int port, sockaddr_in& addr)
{
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return inet_pton(AF_INET, address, &addr.sin_addr);
}

int HandleClient(SOCKET socket, const std::string &outgoing)
{
    if (send(socket, outgoing.data(), outgoing.size(), 0) == SOCKET_ERROR)
    {
        HandleError("send failed");
        return SOCKET_ERROR;
    }

    // Shutdown the sending side of the socket
    shutdown(socket, SD_SEND);

    std::array<char, 4096> buffer;
    int bytesReceived = 0;

    while (true)
    {
        bytesReceived = recv(socket, buffer.data(), (int)buffer.size(), 0);
        if (bytesReceived == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                std::cout << ". " << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else
            {
                HandleError("recv failed");
                return SOCKET_ERROR;
            }
        }
        else if (bytesReceived == 0)
        {
            std::cout << "Connection closed by the server" << std::endl;
            break;
        }
        else
        {
            buffer[bytesReceived] = '\0';
            std::cout << "Received: " << buffer.data() << std::endl;
        }
    }
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

        _sock = CreateSocket(IPPROTO_TCP);
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

int main(int argc, char* argv[])
{
    if (argc < 1)
    {
        std::cerr << "Program name is missing in argv[0]" << std::endl;
        return -1;
    }

    std::string programName = argv[0];

    try
    {
        // Create a socket and set the address information
        Socket tcpSocket;

        sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(sockaddr_in));

        // Set the server address
        if (SetAddress("52.12.14.56", 8888, serverAddr) == SOCKET_ERROR)
        {
            return -1;
        }

        // Connect the socket to the address
        if (connect(tcpSocket.get(), (sockaddr*)&serverAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
        {
            HandleError("Connect failed");
            return -1;
        }

        ioctlsocket(tcpSocket.get(), FIONBIO, &nonBlocking);

        // Send a message to the server
        if (HandleClient(tcpSocket.get(), programName) == SOCKET_ERROR)
        {
            return -1;
        }

        // Shutdown connection and close the socket
        shutdown(tcpSocket.get(), SD_BOTH);
        closesocket(tcpSocket.get());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/*****************************************************************
 * @file   main.cpp
 * @brief  This is a HTTP proxy server that can handle multiple
 * clients concurrently.
 * @author david.hedner@digipen.edu
 * @date   July 2024
 *
 * @copyright � 2024 DigiPen (USA) Corporation.
 *****************************************************************/

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

int SetAddress(const char* address, int port, sockaddr_in& addr, bool useAny = false)
{
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (useAny)
    {
        addr.sin_addr.s_addr = INADDR_ANY;
        return 1;
    }
    return inet_pton(AF_INET, address, &addr.sin_addr);
}

std::string ReceiveData(SOCKET socket)
{
    std::vector<char> buffer(4096);
    std::ostringstream oss;
    int bytesReceived = 0;

    // Receive the entire HTTP request from the client
    while (true)
    {
        bytesReceived = recv(socket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0);
        if (bytesReceived == SOCKET_ERROR)
        {
            // If the socket is non-blocking and there is no data to receive, sleep for a bit
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else
            {
                HandleError("recv failed");
                return "";
            }
        }
        else if (bytesReceived == 0)
        {
            break;
        }
        else
        {
            buffer[bytesReceived] = '\0'; // Null-terminate the received data
            oss << buffer.data();
        }
    }

    return oss.str();
}

std::string GetHostFromRequest(const std::string& request)
{
    std::istringstream iss(request);
    std::string line;
    std::string host;

    while (std::getline(iss, line))
    {
        if (line.find("Host: ") == 0)
        {
            host = line.substr(6);
            // Remove the carriage return at the end of the host
            if (!host.empty() && host[host.size() - 1] == '\r')
            {
                host.pop_back();
            }
            break;
        }
    }

    return host;
}

void HandleClient(SOCKET clientSocket)
{
    // Receive the HTTP request from the client
    std::string request = ReceiveData(clientSocket);

    // Shutdown the client socket for receiving
    shutdown(clientSocket, SD_RECEIVE);

    // Parse the HTTP request to get the host
    std::string host = GetHostFromRequest(request);
    if (host.empty())
    {
        HandleError("Host header not found in the request");
        return;
    }

    // Resolve the host name to an IP address using inet_pton first, then fallback to getaddrinfo
    sockaddr_in webServerAddr;
    memset(&webServerAddr, 0, sizeof(sockaddr_in));
    int result = SetAddress(host.c_str(), 80, webServerAddr);
    if (result == 0) // SetAddress failed, try getaddrinfo
    {
        addrinfo* result = nullptr;
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(host.c_str(), "80", &hints, &result) != 0)
        {
            HandleError("getaddrinfo failed");
            return;
        }
        memcpy(&webServerAddr, result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result);
    }
    else if (result == -1)
    {
        HandleError("inet_pton failed");
        return;
    }

    // Create a socket to connect to the web server
    SOCKET webServerSocket = CreateSocket(IPPROTO_TCP);
    if (webServerSocket == INVALID_SOCKET)
    {
        HandleError("Web server socket creation failed");
        return;
    }

    // Connect to the web server
    if (connect(
            webServerSocket,
            reinterpret_cast<sockaddr*>(&webServerAddr), // reinterpret_cast is needed because
                                                         // sockaddr_in is not the same as sockaddr
            sizeof(sockaddr_in)) == SOCKET_ERROR)
    {
        HandleError("Connect to web server failed");
        return;
    }

    // Send the entire HTTP request to the web server
    if (send(webServerSocket, request.c_str(), static_cast<int>(request.size()), 0) ==
        SOCKET_ERROR)
    {
        HandleError("Send to web server failed");
        return;
    }

    // Forward the response from the web server to the client
    std::vector<char> buffer(4096);
    int bytesReceived = 0;

    while (true)
    {
        bytesReceived =
            recv(webServerSocket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0);
        if (bytesReceived == SOCKET_ERROR)
        {
            // If the socket is non-blocking and there is no data to receive, sleep for a bit
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else
            {
                HandleError("recv from web server failed");
                break;
            }
        }
        else if (bytesReceived == 0)
        {
            break;
        }
        else
        {
            send(clientSocket, buffer.data(), bytesReceived, 0);
        }
    }

    // Close sockets
    shutdown(webServerSocket, SD_BOTH);
    closesocket(webServerSocket);
    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);
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

    void set(SOCKET sock)
    {
        _sock = sock;
    }

    void setNonBlocking()
    {
        ioctlsocket(_sock, FIONBIO, &nonBlocking);
    }

private:
    SOCKET _sock;
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Invalid port number. Port must be between 1 and 65535." << std::endl;
        return 1;
    }

    try
    {
        // Create a socket to listen for incoming connections
        Socket listenSocket;
        if (listenSocket.get() == INVALID_SOCKET)
        {
            HandleError("Listen socket creation failed");
            return 1;
        }
        
        // Construct the listening address
        sockaddr_in listeningAddr;
        if (SetAddress("0.0.0.0", port, listeningAddr, true) == SOCKET_ERROR)
        {
            HandleError("SetAddress failed");
            return 1;
        }

        // Set the listening socket to non-blocking
        listenSocket.setNonBlocking();

        // Bind the listening socket to the listening address
        if (bind(
                listenSocket.get(),
                reinterpret_cast<sockaddr*>(&listeningAddr),
                sizeof(listeningAddr)) == SOCKET_ERROR)
        {
            HandleError("bind failed");
            return 1;
        }

        // Start listening for incoming connections on the TCP socket
        if (listen(listenSocket.get(), SOMAXCONN) == SOCKET_ERROR)
        {
            HandleError("listen failed");
            return 1;
        }

        std::cout << "Listening on port " << port << std::endl;

        // Infinite loop to accept incoming connections
        while (true)
        {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(
                listenSocket.get(), reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);
            if (clientSocket == INVALID_SOCKET)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Accepted socket can become "blocking" again
            ioctlsocket(clientSocket, FIONBIO, &blocking);

            // Create a new thread to handle the client
            std::thread clientThread = std::thread(HandleClient, clientSocket);
            clientThread.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
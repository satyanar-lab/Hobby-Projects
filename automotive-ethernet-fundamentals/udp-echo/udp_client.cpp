#include <iostream>
#include <cstdint>
#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cerrno>

int main()
{

	//create socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd<0)
	{
		std::cerr << "socket() failed: " << strerror(errno) << std::endl;
		return 1;
	}

	//build destination address
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(5000);

	if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) 
	{
    		std::cerr << "inet_pton() failed for 127.0.0.1" << std::endl;
    		close(sockfd);
    		return 1;
	}

	//send message
	const char* message = "hello from client";
	ssize_t bytes_sent = sendto
		(sockfd,
		 message,
		 strlen(message),
		 0,
		 (struct sockaddr*)&server_addr, 
		 sizeof(server_addr)
		 );

	if(bytes_sent <0)
	{
		std::cerr << "sendto failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return 1;
	}

	//receive reply
	char buffer[1024];
	sockaddr_in from_addr;
	socklen_t from_addr_len = sizeof(from_addr);

	ssize_t bytes_received = recvfrom
		(
		 sockfd,
		 buffer,
		 sizeof(buffer)-1,
		 0,
		 (struct sockaddr*)&from_addr, 
		 &from_addr_len
		);

	if(bytes_received < 0)
	{
		std::cerr << "recvfrom() failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return 1;
	}

	buffer[bytes_received] = '\0';

	//print the reply
	std::cout << "Received reply: \""  << buffer <<  "\" (" << bytes_received <<  " bytes)" << std::endl;

	//clean-up
	close(sockfd);
	return 0;
}




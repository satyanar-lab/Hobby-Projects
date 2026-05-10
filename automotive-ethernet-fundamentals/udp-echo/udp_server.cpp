#include <iostream>     	// for std::cout
#include <cstdint> 		// for uint16_t
#include <cstring> 		// for memset()
#include <sys/socket.h>	        // for socket(), bind()
#include <netinet/in.h>	 	//for sockaddr_in, AF_INET
#include <arpa/inet.h>	 	// for htons(), htonl()
#include <unistd.h> 		// for close()
#include <cerrno>		// for errno


int main()
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	char buffer[1024];
	sockaddr_in server_addr;
	sockaddr_in client_addr;
	char client_ip[INET_ADDRSTRLEN];
	socklen_t client_addr_len = sizeof(client_addr);

	if (sockfd < 0)
	{
		std::cerr << "socket() failed: " << strerror(errno) << std::endl;
		return 1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;       // IPv4
	server_addr.sin_port = htons(5000);	// port address in network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY;	// bind to all interfaces (0.0.0.0)
	
	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		std::cerr << "bind() failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return 1;
	}
	
	std::cout << "Server bound to port 5000, waiting for messages..." << std::endl;

	while(true)
	{
	ssize_t bytes_received = recvfrom
	       	(
			sockfd,			//socket
			buffer,			//where to put the data
			sizeof(buffer)-1,	//max bytes to receive
			0,			//flags - none
			(struct sockaddr*)&client_addr, 		//who sent it
			&client_addr_len	//size of that struct
		);
	if(bytes_received < 0)
	{
		std::cerr << "recvfrom() failed: " << strerror(errno) << std::endl;
	        continue;
	}
	buffer[bytes_received] = '\0';
	
	inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

	std::cout << "Received " << bytes_received << " bytes from "
		<< client_ip << " : " << ntohs(client_addr.sin_port)
		<< " -\"" << buffer << "\"" << std::endl;

	ssize_t bytes_sent = sendto
		(
		 sockfd,
		 buffer,
		 bytes_received,
		 0,
		 (struct sockaddr*)&client_addr,
		 client_addr_len
		);
	
	if(bytes_sent < 0)
	{
		std::cerr << "sendto() failed: " << strerror(errno) << std::endl;
	}
	
	}

	close(sockfd);
	return 0;
}


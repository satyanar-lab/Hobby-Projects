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

	if (sockfd < 0)
	{
		std::cerr << "socket() failed: " << strerror(errno) << std::endl;
		return 1;
	}

	sockaddr_in server_addr;
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
	
	std::cout << "Server bound to port 5000" << std::endl;
	close(sockfd);

	return 0;
}


#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cerrno>

constexpr uint8_t  UDS_SID_READ_DATA_BY_IDENTIFIER	 = 0x22;
constexpr uint8_t  UDS_SID_READ_DATA_BY_IDENTIFIER_POS   = 0x62;  // = 0x22 + 0x40
constexpr uint8_t  UDS_NEGATIVE_RESPONSE		 = 0x7F;
constexpr uint16_t UDS_DID_VIN				 = 0xF190;
constexpr uint16_t UDS_SERVER_PORT			 = 13400;
constexpr size_t   UDS_REQUEST_SIZE			 = 3;

int main()
{

	
	// create socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (sockfd < 0)
	{
		std::cerr << "socket() failed: " << strerror(errno) << std::endl;
		return 1;
	}

	// build destination address
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(13400);

	if(inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
	{
		std::cerr << "inet_pton() failed: " << strerror(errno) << std::endl;
		return 1;
	}

	// Construct the UDS request: [SID] [DID high] [DID low]
	uint8_t request[UDS_REQUEST_SIZE];
	
	request[0] = UDS_SID_READ_DATA_BY_IDENTIFIER;	// 0x22
	request[1] = (UDS_DID_VIN >> 8) & 0xFF;		// 0xF1
	request[2] = (UDS_DID_VIN & 0xFF);		// 0x90
	

        ssize_t bytes_sent = sendto
                (sockfd,
                 request,
                 UDS_REQUEST_SIZE,
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
        uint8_t buffer[1024];
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

        // Validate response is at least 16 bytes (SOME/IP header size)
        if (bytes_received < 1) {
        std::cerr << "Response too short for any UDS reply ("
              << bytes_received << " bytes)" << std::endl;
        close(sockfd);
        return 1;
        }
	
	uint8_t first_byte = buffer[0];

	if (first_byte == UDS_SID_READ_DATA_BY_IDENTIFIER_POS)
	{
		// Postive response: 0x62 [DID high] [DID low] [data...]
		if (bytes_received < 3)
		{
			std::cerr << "Positive response too short" << std::endl;
			close(sockfd);
			return 1;
		}

		uint16_t did = (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2];
		std::printf("Positive response (0x62) for DID 0x%04X\n", did);
		std::cout << "Data (" << (bytes_received - 3) << " bytes): ";

		for(ssize_t i = 3; i < bytes_received; ++i)
		{
			std::printf("%02X ", buffer[i]);
		}
		std::cout << std::endl;

		// Print as ASCII 
		std::cout << "As ASCII: \"";
		for (ssize_t i =3; i < bytes_received; ++i)
		{
			std::cout << static_cast<char>(buffer[i]);
		}
		std::cout << "\"" << std::endl;
	}
	else if (first_byte == UDS_NEGATIVE_RESPONSE && bytes_received >= 3)
	{
		// Negative response: 0x7F [rejected SID] [NRC]
		std::printf("Negative response: rejected SID 0x%02X, NRC 0x%02X\n", buffer[1], buffer[2]);
	}
	else
	{
		std::printf("Unexpected response, first byte 0x%02X\n", first_byte);
	}

	close(sockfd);
	return 0;
}

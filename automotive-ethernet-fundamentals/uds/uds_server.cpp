#include <iostream>             // for std::cout
#include <cstdint>              // for uint16_t
#include <cstring>              // for memset()
#include <sys/socket.h>         // for socket(), bind()
#include <netinet/in.h>         //for sockaddr_in, AF_INET
#include <arpa/inet.h>          // for htons(), htonl()
#include <unistd.h>             // for close()
#include <cerrno>               // for errno

constexpr uint8_t  UDS_SID_READ_DATA_BY_IDENTIFIER       = 0x22;
constexpr uint8_t  UDS_SID_READ_DATA_BY_IDENTIFIER_POS   = 0x62;  // = 0x22 + 0x40
constexpr uint8_t  UDS_NEGATIVE_RESPONSE                 = 0x7F;
constexpr uint16_t UDS_DID_VIN                           = 0xF190;
constexpr uint16_t UDS_SERVER_PORT                       = 13400;

constexpr uint8_t  NRC_SERVICE_NOT_SUPPORTED             = 0x11;
constexpr uint8_t  NRC_INCORRECT_MESSAGE_LENGTH          = 0x13;
constexpr uint8_t  NRC_REQUEST_OUT_OF_RANGE              = 0x31;

const char VIN[] = "VIN1234567890ABCD";
constexpr size_t VIN_LENGTH = 17;


int main()
{
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        if (sockfd < 0)
        {
                std::cerr << "socket() failed: " << strerror(errno) << std::endl;
                return 1;
        }

        uint8_t buffer[1024];
        sockaddr_in server_addr;
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = AF_INET;       // IPv4
        server_addr.sin_port = htons(UDS_SERVER_PORT);     // port address in network byte order
        server_addr.sin_addr.s_addr = INADDR_ANY;       // bind to all interfaces (0.0.0.0)

        if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        {
                std::cerr << "bind() failed: " << strerror(errno) << std::endl;
                close(sockfd);
                return 1;
        }

        std::cout << "Server listening on port 13400..." << std::endl;

        while(true)
        {
        ssize_t bytes_received = recvfrom
                (
                        sockfd,                 //socket
                        buffer,                 //where to put the data
                        sizeof(buffer)-1,       //max bytes to receive
                        0,                      //flags - none
                        (struct sockaddr*)&client_addr,                 //who sent it
                        &client_addr_len        //size of that struct
                );
	if(bytes_received < 3)
	{
		// Negative response: 0x13 [SID] [NRC]
		uint8_t response[3];
    		response[0] = UDS_NEGATIVE_RESPONSE;                                  // 0x7F
    		response[1] = (bytes_received >= 1) ? buffer[0] : 0x00;               // echo SID if we got one
    		response[2] = NRC_INCORRECT_MESSAGE_LENGTH;                           // 0x13
    		sendto(sockfd, response, 3, 0, (struct sockaddr*)&client_addr, client_addr_len);
    		continue;
	}

	uint8_t request_sid = buffer[0];

	// response buffer
	uint8_t response[256];
	size_t response_size = 0;

	if (request_sid == UDS_SID_READ_DATA_BY_IDENTIFIER)
	{
		uint16_t did = (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2];
		if (did == UDS_DID_VIN)
		{
			// Positive response
			response[0] = 0x62;
			response[1] = 0xF1;
			response[2] = 0x90;
			memcpy(&response[3], VIN, VIN_LENGTH);
			response_size = 3 + VIN_LENGTH;
		}
		else
		{
			// NRC_REQUEST_OUT_OF_RANGE
          		response[0] = 0x7F;
          		response[1] = request_sid;
          		response[2] = NRC_REQUEST_OUT_OF_RANGE;
          		response_size = 3;
		}
	}
	else
	{
		// NRC_SERVICE_NOT_SUPPORTED
		response[0] = 0x7F;
        	response[1] = request_sid;
        	response[2] = NRC_SERVICE_NOT_SUPPORTED;
       		response_size = 3;
	}

	// Send the response
        ssize_t bytes_sent = sendto(
                sockfd,
                response,
                response_size,
                0,
                (struct sockaddr*)&client_addr,
                client_addr_len
                );
        if (bytes_sent < 0) {
                std::cerr << "sendto() failed: " << strerror(errno) << std::endl;
        }

        }

        close(sockfd);
        return 0;
}


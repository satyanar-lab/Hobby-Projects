#include <iostream>             // for std::cout
#include <cstdint>              // for uint16_t
#include <cstdio>		// for printf
#include <cstring>              // for memset()
#include <sys/socket.h>         // for socket(), bind()
#include <netinet/in.h>         //for sockaddr_in, AF_INET
#include <arpa/inet.h>          // for htons(), htonl()
#include <unistd.h>             // for close()
#include <cerrno>               // for errno

// SOME/IP protocol constants
constexpr uint16_t SERVICE_ID_EXTERIOR_LIGHTING  = 0x5100;
constexpr uint16_t METHOD_ID_SET_LAMP_COMMAND    = 0x0001;
constexpr uint16_t CLIENT_ID                     = 0x0001;
constexpr uint16_t SESSION_ID                    = 0x0001;
constexpr uint8_t  PROTOCOL_VERSION              = 0x01;
constexpr uint8_t  INTERFACE_VERSION             = 0x01;
constexpr uint8_t  MESSAGE_TYPE_REQUEST          = 0x00;
constexpr uint8_t  RETURN_CODE_E_OK              = 0x00;

// Payload values
constexpr uint8_t  LAMP_FUNCTION_LEFT_INDICATOR  = 0x01;
constexpr uint8_t  LAMP_STATE_ON                 = 0x01;
constexpr uint8_t  LAMP_INTENSITY_FULL           = 0x64;  // 100 = 100%


int main()
{
	// Construct a SOME/IP message: 16-byte header + 4-byte payload = 20 bytes
	constexpr size_t MESSAGE_SIZE = 20;
	constexpr uint32_t LENGTH_FIELD_VALUE = 12;  // 8 bytes of header tail + 4 bytes payload

	uint8_t message[MESSAGE_SIZE];

	// Service ID (offset 0, 2 bytes, big-endian)
	message[0] = (SERVICE_ID_EXTERIOR_LIGHTING >> 8) & 0xFF;
	message[1] = SERVICE_ID_EXTERIOR_LIGHTING & 0xFF;

	// Method ID (offset 2, 2 bytes, big-endian)
	message[2] = (METHOD_ID_SET_LAMP_COMMAND >> 8) & 0xFF;
	message[3] = METHOD_ID_SET_LAMP_COMMAND & 0xFF;

	// Length (offset 4, 4 bytes, big-endian)
	message[4] = (LENGTH_FIELD_VALUE >> 24) & 0xFF;
	message[5] = (LENGTH_FIELD_VALUE >> 16) & 0xFF;
	message[6] = (LENGTH_FIELD_VALUE >> 8) & 0xFF;
	message[7] = LENGTH_FIELD_VALUE & 0xFF;

	// Client ID (offset 8, 2 bytes, big-endian)
	message[8] = (CLIENT_ID >> 8) & 0xFF;
	message[9] = CLIENT_ID & 0xFF;

	// Session ID (offset 10, 2 bytes, big-endian)
	message[10] = (SESSION_ID >> 8) & 0xFF;
	message[11] = SESSION_ID & 0xFF;

	// Single-byte fields (offset 12-15)
	message[12] = PROTOCOL_VERSION;
	message[13] = INTERFACE_VERSION;
	message[14] = MESSAGE_TYPE_REQUEST;
	message[15] = RETURN_CODE_E_OK;	

	// Payload (offset 16-19): LampCommand
	message[16] = LAMP_FUNCTION_LEFT_INDICATOR;
	message[17] = LAMP_STATE_ON;
	message[18] = LAMP_INTENSITY_FULL;
	message[19] = 0x00;  // reserved	
	

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
	std::cout << "Sending " << MESSAGE_SIZE << " bytes: ";
	for (size_t i = 0; i < MESSAGE_SIZE; ++i)
	{
		std::printf("%02X ", message[i]);
	}
	std::cout << std::endl;

        ssize_t bytes_sent = sendto
                (sockfd,
                 message,
                 MESSAGE_SIZE,
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
        std::cout << "Received reply: \"" << buffer << "\" (" << bytes_received <<  " bytes)" << std::endl;
        //clean-up
        close(sockfd);
        return 0;
}

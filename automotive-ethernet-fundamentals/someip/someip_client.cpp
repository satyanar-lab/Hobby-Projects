#include <iostream>             // for std::cout
#include <cstdint>              // for uint16_t
#include <cstdio>		// for printf
#include <cstring>              // for memset()
#include <sys/socket.h>         // for socket(), bind()
#include <netinet/in.h>         //for sockaddr_in, AF_INET
#include <arpa/inet.h>          // for htons(), htonl()
#include <unistd.h>             // for close()
#include <cerrno>               // for errno

#include "../common/byte_order.hpp"

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
	write_u16_be(message, 0, SERVICE_ID_EXTERIOR_LIGHTING);

	// Method ID (offset 2, 2 bytes, big-endian)
	write_u16_be(message, 2, METHOD_ID_SET_LAMP_COMMAND);

	// Length (offset 4, 4 bytes, big-endian)
	write_u32_be(message, 4, LENGTH_FIELD_VALUE);

	// Client ID (offset 8, 2 bytes, big-endian)
	write_u16_be(message, 8, CLIENT_ID);

	// Session ID (offset 10, 2 bytes, big-endian)
	write_u16_be(message, 10, SESSION_ID);

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
	if (bytes_received < 16) {
    	std::cerr << "Response too short for SOME/IP header ("
              << bytes_received << " bytes)" << std::endl;
    	close(sockfd);
    	return 1;
	}	

	// Parse the SOME/IP RESPONSE header
	uint16_t resp_service_id        = read_u16_be(buffer, 0);
	uint16_t resp_method_id         = read_u16_be(buffer, 2);
	uint32_t resp_length            = read_u32_be(buffer, 4);
	uint16_t resp_client_id         = read_u16_be(buffer, 8);
	uint16_t resp_session_id        = read_u16_be(buffer, 10);
	uint8_t  resp_protocol_version  = buffer[12];
	uint8_t  resp_interface_version = buffer[13];
	uint8_t  resp_message_type      = buffer[14];
	uint8_t  resp_return_code       = buffer[15];

	// Print the parsed response
	std::cout << "Received " << bytes_received << " bytes:" << std::endl;
	std::printf("  Service ID:        0x%04X\n", resp_service_id);
	std::printf("  Method ID:         0x%04X\n", resp_method_id);
	std::printf("  Length:            %u\n", resp_length);
	std::printf("  Client ID:         0x%04X\n", resp_client_id);
	std::printf("  Session ID:        0x%04X\n", resp_session_id);
	std::printf("  Protocol Version:  0x%02X\n", resp_protocol_version);
	std::printf("  Interface Version: 0x%02X\n", resp_interface_version);
	std::printf("  Message Type:      0x%02X", resp_message_type);
	
	if (resp_message_type == 0x80) {
    		std::cout << " (RESPONSE)";
	} 
	
	else if (resp_message_type == 0x81) {
    		std::cout << " (ERROR)";
	}
	std::cout << std::endl;
	std::printf("  Return Code:       0x%02X\n", resp_return_code);

	// Parse and print response payload
	ssize_t resp_payload_size = bytes_received - 16;
	
	if (resp_payload_size > 0) {
    		std::cout << "  Payload (" << resp_payload_size << " bytes): ";
    	
		for (ssize_t i = 0; i < resp_payload_size; ++i) {
        		std::printf("%02X ", buffer[16 + i]);
    		}	
    	std::cout << std::endl;

    	// Decode the response payload specifically
    	if (resp_payload_size >= 4) {
        	std::printf("  Status:            0x%02X", buffer[16]);
        	if (buffer[16] == 0x01) std::cout << " (COMMAND ACCEPTED)";
        		std::cout << std::endl;
        		std::printf("  Lamp function:     0x%02X (echoed)\n", buffer[17]);
        		std::printf("  Lamp state:        0x%02X (echoed)\n", buffer[18]);
        		std::printf("  Sequence counter:  %u\n", buffer[19]);
    			}
	}			
        
	//clean-up
        close(sockfd);
        return 0;
}

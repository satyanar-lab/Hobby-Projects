#include <iostream>     	// for std::cout
#include <cstdint> 		// for uint16_t
#include <cstring> 		// for memset()
#include <sys/socket.h>	        // for socket(), bind()
#include <netinet/in.h>	 	//for sockaddr_in, AF_INET
#include <arpa/inet.h>	 	// for htons(), htonl()
#include <unistd.h> 		// for close()
#include <cerrno>		// for errno
				//
#include "../common/byte_order.hpp"

constexpr uint8_t MESSAGE_TYPE_RESPONSE = 0x80;
constexpr uint8_t STATUS_COMMAND_ACCEPTED = 0x01;

// Module-level state - tracks how many requests we've handled.
// In a real ECU this would be per-session state.
static uint8_t response_sequence_counter = 0;


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

	// Validate we have at least a SOME/IP header
	if (bytes_received < 16) {
    	std::cerr << "Datagram too short for SOME/IP header (" 
              << bytes_received << " bytes)" << std::endl;
    	continue;
	}	

	// Parse SOME/IP header fields
	uint16_t service_id        = read_u16_be(buffer, 0);
	uint16_t method_id         = read_u16_be(buffer, 2);
	uint32_t length            = read_u32_be(buffer, 4);
	uint16_t client_id         = read_u16_be(buffer, 8);
	uint16_t session_id        = read_u16_be(buffer, 10);
	uint8_t  protocol_version  = buffer[12];
	uint8_t  interface_version = buffer[13];
	uint8_t  message_type      = buffer[14];
	uint8_t  return_code       = buffer[15];

	// Print client address and SOME/IP fields
	char client_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
	std::cout << "Received " << bytes_received << " bytes from "
          << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
	std::printf("  Service ID:        0x%04X\n", service_id);
	std::printf("  Method ID:         0x%04X\n", method_id);
	std::printf("  Length:            %u\n", length);
	std::printf("  Client ID:         0x%04X\n", client_id);
	std::printf("  Session ID:        0x%04X\n", session_id);
	std::printf("  Protocol Version:  0x%02X\n", protocol_version);
	std::printf("  Interface Version: 0x%02X\n", interface_version);
	std::printf("  Message Type:      0x%02X\n", message_type);
	std::printf("  Return Code:       0x%02X\n", return_code);

	// Print payload as hex
	ssize_t payload_size = bytes_received - 16;
	
	if (payload_size > 0) {
    	std::cout << "  Payload (" << payload_size << " bytes): ";
    	
	for (ssize_t i = 0; i < payload_size; ++i) {
        std::printf("%02X ", buffer[16 + i]);
   	}
   	std::cout << std::endl;
	} 
	
	else {
    	std::cout << "  Payload: (empty)" << std::endl;
	}

	
	
	// Build a SOME/IP RESPONSE message (16-byte header + 4-byte payload)
	constexpr size_t RESPONSE_SIZE = 20;
	constexpr uint32_t RESPONSE_LENGTH_FIELD = 12;  // 8 header tail + 4 payload

	uint8_t response[RESPONSE_SIZE];

	// Service ID — same as request (offset 0, 2 bytes, big-endian)
	write_u16_be(response, 0, service_id);

	// Method ID — same as request
	write_u16_be(response, 2, method_id);

	// Length (offset 4, 4 bytes, big-endian)
	write_u32_be(response, 4, RESPONSE_LENGTH_FIELD);

	// Client ID — same as request
	write_u16_be(response, 8, client_id);

	// Session ID — same as request
	write_u16_be(response, 10, session_id);

	// Single-byte header fields — this is where it differs from a REQUEST
	response[12] = 0x01;                       // Protocol Version
	response[13] = 0x01;                       // Interface Version
	response[14] = MESSAGE_TYPE_RESPONSE;      // 0x80 — for response
	response[15] = 0x00;                       // Return Code E_OK

	// Payload — status + echoes + sequence counter
	response[16] = STATUS_COMMAND_ACCEPTED;
	response[17] = buffer[16];                 // echo back the lamp function
	response[18] = buffer[17];                 // echo back the lamp state
	response[19] = response_sequence_counter;
	response_sequence_counter++;

	// Send the response
	ssize_t bytes_sent = sendto(
    		sockfd,
    		response,
    		RESPONSE_SIZE,
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


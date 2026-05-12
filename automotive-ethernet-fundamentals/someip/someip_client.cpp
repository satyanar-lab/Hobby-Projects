#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "someip_codec.hpp"

// Application-layer constants for the request.
constexpr uint16_t SERVICE_ID_EXTERIOR_LIGHTING  = 0x5100;
constexpr uint16_t METHOD_ID_SET_LAMP_COMMAND    = 0x0001;
constexpr uint16_t CLIENT_ID                     = 0x0001;
constexpr uint16_t SESSION_ID                    = 0x0001;

// Payload values for the LampCommand.
constexpr uint8_t LAMP_FUNCTION_LEFT_INDICATOR = 0x01;
constexpr uint8_t LAMP_STATE_ON                = 0x01;
constexpr uint8_t LAMP_INTENSITY_FULL          = 0x64;  // 100 %

int main() {
    // Build the SOME/IP REQUEST: 16-byte header + 4-byte payload.
    constexpr size_t   MESSAGE_SIZE       = 20;
    constexpr uint32_t LENGTH_FIELD_VALUE = 12;

    SomeIpHeader request_hdr{};
    request_hdr.service_id        = SERVICE_ID_EXTERIOR_LIGHTING;
    request_hdr.method_id         = METHOD_ID_SET_LAMP_COMMAND;
    request_hdr.length            = LENGTH_FIELD_VALUE;
    request_hdr.client_id         = CLIENT_ID;
    request_hdr.session_id        = SESSION_ID;
    request_hdr.protocol_version  = 0x01;
    request_hdr.interface_version = 0x01;
    request_hdr.message_type      = SOMEIP_MSG_TYPE_REQUEST;
    request_hdr.return_code       = SOMEIP_RETURN_CODE_E_OK;

    uint8_t message[MESSAGE_SIZE];
    write_someip_header(message, request_hdr);

    // Payload: LampCommand.
    message[16] = LAMP_FUNCTION_LEFT_INDICATOR;
    message[17] = LAMP_STATE_ON;
    message[18] = LAMP_INTENSITY_FULL;
    message[19] = 0x00;  // reserved

    // Create UDP socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // Build destination address: 127.0.0.1:5000.
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(5000);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "inet_pton() failed for 127.0.0.1" << std::endl;
        close(sockfd);
        return 1;
    }

    // Send the message.
    std::cout << "Sending " << MESSAGE_SIZE << " bytes: ";
    for (size_t i = 0; i < MESSAGE_SIZE; ++i) {
        std::printf("%02X ", message[i]);
    }
    std::cout << std::endl;

    ssize_t bytes_sent = sendto(
        sockfd, message, MESSAGE_SIZE, 0,
        (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bytes_sent < 0) {
        std::cerr << "sendto failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }

    // Receive the reply.
    uint8_t buffer[1024];
    sockaddr_in from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    ssize_t bytes_received = recvfrom(
        sockfd, buffer, sizeof(buffer) - 1, 0,
        (struct sockaddr*)&from_addr, &from_addr_len);
    if (bytes_received < 0) {
        std::cerr << "recvfrom() failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }

    // Parse the SOME/IP RESPONSE header.
    SomeIpHeader response_hdr;
    if (!parse_someip_header(buffer, bytes_received, response_hdr)) {
        std::cerr << "Response too short for SOME/IP header ("
                  << bytes_received << " bytes)" << std::endl;
        close(sockfd);
        return 1;
    }

    // Print parsed fields.
    std::cout << "Received " << bytes_received << " bytes:" << std::endl;
    std::printf("  Service ID:        0x%04X\n", response_hdr.service_id);
    std::printf("  Method ID:         0x%04X\n", response_hdr.method_id);
    std::printf("  Length:            %u\n", response_hdr.length);
    std::printf("  Client ID:         0x%04X\n", response_hdr.client_id);
    std::printf("  Session ID:        0x%04X\n", response_hdr.session_id);
    std::printf("  Protocol Version:  0x%02X\n", response_hdr.protocol_version);
    std::printf("  Interface Version: 0x%02X\n", response_hdr.interface_version);
    std::printf("  Message Type:      0x%02X", response_hdr.message_type);
    if (response_hdr.message_type == SOMEIP_MSG_TYPE_RESPONSE) {
        std::cout << " (RESPONSE)";
    } else if (response_hdr.message_type == SOMEIP_MSG_TYPE_ERROR) {
        std::cout << " (ERROR)";
    }
    std::cout << std::endl;
    std::printf("  Return Code:       0x%02X\n", response_hdr.return_code);

    // Print payload as hex and decode semantically.
    ssize_t resp_payload_size = bytes_received - SOMEIP_HEADER_SIZE;
    if (resp_payload_size > 0) {
        std::cout << "  Payload (" << resp_payload_size << " bytes): ";
        for (ssize_t i = 0; i < resp_payload_size; ++i) {
            std::printf("%02X ", buffer[SOMEIP_HEADER_SIZE + i]);
        }
        std::cout << std::endl;

        if (resp_payload_size >= 4) {
            std::printf("  Status:            0x%02X", buffer[16]);
            if (buffer[16] == 0x01) std::cout << " (COMMAND ACCEPTED)";
            std::cout << std::endl;
            std::printf("  Lamp function:     0x%02X (echoed)\n", buffer[17]);
            std::printf("  Lamp state:        0x%02X (echoed)\n", buffer[18]);
            std::printf("  Sequence counter:  %u\n", buffer[19]);
        }
    }

    close(sockfd);
    return 0;
}

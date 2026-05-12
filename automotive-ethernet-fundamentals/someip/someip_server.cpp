#include <iostream>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "someip_codec.hpp"

// Application-layer constants for the response payload.
constexpr uint8_t STATUS_COMMAND_ACCEPTED = 0x01;

// Module-level state: sequence counter persists across requests.
// In a real ECU this would be per-session state in a session map.
static uint8_t response_sequence_counter = 0;

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    uint8_t buffer[1024];
    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(5000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Server bound to port 5000, waiting for messages..." << std::endl;

    while (true) {
        ssize_t bytes_received = recvfrom(
            sockfd, buffer, sizeof(buffer) - 1, 0,
            (struct sockaddr*)&client_addr, &client_addr_len);

        if (bytes_received < 0) {
            std::cerr << "recvfrom() failed: " << strerror(errno) << std::endl;
            continue;
        }

        // Parse the SOME/IP header using the codec.
        SomeIpHeader request;
        if (!parse_someip_header(buffer, bytes_received, request)) {
            std::cerr << "Datagram too short for SOME/IP header ("
                      << bytes_received << " bytes)" << std::endl;
            continue;
        }

        // Print client address and parsed fields.
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Received " << bytes_received << " bytes from "
                  << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
        std::printf("  Service ID:        0x%04X\n", request.service_id);
        std::printf("  Method ID:         0x%04X\n", request.method_id);
        std::printf("  Length:            %u\n", request.length);
        std::printf("  Client ID:         0x%04X\n", request.client_id);
        std::printf("  Session ID:        0x%04X\n", request.session_id);
        std::printf("  Protocol Version:  0x%02X\n", request.protocol_version);
        std::printf("  Interface Version: 0x%02X\n", request.interface_version);
        std::printf("  Message Type:      0x%02X\n", request.message_type);
        std::printf("  Return Code:       0x%02X\n", request.return_code);

        // Print payload as hex.
        ssize_t payload_size = bytes_received - SOMEIP_HEADER_SIZE;
        if (payload_size > 0) {
            std::cout << "  Payload (" << payload_size << " bytes): ";
            for (ssize_t i = 0; i < payload_size; ++i) {
                std::printf("%02X ", buffer[SOMEIP_HEADER_SIZE + i]);
            }
            std::cout << std::endl;
        } else {
            std::cout << "  Payload: (empty)" << std::endl;
        }

        // Build the SOME/IP RESPONSE: 16-byte header + 4-byte payload.
        constexpr size_t   RESPONSE_SIZE         = 20;
        constexpr uint32_t RESPONSE_LENGTH_FIELD = 12;  // 8 header tail + 4 payload

        SomeIpHeader response_hdr{};
        response_hdr.service_id        = request.service_id;
        response_hdr.method_id         = request.method_id;
        response_hdr.length            = RESPONSE_LENGTH_FIELD;
        response_hdr.client_id         = request.client_id;
        response_hdr.session_id        = request.session_id;
        response_hdr.protocol_version  = 0x01;
        response_hdr.interface_version = 0x01;
        response_hdr.message_type      = SOMEIP_MSG_TYPE_RESPONSE;
        response_hdr.return_code       = SOMEIP_RETURN_CODE_E_OK;

        uint8_t response[RESPONSE_SIZE];
        write_someip_header(response, response_hdr);

        // Payload: status + echoed function/state + sequence counter.
        response[16] = STATUS_COMMAND_ACCEPTED;
        response[17] = buffer[16];  // echo lamp function
        response[18] = buffer[17];  // echo lamp state
        response[19] = response_sequence_counter;
        response_sequence_counter++;

        ssize_t bytes_sent = sendto(
            sockfd, response, RESPONSE_SIZE, 0,
            (struct sockaddr*)&client_addr, client_addr_len);
        if (bytes_sent < 0) {
            std::cerr << "sendto() failed: " << strerror(errno) << std::endl;
        }
    }

    close(sockfd);
    return 0;
}

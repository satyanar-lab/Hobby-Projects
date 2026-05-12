#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cerrno>

#include "uds_codec.hpp"

constexpr uint16_t UDS_SERVER_PORT = 13400;

int main() {
    // Create UDP socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // Build destination address.
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(UDS_SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "inet_pton() failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }

    // Build the request: 0x22 + big-endian DID.
    uint8_t request[UDS_READ_DID_REQUEST_SIZE];
    request[0] = UDS_SID_READ_DATA_BY_IDENTIFIER;
    write_u16_be(request, 1, UDS_DID_VIN);

    ssize_t bytes_sent = sendto(
        sockfd, request, UDS_READ_DID_REQUEST_SIZE, 0,
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

    // Classify and decode the response.
    UdsResponseKind kind = classify_uds_response(buffer, bytes_received);

    if (kind == UdsResponseKind::Positive) {
        // Positive response: 0x62 [DID hi] [DID lo] [data...]
        if (bytes_received < static_cast<ssize_t>(UDS_READ_DID_POS_RESPONSE_MIN)) {
            std::cerr << "Positive response too short" << std::endl;
            close(sockfd);
            return 1;
        }
        uint16_t did = read_u16_be(buffer, 1);
        std::printf("Positive response (0x62) for DID 0x%04X\n", did);
        std::cout << "Data (" << (bytes_received - 3) << " bytes): ";
        for (ssize_t i = 3; i < bytes_received; ++i) {
            std::printf("%02X ", buffer[i]);
        }
        std::cout << std::endl;

        std::cout << "As ASCII: \"";
        for (ssize_t i = 3; i < bytes_received; ++i) {
            std::cout << static_cast<char>(buffer[i]);
        }
        std::cout << "\"" << std::endl;
    }
    else if (kind == UdsResponseKind::Negative) {
        UdsNegativeResponse neg;
        parse_uds_negative_response(buffer, bytes_received, neg);
        std::printf("Negative response: rejected SID 0x%02X, NRC 0x%02X\n",
                    neg.rejected_sid, neg.nrc);
    }
    else {
        std::printf("Unexpected response, first byte 0x%02X\n",
                    bytes_received >= 1 ? buffer[0] : 0);
    }

    close(sockfd);
    return 0;
}

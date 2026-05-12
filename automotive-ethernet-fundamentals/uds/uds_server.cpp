#include <iostream>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include "uds_codec.hpp"

// Server-side configuration.
constexpr uint16_t UDS_SERVER_PORT = 13400;
const char VIN[] = "VIN1234567890ABCD";
constexpr size_t VIN_LENGTH = 17;

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
    server_addr.sin_port        = htons(UDS_SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Server listening on port 13400..." << std::endl;

    while (true) {
        ssize_t bytes_received = recvfrom(
            sockfd, buffer, sizeof(buffer) - 1, 0,
            (struct sockaddr*)&client_addr, &client_addr_len);

        // Build response buffer up-front; size set per branch below.
        uint8_t response[256];
        size_t  response_size = 0;

        // Too-short request: negative response with NRC 0x13.
        if (bytes_received < static_cast<ssize_t>(UDS_READ_DID_REQUEST_SIZE)) {
            uint8_t echoed_sid = (bytes_received >= 1) ? buffer[0] : 0x00;
            response_size = build_uds_negative_response(
                response, echoed_sid, NRC_INCORRECT_MESSAGE_LENGTH);
        }
        else {
            // Parse the request via the codec.
            UdsReadDidRequest request;
            parse_uds_read_did_request(buffer, bytes_received, request);

            if (request.sid == UDS_SID_READ_DATA_BY_IDENTIFIER) {
                if (request.did == UDS_DID_VIN) {
                    response_size = build_uds_positive_response(
                        response, UDS_DID_VIN,
                        reinterpret_cast<const uint8_t*>(VIN), VIN_LENGTH);
                } else {
                    response_size = build_uds_negative_response(
                        response, request.sid, NRC_REQUEST_OUT_OF_RANGE);
                }
            } else {
                response_size = build_uds_negative_response(
                    response, request.sid, NRC_SERVICE_NOT_SUPPORTED);
            }
        }

        ssize_t bytes_sent = sendto(
            sockfd, response, response_size, 0,
            (struct sockaddr*)&client_addr, client_addr_len);
        if (bytes_sent < 0) {
            std::cerr << "sendto() failed: " << strerror(errno) << std::endl;
        }
    }

    close(sockfd);
    return 0;
}

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>

int main(void) {
    const char* t0_str = getenv("T0");
    const char* dur_str = getenv("DURATION_S");
    const char* delay_str = getenv("DELAY_MS");
    if (t0_str && dur_str && delay_str) {
        printf("[Sender] T0: %s, DURATION_S: %s, DELAY_MS: %s\n", t0_str, dur_str, delay_str);
    }

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char in_buf[2048];
    unsigned char prev_payload[160];
    bool has_prev = false;

    for (;;) {
        ssize_t n = recvfrom(in_fd, in_buf, sizeof in_buf, 0, NULL, NULL);
        if (n < 164) continue;
        
        // Harness sequence number is already big-endian (network byte order).
        // We read it, convert it to host byte order to do math, and then convert back when packing.
        uint32_t seq_net;
        memcpy(&seq_net, in_buf, 4);
        uint32_t seq_host = ntohl(seq_net);
        
        // 1) Send the current Data packet (164 bytes)
        sendto(out_fd, in_buf, 164, 0, (struct sockaddr *)&relay, sizeof relay);
        
        // 2) K=2 XOR FEC: Every 2nd frame (odd sequence), send a parity packet
        // We do this instead of appending the previous frame to EVERY packet 
        // because appending makes the packet 324 bytes, which is a 2.025x overhead (> 2.0x).
        if (seq_host % 2 == 1 && has_prev) {
            unsigned char fec_buf[164];
            
            // Set the highest bit of the sequence number to flag it as an FEC packet to the receiver.
            uint32_t fec_seq_host = seq_host | 0x80000000;
            uint32_t fec_seq_net = htonl(fec_seq_host);
            memcpy(fec_buf, &fec_seq_net, 4);
            
            // Payload is the XOR of the current and previous frames
            for (int i = 0; i < 160; i++) {
                fec_buf[4 + i] = in_buf[4 + i] ^ prev_payload[i];
            }
            
            sendto(out_fd, fec_buf, 164, 0, (struct sockaddr *)&relay, sizeof relay);
        }
        
        // Store current payload for the next iteration
        memcpy(prev_payload, in_buf + 4, 160);
        has_prev = true;
    }
    return 0;
}

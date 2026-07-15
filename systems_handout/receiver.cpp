#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>

std::unordered_map<uint32_t, std::vector<uint8_t>> data_map;
std::unordered_map<uint32_t, std::vector<uint8_t>> fec_map;
std::mutex mtx;

void try_recover(uint32_t seq) {
    uint32_t odd_seq = seq | 1;
    uint32_t even_seq = odd_seq - 1;
    
    if (fec_map.count(odd_seq)) {
        if (data_map.count(even_seq) && !data_map.count(odd_seq)) {
            std::vector<uint8_t> recovered(160);
            for(int i = 0; i < 160; i++) {
                recovered[i] = fec_map[odd_seq][i] ^ data_map[even_seq][i];
            }
            data_map[odd_seq] = std::move(recovered);
        } else if (data_map.count(odd_seq) && !data_map.count(even_seq)) {
            std::vector<uint8_t> recovered(160);
            for(int i = 0; i < 160; i++) {
                recovered[i] = fec_map[odd_seq][i] ^ data_map[odd_seq][i];
            }
            data_map[even_seq] = std::move(recovered);
        }
    }
}

int main(void) {
    const char* t0_str = getenv("T0");
    const char* dur_str = getenv("DURATION_S");
    const char* delay_str = getenv("DELAY_MS");
    if (!t0_str || !dur_str || !delay_str) {
        fprintf(stderr, "Missing env vars\n");
        return 1;
    }
    
    double t0 = atof(t0_str);
    double dur = atof(dur_str);
    double delay_ms = atof(delay_str);

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Background Playout Thread
    std::thread playout([&]() {
        uint32_t i = 0;
        for (;;) {
            // Calculate exact absolute playout deadline per the specification
            double deadline_s = t0 + (delay_ms / 1000.0) + (i * 0.020);
            
            // Convert double seconds since epoch into std::chrono time_point
            // We wake up 1ms early to ensure the packet traverses localhost UDP
            // and is timestamped by the python player BEFORE the strict deadline.
            auto deadline_time = std::chrono::time_point<std::chrono::system_clock>(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::duration<double>(deadline_s - 0.001)
                )
            );
            
            auto now = std::chrono::system_clock::now();
            if (deadline_time > now) {
                std::this_thread::sleep_until(deadline_time);
            }
            
            auto current_s = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (current_s > t0 + dur + 1.0) {
                break; // Safely terminate thread slightly after duration
            }

            {
                std::lock_guard<std::mutex> lock(mtx);
                if (data_map.count(i)) {
                    unsigned char out_buf[164];
                    uint32_t seq_net = htonl(i);
                    memcpy(out_buf, &seq_net, 4);
                    memcpy(out_buf + 4, data_map[i].data(), 160);
                    sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof player);
                }
                
                // Cleanup older frames to prevent memory bloat
                if (i >= 50) {
                    data_map.erase(i - 50);
                    fec_map.erase(i - 50);
                }
            }
            i++;
        }
    });

    unsigned char buf[2048];
    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 164) continue;
        
        uint32_t raw_seq_net;
        memcpy(&raw_seq_net, buf, 4);
        uint32_t raw_seq = ntohl(raw_seq_net);
        bool is_fec = (raw_seq & 0x80000000) != 0;
        uint32_t seq = raw_seq & 0x7FFFFFFF;
        
        std::vector<uint8_t> payload(buf + 4, buf + 164);
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (is_fec) {
                fec_map[seq] = std::move(payload);
            } else {
                data_map[seq] = std::move(payload);
            }
            try_recover(seq);
        }
    }

    playout.join();
    return 0;
}

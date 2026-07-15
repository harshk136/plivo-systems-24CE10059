#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

    unsigned char buf[2048];
    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n <= 0) continue;
        sendto(out_fd, buf, (size_t)n, 0, (struct sockaddr *)&relay,
               sizeof relay);
    }
    return 0;
}

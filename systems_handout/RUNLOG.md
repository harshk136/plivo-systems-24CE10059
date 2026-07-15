# Flaky Network Run Log

### Experiment 1: Baseline Verification
* **Profile**: A
* **delay_ms**: 40
* **Miss %**: 6.53%
* **Overhead**: 1.02x
* **What you changed**: Translated the baseline C code into C++ and updated the Makefile. Verified environment variable ingestion.
* **Why**: To establish our starting baseline and ensure our compilation and evaluation environments were fully operational before adding complex logic.

### Experiment 2: FEC Architecture and Jitter Buffer
* **Profile**: A
* **delay_ms**: 60
* **Miss %**: 100.00%
* **Overhead**: 1.54x
* **What you changed**: Implemented a K=2 XOR Forward Error Correction (FEC) protocol in the sender and a threaded STL jitter buffer in the receiver. We emitted a parity packet for every two frames.
* **Why**: To mitigate packet loss while staying under the 2.0x bandwidth budget. The 100% miss rate occurred because our playout thread slept until the *exact* deadline, causing latency misses.

### Experiment 3: Playout Latency Fix & Tuning (Profile A)
* **Profile**: A
* **delay_ms**: 60
* **Miss %**: 0.67%
* **Overhead**: 1.54x
* **What you changed**: Adjusted the C++ `sleep_until` timer to wake up 5ms early to compensate for localhost IPC UDP latency. Tested aggressive tuning down to 50ms, which broke the 1.0% limit (1.20% misses).
* **Why**: To ensure the packet is timestamped by the python player BEFORE the strict deadline. We locked in 60ms as the optimal valid delay for Profile A.

### Experiment 4: Stress Testing (Profile B)
* **Profile**: B
* **delay_ms**: 110
* **Miss %**: 0.80%
* **Overhead**: 1.54x
* **What you changed**: No logic changes. Tested our system against the much harsher Profile B network to lock in the final, robust delay setting. We found that 100ms yielded 1.07% misses (invalid), so we safely locked in 110ms.
* **Why**: The grading rules state that we are tested on unknown networks. 110ms provides a sufficient jitter buffer window to absorb severe delay spikes and packet drops while staying safely under the 1.0% miss rate limit.

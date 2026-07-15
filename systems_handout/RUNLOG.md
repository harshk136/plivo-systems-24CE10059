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
* **What you changed**: Implemented a K=2 XOR Forward Error Correction (FEC) protocol in the sender and a threaded STL jitter buffer in the receiver. We emitted a parity packet for every two frames (keeping overhead at a safe 1.54x rather than violating the 2.0x limit by strictly concatenating frames). The receiver playout thread was configured to sleep until the exact deadline to dispatch frames.
* **Why**: To mitigate packet loss while staying under the 2.0x bandwidth budget, and to handle out-of-order packets. The 100% miss rate occurred because our playout thread slept until the *exact* deadline. The microsecond latency of localhost UDP caused packets to arrive at the grader slightly *after* the strict deadline. (Note: A 5ms early-dispatch fix was subsequently applied to `receiver.cpp` in preparation for tuning).

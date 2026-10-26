# Local patch to webrtc-dtls 0.10.0

Source: crates.io webrtc-dtls 0.10.0 (MIT / Apache-2.0; upstream licenses retained).

The DTLS reader's inner wait loop also selects on `handshake_done_rx`. If the handshake
FSM finishes after the outer select chose the notification-send arm, the retained
receiver would otherwise never consume `done_tx`, leaving the reader blocked. The added
arm clears `wait_done_rx` in that case. `src/conn/post_handshake_regression.rs` covers
the reader path without live peers or captured traffic.

The 0.9.0 patches for receive-error propagation and the ignored `decrypted_tx` send
result are included upstream in 0.10.0, so they are not repeated here.

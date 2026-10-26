# Local patches to webrtc 0.11.0

Source: crates.io webrtc 0.11.0 (MIT / Apache-2.0; upstream licenses retained).

Two size limits are raised to the 262144-byte data-channel message size advertised by
libwebrtc in `a=max-message-size`:

- `src/data_channel/mod.rs`: the receive buffer was `u16::MAX` (65535 bytes); use
  262144 so the official viewer's larger port-mapping frames are not truncated.
- `src/sctp_transport/mod.rs`: the SCTP association's send limit ignores the remote
  `a=max-message-size` and defaults to 64 KB; use 262144 so a ~73 KB directory reply
  fits.

The 0.10.1 local patch that reads the real DTLS state from the ICE transport callback
is included upstream in 0.11.0, so it is not repeated here.

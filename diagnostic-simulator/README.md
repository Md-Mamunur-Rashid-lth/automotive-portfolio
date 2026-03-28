## Known limitations and planned improvements

### Multi-frame transport (ISO 15765-2)
Long responses (> 8 bytes) such as VIN readout currently truncate to one
CAN frame. A full implementation would use:
- **First Frame (FF):** announces the total length
- **Flow Control (FC):** tester grants permission to continue  
- **Consecutive Frames (CF):** deliver remaining bytes in order

This is the correct behaviour per ISO 15765-2 and is the next planned
extension to this simulator.

### QNX portability
The SocketCAN interface (`can_socket.cpp`) uses Linux-specific headers.
All OS-specific code is isolated behind the `CanSocket` class.
Porting to QNX would require replacing only that class with a   27T8YHUY7654
`io-pkt` equivalent — all higher layers are POSIX-clean.
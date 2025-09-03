# Computer Network Labs

This repository showcases my hands-on projects in computer networking, implementing core network protocols from the link layer to the transport layer.

##  Directory Structure

### Part 1: TCP Implementation
Implements the core mechanisms of the transport layer TCP protocol.

- **Lab 0: Networking Warmup:** Manual network tools, byte stream abstraction.
- **Lab 1: Stream Reassembler:** Handles out-of-order TCP segments.
- **Lab 2: TCP Receiver:** Processes sequence numbers, generates acknowledgments and window size.
- **Lab 3: TCP Sender:** Implements reliability mechanisms (retransmission timeout, exponential backoff).
- **Lab 4: TCP Connection:** Integrates sender and receiver, manages connection state (handshakes, teardown).

**Achievement:** A complete user-space TCP stack that can replace the kernel stack for network communication (e.g., in `webget`).

### Part 2: Network Stack Extension
Builds upon the TCP implementation to add link layer and network layer functionality.

- **Lab 5_00: Broadcast:** Implements a simple Hub for frame broadcasting.
- **Lab 5_01: Switching:** Implements Bridge/Switch functionality, including MAC address learning and forwarding.
- **Lab 6_02: Router:** Implements Router functionality, including ARP, IP forwarding, longest prefix match, and ICMP.
- **Lab 6_03: NAT:** Implements Network Address Translation, supporting both SNAT and DNAT.

**Environment:** These labs use the **Mininet** network emulator to create topologies and validate the implementation of custom network devices (Hub, Switch, Router, NAT) by replacing the software running on nodes.

### writeups/  
Contains detailed reports for each lab, explaining design decisions, challenges, and debugging steps.

##  Technologies & Tools
- **Languages**: C++, Python  
- **Tools**: Linux, Git, Mininet  

# IPv4 Patricia Tree — Longest Prefix Match (LPM)

A C++ implementation of an IPv4 routing table using a Patricia-tree-style data structure to explore routing decisions and Longest Prefix Match (LPM).

## Overview

Routers use **Longest Prefix Match (LPM)** to select the most specific network prefix matching a destination IPv4 address.

For example, consider these routing entries:

| Network Prefix | Route |
|---|---|
| `10.0.0.0/8` | Network A |
| `10.1.0.0/16` | Network B |
| `10.1.2.0/24` | Network C |

For destination `10.1.2.50`, the selected route is `10.1.2.0/24` because it has the longest matching prefix.

This project implements IPv4 address handling, prefix validation, tree insertion, and route lookup in C++.

## Features

- IPv4 addresses represented using `uint32_t`
- Subnet mask validation
- Network address validation
- Bitwise operations for finding differing bits
- Internal and leaf node representation
- Multiple route prefixes associated with a network prototype
- Longest Prefix Match lookup
- Tree visualization for debugging
- Test cases for overlapping prefixes and unmatched destinations

## Concepts Covered

- Patricia trees and binary tree traversal
- IPv4 addressing and subnet masks
- Bit manipulation and bit positions
- Network prefixes and routing tables
- Longest Prefix Match
- Dynamic memory management and pointers
- Time and space complexity analysis

## Example Routing Table

The sample program inserts the following routes:

| Network | Subnet Mask | Prefix |
|---|---|---|
| `10.0.0.0` | `255.0.0.0` | `/8` |
| `10.1.0.0` | `255.255.0.0` | `/16` |
| `10.1.2.0` | `255.255.255.0` | `/24` |
| `10.0.0.0` | `255.255.0.0` | `/16` |
| `192.168.1.0` | `255.255.255.0` | `/24` |

## Sample Output

```text
Destination: 10.1.2.50
Best route: Network C /24 (/24)

Destination: 10.1.3.50
Best route: Network B /16 (/16)

Destination: 10.0.5.10
Best route: Network D /16 (/16)

Destination: 192.168.1.10
Best route: Network E /24 (/24)

Destination: 8.8.8.8
No matching route
```

## Requirements

- C++ compiler supporting C++17 or later
- GCC, MinGW, or another compatible compiler
- Visual Studio Code (optional)

## Compile and Run

### Windows (PowerShell)

```powershell
g++ -std=c++17 -Wall -Wextra patricia_lpm_final.cpp -o patricia_lpm
.\patricia_lpm.exe
```

### Linux / macOS

```bash
g++ -std=c++17 -Wall -Wextra patricia_lpm_final.cpp -o patricia_lpm
./patricia_lpm
```

Replace the source filename with the actual filename in your repository if necessary.

## Project Structure

```text
ipv4-patricia-lpm/
├── patricia_lpm_final.cpp
└── README.md
```

## Future Improvements

- Add automated unit tests for edge cases
- Support default routes (`/0`) and host routes (`/32`)
- Test duplicate-prefix handling and invalid input
- Benchmark insertion and lookup performance
- Compare against a brute-force LPM implementation
- Evaluate memory usage on larger routing tables
- Compare the implementation with published Patricia-tree routing algorithms

## Learning Objectives

This project is intended to strengthen understanding of C++ data structures, bitwise operations, IPv4 routing, and Longest Prefix Match algorithms.

## Author

**Vipul Dhiman**

GitHub: [VipulDhiman8865](https://github.com/VipulDhiman8865)

---

*This is an educational implementation. Its exact data-structure invariants and performance characteristics should be verified before treating it as a production routing-table implementation.*

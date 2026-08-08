# AEGIS

AEGIS is a high-performanced, decentralized and encrypted network engine based on modern C++20 standard, boost.asio and libsodium.

## Core Features

- **AETP Protocol Integration**: AEGIS supports AETP Alpha 1.1.0, which is a high-performance E2EE transport protocol allowing 1-RTT handshake.

- **Modern Async Architecture**: Using C++20 Coroutine and Boost.Asio and implementing a high-performance network engine.

- **Double Engine Scheduling**: Integrated thread pool allows AEGIS's encryption runs in parallel without blocking the IO thread.

- **High-Performance**: Deeply applied Move Semantics to reduce memory copy and dangling pointer.

- **Self-Certifying and Decentralized**: Peer ID is based on the long-term public key's base64 to reduce the risk of identity theft from the root.

- **Highly-Secured**: Integrated libsodium uses XChaCha20-Poly1305 for encryption and Ed25519 for digital signature.

## Build

Environment requirements:
- C++20 Compiler (GCC, Clang or MSVC)
- CMake and Ninja
- boost
- libsodium

```bash

git clone https://github.com/Herminos/Aegis.git

cd Aegis

mkdir build

cd build

cmake ..

make -j$(nproc)

```

## Usage

- Run `./AEGIS --help` to see the help message.

- If peers are in the same LAN, use `--lan` to set into UDP Broadcast Mode to quickly find peers (Actually multicast is used under the hood).

- If peers are in different LANs, you may need a VPS which has a public IP to create a SSH reverse tunnel or run frp for you, then use `--listen` and `--connect` to set the host to listen on localhost and connect to your VPS.

## Liscense

AEGIS Alpha 1.0.0 is released under MIT License, but all the future releases will be under AGPL-3.0.

## Future

- AEGIS may act as a daemon process in the future, so it can interact with browser GUIs using WebSocket (boost.beast).

- AETP Alpha 2.0.0 will be implemented in future AEGIS version, which will provide AETP traffic's **Rule of Absolute Entropy** and **Zero-Byte Protocol Fingerprinting**.

- AEGIS is a personal hobby project now, but I hope more amateur developers can join it.
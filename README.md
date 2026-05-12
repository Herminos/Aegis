# AEGIS

AEGIS is a high-performanced, decentralized and encrypted network engine based on modern C++20 standard, boost.asio and libsodium.

## Core Features

- **Modern Async Architecture**: Using C++20 Coroutine and Boost.Asio and implementing a high-performanced network engine.

- **Double Engine Scheduling**: Integrated thread pool allows AEGIS's encryption runs in parallel without blocking the IO thread.

- **High-Performance**: Deeply applied Move Semantics to reduce memory copy and dangling pointer.

- **Self-Certifying and Decentralized**: Peer ID is based on the hash value (BLAKE3) of public key to reduce the risk of identity theft from the root.

- **Highly-Secured**: Integrated libsodium uses XChaCha20-Poly1305 for encryption and Ed25519 for digital signature.

## Build

Environment requirements:
- C++20 Compiler (MinGW or MSVC)
- CMake
- boost.asio,boost.json
- libsodium

`pwsh

git clone https://github.com/Herminos/Aegis.git

cd Aegis

mkdir build

cd build

cmake ..

cmake --build .

./AEGIS

`
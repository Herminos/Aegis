Network Working Group                                          H. Herminos
Internet-Draft                                                May 17, 2026
Category: Standards Track

# AETP: Aegis Encrypted Transport Protocol Alpha 2.0.0
*(Draft Specification)*

## Abstract

AETP (Aegis Encrypted Transport Protocol) is a secure, application-layer transport protocol designed to run over TCP or TLS streams. Its primary objectives are to provide Aegis P2P nodes with absolute resistance against Deep Packet Inspection (DPI), Perfect Forward Secrecy (PFS), and high-performance asynchronous data exchange. 

AETP Alpha 2.0.0 fundamentally deprecates the explicit state machines and plaintext headers of Alpha 1.1.0. It introduces the **Rule of Absolute Entropy**: all bytes transmitted over the wire MUST be statistically indistinguishable from uniform random noise. To achieve this, AETP utilizes Elligator 2 obfuscation, an implicit incrementing counter mechanism, dual-block AEAD framing, and zero-byte protocol fingerprinting.

## Status of This Memo

This document defines the standard implementation for the AETP Alpha 2.0.0 release.

## 1. Introduction

Traditional transport protocols expose metadata (versions, flags, lengths, state types) in plaintext, rendering them vulnerable to heuristic and signature-based blocking by censorship firewalls. AETP Alpha 2.0.0 operates under the assumption that the network is highly hostile. 

AETP segments continuous data into deterministic, fully encrypted chunks without any explicit protocol identification. Connection lifecycles are dictated by C++20 coroutine linear execution flow (Program Counter) rather than explicit Finite State Machine (FSM) variables.

## 2. Deprecation of v1.1.0 Features

Implementations upgrading to AETP Alpha 2.0.0 **MUST NOT** include the following legacy features:
* **DEPRECATED:** Plaintext Magic Words or Protocol Identifiers.
* **DEPRECATED:** Plaintext message types (e.g., `0x01` for Handshake, `0x02` for Data).
* **DEPRECATED:** Explicit Nonce transmission over the wire.
* **DEPRECATED:** CRC16 or any non-cryptographic checksums.

## 3. Cryptographic Primitives

AETP mandates the use of the `libsodium` library to guarantee constant-time, side-channel resistant operations:
* **Long-Term Identity & Signatures**: Ed25519
* **Ephemeral Key Exchange**: Ristretto255 (chosen for its zero-distortion blind spot group, ensuring public keys resemble uniform random bytes).
* **Authenticated Encryption**: XChaCha20-Poly1305-IETF
* **Curve Point Obfuscation**: Elligator 2 Mapping

## 4. The 128-Byte Blind Handshake (Elligator 2 Obfuscation)

Once the TCP connection is established, both endpoints MUST begin the **Blind Handshake** phase.

The handshake phase consists of a strict 1-RTT exchange. Neither endpoint SHALL prepend any type identifier. The payload is exactly 128 bytes. 

To enforce the **Rule of Absolute Entropy**, standard Ed25519 public keys and signature points MUST NOT be transmitted in the clear, as their algebraic validity (e.g., verifying if a point lies on the Curve25519 equation) presents a highly detectable heuristic fingerprint to Deep Packet Inspection (DPI) firewalls.

The Initiator and Listener exchange the following 128-byte block:

    [ 32 Bytes: Obfuscated Long-Term Public Key      ]
    [ 32 Bytes: Ephemeral Public Key (Ristretto255)  ]
    [ 64 Bytes: Obfuscated Detached Signature        ]

**Cryptographic Obfuscation Rules:**
1. **Long-Term PK:** The Ed25519 Long-Term Public Key MUST be mapped to a uniform random string of 32 bytes using the **Elligator 2** mapping algorithm prior to transmission.
2. **Ephemeral PK:** The Ristretto255 group natively produces uniform representations and requires no further obfuscation.
3. **Signature:** The Ed25519 detached signature consists of a curve point `R` and a scalar `S`. The point `R` MUST also be obfuscated using **Elligator 2** mapping to eliminate structural curve fingerprints.

The receiving endpoint MUST blindly reverse the Elligator 2 mappings to recover the true Ed25519 key and signature `R` point before executing standard signature validation. Any failure in mapping inversion or signature validation MUST result in a Fail-Fast connection termination.

## 5. Implicit Counter Protocol (Nonce Management)

AETP v2.0.0 strictly prohibits the transmission of the AEAD Nonce over the wire. 

Upon successful generation of the symmetric Session Keys (`tx_key`, `rx_key`), both endpoints **MUST** initialize two 64-bit unsigned integers in local memory:
* `uint64_t tx_counter = 0;`
* `uint64_t rx_counter = 0;`

For every AEAD encryption operation, the 64-bit `tx_counter` is padded to the required Nonce size (24 bytes for XChaCha20) and incremented by `1`.
For every AEAD decryption operation, the endpoint uses its local `rx_counter`, padding it identically, and increments it by `1` upon a successful Poly1305 MAC validation.

*Security Note: This mathematically guarantees protection against replay attacks with zero wire-overhead. A replayed packet will fail the Poly1305 validation due to a Nonce mismatch in local memory.*

## 6. Binary Framing: Dual-Block AEAD Model

Once the handshake is complete, all application data MUST be encapsulated into a stream of contiguous, fully encrypted Dual-Block frames. 

A single AETP Frame consists of:

**Block 1: Encrypted Length Block (Exactly 18 Bytes)**

[ 2 Bytes: Encrypted Length (N) ]
[ 16 Bytes: Poly1305 MAC        ]

* **Plaintext:** 2 bytes containing the length (`N`) of the upcoming payload.
* **Ciphertext:** 2 bytes of encrypted length + 16 bytes of Poly1305 MAC.
* *Behavior:* The receiving stream MUST blindly read exactly 18 bytes and decrypt this block to determine `N`.

**Block 2: Encrypted Payload Block (Exactly N + 16 Bytes)**

[ N Bytes: Ciphertext      ]
[ 16 Bytes: Poly1305 MAC   ]

* **Plaintext:** `N` bytes of actual application payload.
* **Ciphertext:** `N` bytes of encrypted payload + 16 bytes of Poly1305 MAC.
* *Behavior:* The receiving stream dynamically allocates a buffer of `N + 16` bytes, reads the payload block, and decrypts it.

## 7. Lifecycle, Concurrency & Stream Serialization

Because AETP relies on strict, stateful Implicit Counters, thread-safe write serialization is paramount.

* **Stream Serialization Constraint:** Implementations **MUST** ensure that concurrent calls to send application data do not result in interleaved bytes on the underlying socket. 
* Implementations **SHOULD** utilize an Asynchronous Send Queue and a dedicated, single-threaded write coroutine loop to flush frames into the socket sequentially, preserving the integrity of the `tx_counter`.

## 8. Transport Obfuscation Profiles

AETP is agnostic to the underlying stream, enabling modular transport layer obfuscation. Implementations MUST support the following profiles via configuration:

* **Profile A (`RAW_AETP`):** The dual-block AEAD stream is written directly to a raw TCP socket (`boost::asio::ip::tcp::socket`). Used against firewalls relying solely on whitelist exclusions or when minimal CPU overhead is required.
* **Profile B (`TLS_TUNNEL`):** The dual-block AEAD stream is piped into a TLS 1.3 stream proxy (`boost::asio::ssl::stream`). The OpenSSL kernel encapsulates the AETP noise into standard `0x17` Application Data records. Used for blending into commercial HTTPS traffic to bypass strict whitelisting DPI.

## 9. Security Considerations

* **Perfect Forward Secrecy (PFS):** Implementations **MUST** call `sodium_memzero()` (or equivalent memory wiping functions) on the `ephemeral_sk`, `rx_key`, and `tx_key` immediately upon entering the terminated state, object destruction, or encountering a MAC validation error.
* **Fail-Fast Defense:** Under no circumstances SHALL the protocol return specific error codes (e.g., "Invalid Signature", "Length Too Large") over the wire. Any malformed byte MUST trigger a silent and immediate socket reset (`RST`).
* **Length Obfuscation (Optional):** Implementations MAY pad the payload before encryption to mitigate traffic analysis based on packet sizes, though Profile B (`TLS_TUNNEL`) natively provides record boundary obfuscation.
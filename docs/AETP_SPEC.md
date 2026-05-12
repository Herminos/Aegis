Network Working Group                                         H. Herminos
Internet-Draft                                               May 12, 2026
Category: Standards Track
Expires: November 13, 2026

# AETP: Aegis Encrypted Transport Protocol v1.0.0
*(Draft Specification)*

## Abstract

AETP is a secure application-layer transport protocol designed to run over TCP. Its primary objectives are to provide Aegis P2P nodes with resistance against Man-In-The-Middle (MITM) attacks, Perfect Forward Secrecy (PFS), and high-performance asynchronous data exchange. AETP utilizes a highly optimized 1-RTT cryptographic handshake.

## Status of This Memo

This document defines the standard implementation for the AETP Alpha 1.0.0 release.

## 1. Introduction

AETP segments the continuous TCP stream into discrete, deterministic frames using a strict binary header. All connection establishment procedures MUST adhere to a robust Finite State Machine (FSM) that binds long-term node identities to ephemeral session keys.

## 2. Binary Framing

Every AETP message MUST start with a fixed 10-byte header.

```text

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Magic (0xAE 0x47)         |  Ver (0x01)   | Type (0xXX)   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      Payload Length (N)                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         CRC16 (Header)        |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
   |                                                               |
   |                  Payload (N bytes)                            |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

```

**Fields:**
* **Magic**: MUST be `0xAE 0x47` (Aegis Gate).
* **Ver**: Protocol version. Current version is `0x01`.
* **Type**:
    * `0x01`: SENDING_PUBKEY (Handshake Packet)
    * `0x02`: ENCRYPTED_DATA_TRANSMISSION (Application Data)
    * `0xFF`: TERMINATED (Error / Socket Close)
* **Length**: Payload byte count in Network Byte Order (Big-Endian).
* **CRC16**: Checksum of the first 8 bytes (Header validation).

## 3. Cryptographic Primitives

AETP mandates the use of the `libsodium` library:
* **Long-Term Identity & Signatures**: Ed25519
* **Ephemeral Key Exchange**: Curve25519 (X25519)
* **Authenticated Encryption**: XChaCha20-Poly1305-IETF

## 4. Connection State Machine

The Finite State Machine (FSM) for AETP is as follows:

    enum class SessionState {
       IDLE,
       PUBKEY_EXCHANGING, // Client and Server exchange their long-term 
                          // public key and ephemeral session public key, 
                          // and a digital signature of ("AEGIS" + Ephemeral 
                          // public key) using their own long-term private key.
       ACTIVE,
       TERMINATED
    };

Implementations MUST maintain the following transition logic:

**[ Client Side (Initiator) ]**

    [IDLE]
       |
       |-- (Generate Ephemeral Key pair)
       |-- (Sign "AEGIS" + Ephemeral public key with long-term private key)
       |-- (Send SENDING_PUBKEY) ----> [PUBKEY_EXCHANGING]
       |
    [PUBKEY_EXCHANGING]
       |
       |-- (Recv SENDING_PUBKEY)
       |   |
       |   |-- (Verify Peer's Signature)
       |       |
       |       |-- [Signature Valid] ----> (Compute Session Key) ----> [ACTIVE]
       |       |-- [Signature Invalid] --> [TERMINATED]
       |
    [ACTIVE]
       |
       |-- (Recv ENCRYPTED_DATA_TRANSMISSION) ----> (Decrypt & Dispatch)
       |-- (Auth Fail / MAC Error) -> [TERMINATED]
       |
    [TERMINATED]
       |
       |-- (Wipe Keys) ---> (Close Socket)

**[ Server Side (Listener) ]**

    [IDLE]
       |
       |-- (Accept Connection) ----> [PUBKEY_EXCHANGING]
       |
    [PUBKEY_EXCHANGING]
       |
       |-- (Recv SENDING_PUBKEY)
       |-- (Verify Peer's Signature)
       |   |-- [Signature Invalid] --> [TERMINATED]
       |   |-- [Signature Valid] 
       |       |-- (Generate Ephemeral Key Pair)
       |       |-- (Compute Session Keys)
       |       |-- (Sign "AEGIS" + Server's Ephemeral public key with Server's long-term private key)
       |       |-- (Send SENDING_PUBKEY) ----> [ACTIVE]
       |
    [ACTIVE]
       |
       |-- (Recv ENCRYPTED_DATA_TRANSMISSION) ----> (Decrypt & Dispatch)
       |-- (Auth Fail / MAC Error) --> [TERMINATED]

## 5. Handshake Procedure (SENDING_PUBKEY)

The payload for a Type `0x01` (`SENDING_PUBKEY`) message is exactly 128 bytes:

    [ 32 Bytes: Long-Term Public Key (Ed25519) ]
    [ 32 Bytes: Ephemeral Public Key (X25519)  ]
    [ 64 Bytes: Detached Signature (Ed25519)   ]

**Signature Generation:**
The signature MUST be calculated over the following buffer:
Context String (`"AEGIS"`) concatenated with the 32-byte Ephemeral Public Key.
`crypto_sign_detached(sig, "AEGIS" || Ephemeral_PK, LongTerm_SK)`

## 6. Application Payload (ENCRYPTED_DATA_TRANSMISSION)

Once in the `[ACTIVE]` state, all Type `0x02` payloads MUST be encrypted.
The structure of the payload is:

    [ 24 Bytes: Nonce (monotonically increasing) ]
    [ N  Bytes: Ciphertext (Encrypted JSON)      ]
    [ 16 Bytes: MAC (Poly1305 Authenticator)     ]

*(Note: `libsodium`'s `crypto_aead_xchacha20poly1305_ietf_encrypt` appends the MAC automatically).*

## 7. Security Considerations

* **Perfect Forward Secrecy**: Implementations MUST call `sodium_memzero()` on the `ephemeral_sk`, `rx_key`, and `tx_key` immediately upon entering the `[TERMINATED]` state or object destruction.
* **Buffer Overflow**: Implementations MUST verify the Header `Length` field against a maximum threshold (e.g., 16MB) before allocating receive buffers.
* **Identity Verification**: The Long-Term Public Key received in the handshake SHOULD be verified against the peer's UDP discovery broadcast.
# AEGIS Alpha 1.0.0 — Comprehensive Code Review

**Date**: 2026-06-10
**Reviewer**: Automated Code Review
**Scope**: Full codebase (13 source files, ~2500 lines C++)

---

## Table of Contents

1. [CRITICAL — Security & Crash Risks](#1-critical--security--crash-risks)
2. [HIGH — Correctness Bugs](#2-high--correctness-bugs)
3. [MEDIUM — Design & Maintainability](#3-medium--design--maintainability)
4. [LOW — Code Style & Polish](#4-low--code-style--polish)

---

## 1. CRITICAL — Security & Crash Risks

### 1.1 Nonce Construction — Safe Under Single-Threaded IO, But Fragile

**File**: `src/Session/Session.cpp:471-478`
```cpp
auto self(shared_from_this());
boost::asio::co_spawn(
    socket.get_executor(),
    [self, ...]() mutable -> boost::asio::awaitable<void> {
        try {
            self->tx_counter+=randombytes_random()%7+1;        // ① sync
            std::memcpy(nonce.data(), &self->tx_counter, ...); // ② sync
            randombytes_buf(nonce.data() + 8, 16);             // ③ sync
            auto cipher = co_await self->encryptor.async_encrypt(...); // ④ SUSPEND
            ...
```

**Analysis**: In the current architecture (`io_context` with a single `io.run()` call in `main.cpp:112`), coroutines execute **serially** on one thread. A coroutine runs ①②③ to completion before hitting the `co_await` at ④ — the only suspension point in the path. By the time another coroutine resumes, `tx_counter` has already been updated. **Under this single-threaded model, there is no race condition.**

The nonce format `[counter(8B)][random(16B)]` is sound — the 16 random bytes (via `randombytes_buf`) give a collision probability of ~2⁻¹²⁸, and the monotonically-increasing counter adds defense-in-depth against RNG failure.

**Risk**: This correctness is **implicitly dependent** on single-threaded execution. If anyone later:
- Calls `io.run()` from multiple threads
- Replaces the `io_context` with a multi-threaded `thread_pool` executor
- Uses `BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO`

the code **silently** becomes unsafe with no compile-time or runtime warning. Nonce reuse under XChaCha20-Poly1305 is catastrophic.

**Fix**: Add a static assertion or prominent comment to make the single-threaded contract explicit:
```cpp
// CRITICAL: tx_counter correctness depends on single-threaded io_context.
// All writes to tx_counter occur before the only co_await suspension point in
// this coroutine body. Do NOT call io.run() from multiple threads.
```
Or, for defence-in-depth, use `std::atomic<uint64_t>` for the counter — negligible overhead, zero risk.

---

### 1.2 No Persistent Identity Keys — New Identity on Every Launch

**File**: `src/crypto/Encryptor.cpp:31-34`
```cpp
void Encryptor::load_identity_key_pair() {
    //写一个判断是否有现成的PEM来加载
    generate_identity_key_pair();
}
```

**Problem**: The TODO comment (Chinese: "write a check for existing PEM to load") reveals this was never implemented. Every program restart generates new Ed25519 keys, creating a new Node ID. This:
- Breaks the self-certifying identity model described in the README
- Invalidates any previously-established trust relationships
- Makes tombstoning useless (old IDs can never reappear)

**Fix**: Implement actual key persistence — load from `~/.aegis/identity.key` if it exists, otherwise generate and save.

---

### 1.3 Use-After-Free via Dangling Lambda Captures

**File**: `src/main.cpp:56-58`
```cpp
tcp_sender.set_accept_handler(
    [&session_manager, &io](ip::tcp::socket peer_socket) {
        session_manager.new_session_from_socket_and_start(std::move(peer_socket));
    }
);
```

**Problem**: The TCP acceptor re-arms itself recursively. If `main()` were extended to support graceful shutdown, or if the `io_context` is stopped and restarted, the lambda captures `[&session_manager, &io]` would dangle. More critically, there's **no mechanism to stop the accept chain** — if a shutdown is initiated, new accepts can still fire after `session_manager` is partially torn down.

Additionally in `exit_aegis_handler` (`SessionManager.hpp:88-92`):
```cpp
std::thread([this](){
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待2秒让所有会话有机会发送结束消息
    io.stop();
}).detach();
```
The detached thread captures `[this]` with a 1-second delay (note: comment says 2 seconds, code does 1). If the `SessionManager` is destroyed before the thread fires, this is a use-after-free crash. Also, comment is wrong — claims 2s sleep, only does 1s.

**Fix**: Use `shared_from_this` or weak pointers for async callbacks. Add a proper shutdown sequence with `acceptor.close()` before destroying objects.

---

### 1.4 Enormous Payload Size Limit Enables Trivial DoS

**File**: `src/Session/Session.cpp:220-221`
```cpp
if(header.payload_length >= 1*1024*1024*1024){
    throw std::runtime_error("Payload over 1GB, too large");
}
```

**Problem**: The AETP spec says the maximum should be 16MB. This code allows up to 1GB. More critically, it allocates the buffer **before** validating:
```cpp
std::vector<uint8_t> payload_buffer(header.payload_length);  // Allocates 1GB!
```
An attacker can send a header claiming a 999MB payload, causing a 999MB heap allocation. A few dozen such connections will exhaust memory.

**Fix**: Reduce the limit to match the spec (e.g., 16MB) and validate the limit **before** allocation:
```cpp
constexpr uint32_t MAX_PAYLOAD = 16 * 1024 * 1024; // 16MB per spec
if(header.payload_length > MAX_PAYLOAD) {
    throw std::runtime_error("Payload exceeds maximum allowed size.");
}
std::vector<uint8_t> payload_buffer(header.payload_length);
```

---

### 1.5 No Thread Safety on `session_map`

**File**: `include/AEGIS/SessionManager.hpp` and `src/Managers/SessionManager.cpp`

**Problem**: `session_map` and `tombstoned_ids` are accessed concurrently from:
1. The IO thread (via `boost::asio::post`)
2. The input thread (via `send_message`)
3. The coroutine completion handlers
4. The UDP receive handler chain

None of these accesses are synchronized. Concurrent `map::erase` and `map::find` operations are undefined behavior and will cause crashes.

**Fix**: Use a `strand` to serialize all access, or add a `std::mutex`:
```cpp
class SessionManager {
private:
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Session>> session_map;
    // all access must hold mutex_
};
```

---

### 1.6 UDP Reply Sent to Multicast Address Instead of Sender

**File**: `src/Managers/UdpManager.cpp:79-96` and constructor
```cpp
UdpBroadcaster::UdpBroadcaster(io_context &_io, int port) 
    : socket(_io), timer(_io) 
{
    ...
    remote_endpoint=ip::udp::endpoint(ip::make_address_v4("239.255.0.1"), port);
}
```

`remote_endpoint` is permanently set to the multicast group address. `send_reply()` sends to `remote_endpoint`, meaning "replies" are actually multicast to all nodes. This may be intentional for LAN discovery, but:
1. The function is named `send_reply` implying unicast
2. It wastes bandwidth and exposes reply content to all LAN nodes
3. On networks with multicast disabled or filtered, replies will never reach the intended peer

**Fix**: Either rename to `send_multicast_reply()` and document the design, or pass the sender's endpoint from `on_broadcast_handler` and unicast to it.

---

## 2. HIGH — Correctness Bugs

### 2.1 `close_session()` Uses Wrong Error Code

**File**: `src/Session/Session.cpp:362-378`
```cpp
void Session::close_session(const boost::system::error_code& ec) {
    if (socket.is_open()) {
        boost::system::error_code ignored_ec;
        if(socket.close(ignored_ec)){
            log_error(string("[ERROR] Error closing socket: ") + ec.message());
            //                                              ^^ BUG: should be ignored_ec
        }
    }
    ...
}
```

**Problem**: The code intentionally captures the close error in `ignored_ec` but then logs `ec` (the caller's error code, which might be `connection_reset` or similar). This misreports why the socket close failed.

**Fix**: Log `ignored_ec.message()` instead.

---

### 2.2 Session Never Transitions to `TERMINATED` in `close_session()`

**File**: `src/Session/Session.cpp:362-378`

**Problem**: `close_session()` closes the socket and calls the close handler but never sets `state = SessionState::TERMINATED`. This means:
- `clean_session()` returns immediately (it checks for `TERMINATED`)
- The session's keys are **never wiped** by `clean_session()` — they only get wiped in `~Session()`
- If `shared_ptr` references keep the session alive, keys persist in memory

Compare with the EOF handling path (line 248-251) which correctly sets TERMINATED before calling `clean_session()`.

**Fix**: Add `state = SessionState::TERMINATED;` before the close handler in `close_session()`.

---

### 2.3 Type Confusion — Magic Numbers Instead of Constants

**File**: `src/Session/Session.cpp:227-228`
```cpp
if(header.type==0x01){  // Should be __SENDING_PUBKEY
    ...
}
if(header.type==0x02){  // Should be __SENDING_ENCRYPTED_DATA
```

**Problem**: The constants `__SENDING_PUBKEY` (0x01) and `__SENDING_ENCRYPTED_DATA` (0x02) are defined at line 12-14 but the code uses raw magic numbers. If the constants change, the dispatch breaks silently.

**Fix**: Use the named constants throughout.

---

### 2.4 `UdpListener::listen()` Never Restarts After Error

**File**: `src/Managers/UdpManager.cpp:119-135`
```cpp
void UdpListener::listen() {
    socket.async_receive_from(buffer(recv_buffer), remote_endpoint, 
        [this](const boost::system::error_code& e, std::size_t bytes_transferred) {
            if (e) {
                log_error(string("[ERROR] Error receiving UDP message: ") + e.message());
                return;  // <-- Never calls this->listen() again!
            } else {
                ...
                this->listen();  // Only re-arms on success
            }
        }
    );
}
```

**Problem**: On any UDP receive error, listening stops permanently. The node becomes deaf to further LAN discovery messages. Compare with `TcpSender::accept()` which correctly re-arms.

**Fix**: Re-arm the listener in some error cases (not `operation_aborted` which means intentional shutdown):
```cpp
if (e) {
    if (e != boost::asio::error::operation_aborted) {
        log_error(...);
        this->listen(); // Re-arm
    }
    return;
}
```

---

### 2.5 `ignite_apple.py` Uses Obsolete CLI Interface

**File**: `ignite_apple.py:36-37`
```python
run_apple_script(f"exec {target_binary} 9247")
run_apple_script(f"exec {target_binary} 9471")
```

**Problem**: The old `main.cpp` accepted a raw port number as the only argument. The current `main.cpp` uses `ArgParser::parse()` which expects `--lan <port>`, `--listen <port>`, or `--connect <host:port>`. Passing `9247` as a bare argument will trigger the usage error and exit(1). This script is broken.

**Fix**: Update to use the new CLI format: `--lan 9247`

---

### 2.6 Session Double-Cleanup Possible

**File**: `src/Session/Session.cpp:317-349` (`process_encrypted_data_coroutine`)

**Problem**: When a TERMINATION packet is received:
```cpp
this->state = SessionState::TERMINATED;
clean_session();       // Call #1 -> calls on_session_cleaned_handler which erases from map
co_return;
```

Then in the caller (`start_read_loop_coroutine`), after the termination:
```cpp
if(self->state == SessionState::TERMINATED){
    break;  // exits loop
}
```

But if `close_session()` is also called from an error path in the write loop, `on_session_cleaned_handler` may fire twice for the same session. The first call erases from the map; the second logs "Attempted to clean session with id X but it was not found."

**Fix**: Add a `bool cleaned_ = false` flag to prevent double cleanup, or use `std::atomic_flag`.

---

### 2.7 `std::system("pause")` is Windows-Only

**File**: `src/main.cpp:118`
```cpp
system("pause");
```

**Problem**: This Unix/macOS portability issue blocks execution on non-Windows systems. On macOS/Linux, `pause` is not a standard command, so this will print a "command not found" error.

**Fix**: Make it platform-conditional or remove it:
```cpp
#if defined(_WIN32) || defined(_WIN64)
    system("pause");
#endif
```

---

## 3. MEDIUM — Design & Maintainability

### 3.1 `using namespace std;` and `using namespace boost::asio;` in Headers

**File**: Multiple headers (`Encryptor.hpp`, `Session.hpp`, `SessionManager.hpp`, `TcpManager.hpp`, `UdpManager.hpp`)

**Problem**: This pollutes the global namespace for every translation unit that includes these headers. Anyone including `Encryptor.hpp` inherits `std` and `boost::asio` symbols, which can cause silent name collisions, especially with generic names like `string`, `vector`, `map`, `error_code`, `socket`.

**Fix**: Never `using namespace` in headers. Qualify types explicitly or use limited `using` declarations inside class/function scopes.

---

### 3.2 `Encryptor` Public Members Expose Private Key

**File**: `include/AEGIS/Encryptor.hpp:25-26`
```cpp
class Encryptor {
    public:
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> private_key;  // <-- PUBLIC!
```

**Problem**: The Ed25519 private key is a public data member. Any code with access to the `Encryptor` object can read, modify, or leak the private key.

**Fix**: Make these private, provide const getter for public_key only:
```cpp
class Encryptor {
private:
    std::vector<uint8_t> public_key_;
    std::vector<uint8_t> private_key_;
public:
    const std::vector<uint8_t>& get_public_key() const { return public_key_; }
    // private_key_ should NEVER be exposed
};
```

---

### 3.3 Inconsistent Key Wiping in `clean_session()` vs Destructor

**File**: `src/Session/Session.cpp:390-408` vs `130-156`

**Destructor** (correct):
```cpp
sodium_memzero(peer_ephemeral_public_key.data(), peer_ephemeral_public_key.size());
```

**clean_session()** (incomplete):
```cpp
peer_ephemeral_public_key.clear();  // Just shrinks, doesn't zero memory
```

**Problem**: `vector::clear()` deallocates memory but the cryptographic key material may remain in the heap. `sodium_memzero()` ensures secure overwriting.

**Fix**: Zero with `sodium_memzero()` before calling `clear()` in `clean_session()` too.

---

### 3.4 AES Header Parsing Ignores Unknown Packet Types Silently

**File**: `src/Session/Session.cpp:227-243`

**Problem**: The read loop only handles types `0x01` (handshake) and `0x02` (encrypted data). Any other type (including the `0xFF` termination sent by peers) is silently ignored, causing the read loop to go back to reading another header. This wastes CPU and could be used to keep connections alive maliciously.

**Fix**: Add an explicit `else` branch that either closes the connection or logs a warning:
```cpp
if(header.type == __SENDING_PUBKEY) { ... }
else if(header.type == __SENDING_ENCRYPTED_DATA) { ... }
else {
    log_warning("Unknown AETP packet type, closing connection.");
    throw std::runtime_error("Unknown AETP packet type.");
}
```

---

### 3.5 `CMakeLists.txt` Uses `GLOB_RECURSE` for Source Discovery

**File**: `CMakeLists.txt:19-20`
```cmake
file(GLOB_RECURSE AEGIS_SRC "src/*.cpp")
file(GLOB_RECURSE AEGIS_HDR "include/*.hpp")
```

**Problem**: CMake's own documentation advises against `GLOB_RECURSE` because:
- New files are not detected until CMake is re-run
- Stale files are silently included
- Build reproducibility is compromised

**Fix**: List source files explicitly:
```cmake
set(AEGIS_SRC
    src/main.cpp
    src/crypto/Encryptor.cpp
    src/Session/Session.cpp
    src/Managers/ArgParser.cpp
    src/Managers/InputManager.cpp
    src/Managers/SessionManager.cpp
    src/Managers/TcpManager.cpp
    src/Managers/UdpManager.cpp
    src/Router/CommandRouter.cpp
)
```

---

### 3.6 `CMakeLists.txt` Hardcodes Compiler Flags as Linker Flags

**File**: `CMakeLists.txt:53-57`
```cmake
add_compile_options(
    -DSODIUM_STATIC -lsodium
    -lboost_system -lboost_thread
    -static -static-libgcc -static-libstdc++ 
)
```

**Problem**: `-lsodium`, `-lboost_system`, `-lboost_thread`, `-static` are **linker** flags, not compiler flags. They happen to work with GCC/Clang because the driver passes them through, but:
- This is semantically wrong
- MSVC will reject them
- `-DSODIUM_STATIC` is a preprocessor define that belongs in `target_compile_definitions`
- The static linking flags are GCC/Clang-specific

**Fix**:
```cmake
target_compile_definitions(AEGIS PRIVATE SODIUM_STATIC)
target_link_options(AEGIS PRIVATE -static -static-libgcc -static-libstdc++)
# For MSVC compatibility, use generator expressions
target_link_libraries(AEGIS PRIVATE ${SODIUM_LIBRARIES})
find_package(Boost REQUIRED COMPONENTS system thread)
target_link_libraries(AEGIS PRIVATE Boost::system Boost::thread)
```

---

### 3.7 `UdpBroadcaster` Name Misleading — Is a Multicast Broadcaster

**File**: `include/AEGIS/UdpManager.hpp:14`

**Problem**: The class is named `UdpBroadcaster` but it uses IP multicast (group `239.255.0.1`). These are different concepts. UDP broadcast is to `255.255.255.255`; multicast is to a group address. The README also says "UDP Broadcast Mode (Actually multicast is used under the hood)."

**Fix**: Rename to `UdpMulticaster` or at least add a comment clarifying.

---

### 3.8 Missing `final` on Classes

**Files**: `Session.hpp`, `Encryptor.hpp`, etc.

**Problem**: `Session` inherits from `std::enable_shared_from_this<Session>` and is clearly not designed for further inheritance. Without `final`, there's a risk of slicing or misuse.

**Fix**:
```cpp
class Session final : public std::enable_shared_from_this<Session> { ... };
```

---

### 3.9 Redundant `inline` on Class Member Functions Defined in Headers

**File**: `include/AEGIS/Encryptor.hpp:27-39`

**Problem**: Member functions defined inside a class body are implicitly `inline`. The explicit `inline` keyword is redundant. The comment "这么简单的函数就写在这里了" (Chinese: "such a simple function is written here") suggests the author knew this was a shortcut.

**Fix**: This is fine as-is for trivial getters, but for `verify_signature` (which wraps a crypto call) it belongs in the `.cpp` file.

---

## 4. LOW — Code Style & Polish

### 4.1 Typo in Filename: `depoly.py` → `deploy.py`

**File**: `depoly.py`

**Fix**: Rename file.

---

### 4.2 Inconsistent Error Message Prefixes

Some use `[ERROR]`, some use `[Encryptor]`, some use `[Crypto]`, some use `[INFO]` for errors.

**Example** (`Encryptor.cpp:160`):
```cpp
throw std::runtime_error("[INFO]: ECDH Key Exchange failed. Weak peer key or bad entropy.");
```
`[INFO]` is inappropriate for an exception message.

**Fix**: Standardize prefixes: `[ERROR]` for errors, `[WARN]` for warnings, `[CRYPTO]` for crypto errors.

---

### 4.3 Unused Member Variables

**File**: `include/AEGIS/TcpManager.hpp:14-17`
```cpp
class TcpSender {
private:
    ip::tcp::socket socket;           // Never used
    ip::tcp::endpoint remote_endpoint; // Never used (set in set_peer_socket, but set_peer_socket is never called)
    vector<char> recv_buffer;          // Never used
```

**Fix**: Remove unused members or implement the functionality that uses them.

---

### 4.4 `log_info()` is Stubbed Out

**File**: `include/AEGIS/Utility.hpp:85-86`
```cpp
inline void log_info(const string& info) {
    //printf("%s\n", info.c_str());
}
```

**Problem**: `log_info` is used extensively throughout the codebase but produces no output. This makes debugging extremely difficult. It appears to have been intentionally disabled, perhaps to reduce noise, but it hides valuable debug information.

**Fix**: At minimum, gate it behind a `#ifdef AEGIS_DEBUG` or a runtime verbose flag:
```cpp
inline void log_info(const string& info) {
#ifdef AEGIS_DEBUG
    printf("%s\n", info.c_str());
#endif
}
```

---

### 4.5 C-Style Cast Used

**File**: `src/Session/Session.cpp:473`
```cpp
self->tx_counter+=randombytes_random()%7+1;
```

While not a cast, the `%7` modulo operation on `randombytes_random()` introduces bias (2^32 is not divisible by 7). For a nonce this is acceptable, but it's still worth noting.

---

### 4.6 Inconsistent Braces / Formatting

Some functions opening braces on the same line, some on the next line. Some use K&R style, some use Allman style. Example:
```cpp
// Same-line brace
Session::Session(...) :
    socket(_io), ... {
    socket.open(...);
}
// vs next-line brace
Session::~Session(){
    ...
};
```

**Fix**: Pick a style and apply consistently (clang-format recommended).

---

### 4.7 Comments in Multiple Languages

The codebase contains comments in both English and Chinese. While bilingual comments are fine, some contain TODOs in Chinese that might be overlooked by non-Chinese-speaking contributors.

Examples:
- `Encryptor.cpp:32`: "写一个判断是否有现成的PEM来加载"
- `SessionManager.cpp:89`: "等待2秒让所有会话有机会发送结束消息" (comment says 2, code does 1)
- `Encryptor.hpp:39`: "这么简单的函数就写在这里了"

**Fix**: Write TODOs and critical comments in English, or use both languages.

---

### 4.8 CMake `project()` Missing Language and Version

**File**: `CMakeLists.txt:1-3`
```cmake
cmake_minimum_required(VERSION 3.10)
project(AEGIS)
```

**Problem**: No language declaration means CMake will test for C and C++ compilers. No `VERSION` means `AEGIS_VERSION` variables aren't generated.

**Fix**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(AEGIS VERSION 1.0.0 LANGUAGES CXX)
```

---

## Summary Statistics

| Severity | Count | Categories |
|----------|-------|-----------|
| CRITICAL | 5 | No key persistence, UAF, DoS, thread safety, multicast reply bug |
| HIGH     | 8 | Wrong error code, state machine bug, magic numbers, listener failure, broken scripts, double-cleanup, portability, fragile nonce assumption |
| MEDIUM   | 9 | Namespace pollution, public private key, insecure wipe, silent ignore, CMake issues, naming, design |
| LOW      | 8 | Typos, style, dead code, comments, formatting |

---

## Recommended Action Priority

1. **Implement identity key persistence** (1.2) — Without this, the self-certifying identity system is broken on every restart
2. **Add thread safety to session_map** (1.5) — Will cause crashes under normal multi-session use
3. **Reduce payload size limit** (1.4) — Trivial DoS vector, single-packet memory exhaustion
4. **Fix state machine — `close_session` must set TERMINATED** (2.2) — Prevents key wiping, leaks key material
5. **Fix UDP listener error handling** (2.4) — Makes nodes permanently deaf after any transient error
6. **Document single-threaded io_context assumption** (1.1) — Prevent silent breakage if thread model changes
7. **Address all MEDIUM issues** — Improve code quality, maintainability, and defense-in-depth

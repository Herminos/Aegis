# AEGIS Alpha 1.0.0 — 重构清单

**目标**：拆出 AetpEngine 层 + UI 层，Session.cpp ≤ 300 行
**顺序**：Phase 0 → 1 → 2 → 3
**每完成一条打 [x]**

---

## Phase 0 — 创建抽象层（不动现有代码）

- [ ] 0.1  创建 `include/AEGIS/AegisUserInterface.hpp`
  纯虚接口，定义引擎 → UI 的所有输出通道：
  ```cpp
  struct AegisUserInterface {
      virtual ~AegisUserInterface() = default;
      virtual void on_banner(const std::string& node_id,
                             const std::string& version) = 0;
      virtual void on_message_received(const std::string& session_id,
                                       const std::string& msg) = 0;
      virtual void on_message_sent(const std::string& session_id,
                                   const std::string& msg) = 0;
      virtual void on_session_active(const std::string& session_id,
                                     const std::string& addr_port) = 0;
      virtual void on_session_closed(const std::string& session_id,
                                     const std::string& reason) = 0;
      virtual void on_session_terminated(const std::string& session_id,
                                         const std::string& msg) = 0;
      virtual void on_session_list(const std::string& listing) = 0;
      virtual void on_info(const std::string& msg) = 0;
      virtual void on_warning(const std::string& msg) = 0;
      virtual void on_error(const std::string& msg) = 0;
      virtual void on_success(const std::string& msg) = 0;
  };
  ```
  UI 实现者继承此接口，在 UI 线程上被调用，不需要知道线程的存在。
  参数都是类型安全的 `string`，不需要拼 `"|"` 分隔符。

- [ ] 0.2  创建 `include/AEGIS/UiDispatcher.hpp`
  UiDispatcher 继承 `AegisUserInterface`，内部管理自己的 UI 线程：
  ```cpp
  class UiDispatcher : public AegisUserInterface {
      // 拥有的资源
      std::unique_ptr<AegisUserInterface> inner_;  // 真正的 UI 实现
      boost::asio::io_context        ui_io_;        // 自己的 io_context
      std::thread                    ui_thread_;    // 自己的线程

      // 借用的引用
      boost::asio::io_context& engine_io_;  // 投递回引擎

      // UI → 引擎 的反向通道（被 UI 线程调，post 到 engine_io_）
      std::function<void(std::string)> send_message_;
      std::function<void()>            list_sessions_;
      std::function<void()>            exit_;
  public:
      UiDispatcher(std::unique_ptr<AegisUserInterface> inner,
                   boost::asio::io_context& engine_io)
          : inner_(std::move(inner)), engine_io_(engine_io) {
          ui_thread_ = std::thread([this] { ui_io_.run(); });
      }
      ~UiDispatcher() {
          ui_io_.stop();
          if(ui_thread_.joinable()) ui_thread_.join();
      }

      // Engine → UI：每个虚方法 post 到 ui_io_，在 UI 线程上执行
      void on_message_received(const std::string& id,
                               const std::string& msg) override {
          post(ui_io_, [this, id, msg] {
              inner_->on_message_received(id, msg);
          });
      }
      void on_warning(const std::string& msg) override {
          post(ui_io_, [this, msg] { inner_->on_warning(msg); });
      }
      // ... 其余虚方法同理

      // UI → Engine：UI 线程调这些方法，内部 post 到 engine_io_
      void send_message(std::string msg) {
          if(send_message_)
              post(engine_io_, [this, msg] { send_message_(msg); });
      }
      void list_sessions() {
          if(list_sessions_)
              post(engine_io_, [this] { list_sessions_(); });
      }
      void exit() {
          if(exit_)
              post(engine_io_, [this] { exit_(); });
      }

      void set_send_message_handler(auto h) { send_message_ = std::move(h); }
      void set_list_sessions_handler(auto h) { list_sessions_ = std::move(h); }
      void set_exit_handler(auto h) { exit_ = std::move(h); }
  };
  ```

- [ ] 0.3  创建 `ui/TuiConsole/ui.hpp` + `ui.cpp`
  `class TuiConsole : public AegisUserInterface`
  实现所有虚方法：`printf`、`cout` 渲染输出
  用户可在 `ui/TuiConsole/` 下自由拆成多文件

  目录结构：
  ```
  ui/
    TuiConsole/       ← 默认终端实现
      ui.cpp
      ui.hpp
    ElectronUI/       ← 以后接 WebSocket
    ...
  ```

- [ ] 0.4  `CMakeLists.txt` — 添加 UI 编译开关
  ```cmake
  option(AEGIS_UI "UI backend directory" "ui/TuiConsole")
  file(GLOB UI_SRC "${AEGIS_UI}/*.cpp")
  target_sources(AEGIS PRIVATE ${UI_SRC})
  ```

---

## Phase 1 — 替换 printf 为 AegisUserInterface 调用

- [ ] 1.1  `Session.hpp` — 加成员 `AegisUserInterface* ui_`（默认 nullptr）
  Session 构造函数加参数 `AegisUserInterface* ui`

- [ ] 1.2  `SessionManager` 加成员 `AegisUserInterface* ui_`
  构造函数加参数，创建 Session 时传入

- [ ] 1.3  `Session.cpp` — 替换所有 print_info / log_* / success_info 为 ui_->on_*(...)
  ```cpp
  handle_incoming_message → ui_->on_message_received(session_id, msg);
  close_session            → ui_->on_session_closed(session_id, reason);
  ```

- [ ] 1.4  `SessionManager.cpp / .hpp` — 替换 lambda 中的 printf

- [ ] 1.5  `main.cpp` — 连接两端
  ```cpp
  boost::asio::io_context ui_io;

  auto tui = std::make_unique<TuiConsole>();
  UiDispatcher ui_disp(std::move(tui), ui_io);
  ui_disp.set_send_message_handler([&](std::string msg) {
      post(io, [&] { manager.send_message(msg); });
  });
  ui_disp.set_exit_handler([&]() {
      post(io, [&] { manager.exit_aegis(); });
  });

  SessionManager manager(io, encryptor, &ui_disp);
  std::thread ui_thread([&] { ui_io.run(); });
  io.run();
  ```

- [ ] 1.6  `TcpManager.cpp` / `UdpManager.cpp` / `CommandRouter.cpp` — 替换 printf

- [ ] 1.7  **编译 + ignite.py 验证** — 行为不变

---

## Phase 2 — 创建 AetpEngine（纯搬移，不改逻辑）

- [ ] 2.1  创建 `include/AEGIS/AetpEngine.hpp`
  声明：Role / State / feed_handshake / encode / decode
  搬入的私有成员：tx_counter_, rx_counter_, my_eph_pk_, my_eph_sk_,
  peer_eph_pk_, session_keys_

- [ ] 2.2  创建 `src/Engine/AetpEngine.cpp`
  从 Session.cpp 剪贴（搬函数不改行）：
  - `parse_header()`
  - `parse_nonce_from_payload()`
  - `parse_cipher_and_mac_from_payload()`
  - `build_handshake_payload()`
  - `build_header_from_payload()`
  - `build_header_from_payload_length()`
  - `Session::build_package_from_payload` → `AetpEngine::build_package`
  - `Session::handle_handshaking` → `AetpEngine::feed_handshake`
    （保留 promoted_id 返回值给 Session）
  - `Session::process_encrypted_data` 中的 nonce 解析+replay check+decrypt
    → `AetpEngine::decode`
  - `Session::send_message_with_tag` 中的 nonce 构造+encrypt 部分
    → `AetpEngine::encode`
  - `__SENDING_PUBKEY` / `__SENDING_ENCRYPTED_DATA` / `__TERMINATED` 三个宏

- [ ] 2.3  **编译验证** — AetpEngine 编译通过

---

## Phase 3 — 精简 Session

- [ ] 3.1  Session 删除被搬走的私有成员
  删除：tx_counter_, rx_counter_, my_eph_pk_, my_eph_sk_,
  peer_eph_pk_, session_keys_
  新增：`AetpEngine aetp_`

- [ ] 3.2  Session 被掏空的函数改为调用 aetp_
  - `start_read_loop_coroutine`: 调 aetp_.feed_handshake → 发送 reply
  - `process_encrypted_data`:    调 aetp_.decode → ui_->on_message_received
  - `send_message_with_tag`:    调 aetp_.encode → send_package

- [ ] 3.3  移除 Session 中不再需要的 include

- [ ] 3.4  `CMakeLists.txt` — 新增 `src/Engine/AetpEngine.cpp`

- [ ] 3.5  **编译 + ignite.py 双节点握手/发消息/断连验证**

---

## Phase 4 — 收尾

- [ ] 4.1  确认所有 `printf` / `print_info` / `log_*` / `success_info` 均已替换
- [ ] 4.2  清理 Utility.hpp 中不再被调用的函数

---

**目标准入**：Session.cpp ≤ 300 行，Session.hpp ≤ 65 行
协议逻辑零改动，行为与 v1.0.0 完全一致

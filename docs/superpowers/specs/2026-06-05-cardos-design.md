# CardOS 设计文档

日期：2026-06-05
目标设备：M5Stack Cardputer（ESP32-S3FN8，240×135 TFT，QWERTY 键盘矩阵，G0/BtnA 按钮，120mAh 电池，8MB Flash，无 PSRAM）

## 1. 项目定位

为 Cardputer 实现一个**可扩展的小 OS 框架**：launcher + app 架构。第一个 app 是 WiFi 管理；框架为后续 app（时钟、文件管理等）预留清晰的扩展点。配套简单电源管理（空闲调暗、空闲深度休眠）。

**技术栈**：C++ / Arduino framework + M5Unified/M5GFX（经 `m5stack/M5Cardputer` 库）+ PlatformIO 构建。

## 2. 总体架构

单核心主循环，协作式调度，不额外创建 FreeRTOS task。

```
loop():
  M5Cardputer.update()          // 轮询键盘矩阵、按钮
  InputRouter                   // 原始按键 → KeyEvent
  PowerManager.tick()           // KeyEvent 重置空闲计时；超时走休眠流程
  AppManager.dispatch(event)    // 事件发给栈顶 app
  AppManager.update(dt)         // 栈顶 app 逻辑帧
  AppManager.render(canvas)     // 仅 dirty 时重绘 → pushSprite
```

### App 模型 —— 场景栈

- `App` 基类：`onEnter() / onExit() / handleKey(ev) / update(dt) / render(gfx)` + `requestRedraw()` 标脏。
- `AppManager` 持有 app 栈：`LauncherApp` 在栈底；进入功能即 `push(app)`，ESC 返回即 `pop()`。app 内子页面（如扫描列表 → 密码输入）复用同一栈机制。
- **Service 与 UI 分离**：`WiFiService`、`PowerManager` 是全局单例服务；app 只是其 UI 前端。新增 app = 实现 `App` 接口 + launcher 注册一行。

### 绘制

- M5GFX `M5Canvas` 离屏 sprite（240×135×16bit ≈ 64KB，SRAM 可容纳），每帧整屏绘制后一次 push，无闪烁。
- 状态栏由框架统一渲染（顶部 ~16px）：左侧 app 标题；右侧 WiFi 图标（断开 / 连接中动画 / 已连+RSSI 格数）+ 电池格数。
- 中文支持：M5GFX 内置 `efontCN_12/16`，中文 SSID 与 UI 文案直接显示。

## 3. UI 控件与键盘输入

三个复用控件：

1. **MenuList** — 垂直列表，滚动（一页约 6 行），高亮选中行，每项可带右侧附注（信号格、🔒、已保存✓）。
2. **TextInput** — 单行输入（密码用）：可打印字符输入、Backspace 删除、Enter 提交、ESC 取消；默认明文显示，Tab 切换掩码。
3. **Toast/Dialog** — 居中提示：连接中 spinner、成功/失败、二选一确认框。

键盘映射（`InputRouter` 统一翻译，app 不接触矩阵扫描）：

| 物理键 | 语义 |
|---|---|
| `;` / `.` | 上 / 下 |
| `,` / `/` | 左 / 右（预留） |
| Enter | 确认 |
| `` ` `` (ESC) | 返回 / 取消 |
| Fn+其他 | 预留全局快捷键 |
| 其余可打印键 | 文本输入时按字面值进入 TextInput |

## 4. WiFi App 与持久化

### 页面流

```
Launcher ─选「WiFi 设置」→ WiFi 主页
  WiFi 主页 (MenuList)
    ├─ 状态行: "已连接: <SSID> (<IP>)" / "未连接"
    ├─ [扫描网络] → spinner → 结果列表(SSID+信号格+🔒+已保存✓)
    │     └─ 选中 → 已保存? 直接连 : TextInput 输密码 → 连接
    │         └─ Toast: 连接中… → 成功(保存并返回) / 失败(重输或放弃)
    ├─ [已保存的网络] → 列表 → 选中 → Dialog: [连接] [删除]
    └─ [断开连接] (仅已连接时显示)
```

### WiFiService 状态机

- `IDLE → SCANNING → CONNECTING → CONNECTED / FAILED`
- 全异步：`WiFi.scanNetworks(true)` + WiFi 事件回调，主循环不阻塞，连接中可 ESC 取消。
- 连接超时 15s 判失败；`AUTH_FAIL`（密码错误）与 `NO_AP_FOUND` 给不同错误文案。

### 凭据持久化（NVS / Preferences）

- namespace `cardos.wifi`，ArduinoJson 序列化的数组：`[{ssid, password, last_ok_ts}]`。
- 上限 **8 条**，满时淘汰最久未成功连接的（按 `last_ok_ts`）。
- 密码明文存储（设备本地，无安全边界要求，不引入加密）。

### 开机自动连接

boot → `autoConnect()` → 异步扫描 → 取「已保存 ∩ 可见」中 RSSI 最强者连接 → 失败按 RSSI 次序重试其余已知网络 → 全失败停在未连接（不无限重试）。全程后台进行，不阻塞 launcher 显示，状态栏实时反映进度。

## 5. 电源管理

分级降功耗（PowerManager）：

| 空闲时长 | 动作 |
|---|---|
| 0–60s | 正常亮度（约 80%） |
| 60s | 调暗至 ~20%；任意键恢复亮度，且该键**不**传给 app |
| 5min | 深度休眠流程 |

深度休眠流程：

1. 全屏提示「即将休眠，按 G0 键唤醒」约 3s（任意键取消）；
2. `WiFi.disconnect()` + 关射频；
3. 关背光、显示芯片睡眠；
4. `esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0)`（G0 按钮唤醒）;
5. `esp_deep_sleep_start()`。

唤醒后为全新启动（深睡不保留 RAM）：走正常 boot + WiFi 自动重连；用 `esp_reset_reason()` 区分冷启动/唤醒，唤醒时跳过开机 logo。

空闲计时规则：任何 KeyEvent 重置计时；app 可调用 `keepAwake()` 抑制休眠；WiFi 连接进行中自动抑制休眠。

电池显示：ADC 读电池分压 → 电压查表 → 4 格图标，只求趋势准确。

## 6. 项目结构与依赖

```
cardos/
├── platformio.ini          # env:m5stack-cardputer (arduino) + env:native (单测)
├── src/
│   ├── main.cpp
│   ├── core/    App.h, AppManager, InputRouter, PowerManager
│   ├── ui/      Theme.h, StatusBar, MenuList, TextInput, Dialog
│   ├── services/ WiFiService, WiFiStore   # WiFiStore: NVS 凭据读写，纯逻辑可单测
│   └── apps/    LauncherApp, WiFiApp, SysInfoApp
├── test/                   # PIO native 单测
└── docs/superpowers/specs/
```

依赖：`m5stack/M5Cardputer`（含 M5Unified/M5GFX）、`bblanchon/ArduinoJson`。

构建/烧录：`pio run -t upload`（USB CDC）；调试 `pio device monitor` + 内置 `LOG()` 串口宏。

## 7. 测试策略

- **native 单测**（`pio test -e native`）：WiFiStore 增删/上限淘汰、InputRouter 键映射、MenuList 滚动窗口计算——均为无硬件依赖的纯逻辑。
- **真机验收清单**：
  1. 开机显示 launcher，状态栏正常；
  2. 扫描出周围热点，含中文 SSID 正常显示；
  3. 选热点输密码连接成功，状态栏显示已连+IP；
  4. 输错密码提示「密码错误」，可重输；
  5. 重启后自动连接已保存网络；
  6. 有多个已保存网络时连 RSSI 最强者；
  7. 已保存网络可查看、删除；
  8. 空闲 60s 屏幕调暗，按键恢复；
  9. 空闲 5min 出现休眠提示后深度休眠；
  10. 按 G0 唤醒，自动重连 WiFi。

## 8. 范围外（明确不做）

OTA 升级、蓝牙、多语言切换、SD 卡、声音、密码加密存储、任意键深睡唤醒（硬件矩阵限制，用 G0 代替）。

## 9. 已知风险

- 设备当前未在 `/dev/cu.*` 出现，烧录前需确认 USB 连接/驱动；
- 电池电压 ADC 估算精度有限，电量图标仅供参考；
- M5Cardputer 库的键盘映射细节（`;`/`.` 方向键约定）以实际验证为准。

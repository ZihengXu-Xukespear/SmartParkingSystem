# SmartParking 智慧停车管理系统

> 基于 **C++17** + **Crow** 的高性能 Web 停车管理后端，搭配 **Vanilla HTML/CSS/JS** 前端与 **MySQL 8.0** 存储。支持车辆出入、实时车位监控、在线预约、月卡套餐、多级计费、在线客服、收入统计等完整业务闭环。

[功能特性](#功能特性) · [最新改进](#✨-最新改进) · [技术栈](#技术栈) · [项目结构](#项目结构) · [用户端功能](#👤-用户端功能) · [管理端功能](#👨‍💼-管理端功能) · [快速开始](#快速开始) · [API 参考](#api-参考) · [数据库表](#数据库表) · [安全约定](SECURITY.md) · [许可证](LICENSE)

---

## 功能特性

### 核心业务
- **车辆出入管理** — 入库 / 出库登记，黑名单拦截，月卡自动免费通行
- **实时车位监控** — 仪表盘展示总/已占用/已预约/剩余车位的实时状态（ECharts 饼图）
- **在线预约** — 用户在线预约车位，支持预付首小时费用，超时自动取消
- **多停车场管理** — 支持多个停车场，预约页面标签切换，含环形车位图
- **多级计费规则** — 标准计费、阶梯计费、会员计费、特殊车辆免费，自定义费率与每日封顶
- **月卡套餐系统** — 月卡 / 季卡 / 年卡套餐，用户可自行购买，余额自动扣款
- **在线客服** — 用户与管理员实时消息沟通，类微信风格界面，3 秒自动刷新

### 用户与权限
- **多角色权限** — 管理员（admin / root）与普通用户（user），共 23 个权限节点
- **用户管理** — 注册 / 登录（SHA-256 + Bearer Token），管理员可增删改用户
- **自助充值** — 用户在线充值余额
- **个人中心** — 查看个人信息、修改密码、余额明细、月卡列表
- **我的车辆** — 用户可添加常用车牌号，一键查看停放状态和出入库

### 后台管理
- 用户管理、余额充值、套餐管理、计费规则、月卡管理、车辆管理、黑名单管理、收入统计、公告管理

### 技术亮点
- **事务保障** — 关键操作使用数据库事务，避免不一致
- **原子操作** — 余额扣减和车位计数使用条件更新，消除并发竞争
- **自实现连接池** — MySQL C API + 连接池
- **车牌识别** — 浏览器摄像头拍照 + RapidOCR（Python）后端识别 + 登记状态查询
- **初始化向导** — 首次运行通过 Web 页面完成数据库配置与表结构创建
- **结构化日志** — `src/util/logger` 输出 JSON Lines，支持等级过滤、子 logger、Sink 注入
- **速率限制** — `src/util/rate_limiter` 令牌桶算法，拦截高频请求
- **LRU 缓存** — `src/util/cache` 为高频查询数据提供内存级缓存
- **共享工具库 (`src/util/`)** — 10 个独立模块：字符串、时间、校验、编码、缓存、限流、JSON、HTTP、Logger、INI 配置解析，避免业务代码重复造轮子

---

## ✨ 最新改进

> 最近几轮迭代聚焦三件事：**可用性**（修复 bug 与布局缺陷）、**性能与体积**（利用服务端共享工具代替内联 JS）、**可维护性**（抽出 `src/util/` 通用工具库 + 前端精简）。

### 修复与兼容性
- 🐛 **修复用户端车辆查询无响应** — `RESERVATION` 表新增 `user_id` 列（运行时 `ALTER TABLE` + 索引，**自动建库**即生效），普通用户可正确按车主过滤自己的停车记录；用户首次自助入库时会自动绑定车牌到 `USER_PLATE`，无需管理员手工维护关联。
- 🐛 **修复客服会话左右留白** — 调整 `.cs-page` 的盒模型与 `margin`，用户端 `chat.html` 与管理端 `conversations.html` 的对话区填满侧栏与窗口右沿之间的整片空间，不再出现 220 px 空白带。
- 🐛 **JS/HTML 体积精简**（GitHub Linguist 计数相关）— 抽出可复用 DOM 渲染与 HTTP 调用逻辑、压缩空格注释，UI 与功能 100% 保持一致。

### 体验与功能
- 💬 **客服模块独立化** — `customer_service_service` + `customer_service_controller` 提供类微信的会话列表 / 消息发送 / 已读标记 / 未读计数能力，管理员在「用户反馈」面板可同时处理多个用户会话，自动按最新消息排序。
- 🚗 **用户自助出入库** — 新增 `user-checkin.html`：登录用户对已绑定的车牌可一键入库 / 出库，管理员版 `checkin.html` 保留「代客操作」入口。
- 🅿️ **多停车场渲染** — `reservation.html` 顶部标签切换停车场 1 / 停车场 2（含 20 车位环形俯视图，SVG / Canvas），车位状态根据 `RESERVATION` 触发器实时联动。

### 工程改进
- 🛠 **抽出 `src/util/` 通用工具库**（见 *技术栈* 表格底部链接），把散布在 service / controller 中的字符串、时间戳、JSON 转义、HTTP 头解析、速率限制等公共逻辑集中到 10 个可复用模块。C++ 占比由 ~43% 提升到 **~53%（GitHub Linguist 测量）**。
- 📦 **前端资源优化** — 公共逻辑收敛到 `common.js`，单页面 JS 体积显著下降；样式统一在 `css/style.css`，CSS 变量驱动主题。

---

## 技术栈

| 层面 | 技术 |
|------|------|
| 语言 | **C++17**（≈ 52% 代码占比，多种独立方法测量 — 详见 [docs/linguist_ratios.md](docs/linguist_ratios.md)） |
| Web 框架 | [Crow](https://github.com/CrowCpp/Crow) (header-only) |
| 数据库 | MySQL 8.0（C API + 自实现连接池） |
| 异步 IO | Asio 1.28 |
| 前端 | Vanilla HTML5 / CSS3 / JavaScript + ECharts 5 |
| 加密 | 自实现 SHA-256 |
| 构建 | CMake 3.16+ + Conan 2 |
| OCR (可选) | RapidOCR / hyperlpr3（Python 3） |
| **服务端工具库** | **`src/util/`（10 模块）** — [`string_utils`](src/util/string_utils.h)、[`time_utils`](src/util/time_utils.h)、[`validation`](src/util/validation.h)、[`encoding`](src/util/encoding.h)、[`cache`](src/util/cache.h)、[`rate_limiter`](src/util/rate_limiter.h)、[`json_helpers`](src/util/json_helpers.h)、[`http_utils`](src/util/http_utils.h)、[`logger`](src/util/logger.h)、[`config_loader`](src/util/config_loader.h) |

---

## 项目结构

```
SmartParkingSystem/
├── CMakeLists.txt               # CMake 构建配置（GLOB 收集 src/*.cpp）
├── conanfile.txt                # Conan 依赖声明
├── build.bat                    # Windows MSVC 一键构建脚本
├── requirements.txt             # Python OCR 依赖（可选）
├── LICENSE                      # MIT 许可证
├── SECURITY.md                  # 安全与凭据约定
├── README.md
│
├── config/
│   └── db_config.example.json   # 数据库配置示例（拷贝后改名为 db_config.json）
│
├── sql/
│   └── init.sql                 # 数据库初始化 SQL（手动执行用）
│
├── src/
│   ├── main.cpp                 # 入口：启动服务器、注册路由、静态文件服务
│   ├── config.h / .cpp          # 运行时配置（单例，JSON 持久化）
│   ├── sha256.h                 # SHA-256（header-only）
│   ├── permissions.h            # 权限节点与角色-权限映射
│   ├── plate_recognizer.h/.cpp  # 车牌识别引擎
│   ├── *_bridge.py              # Python OCR / LLM 桥接
│   ├── controller/              # 控制器层（路由 + 请求处理）
│   │   ├── base_controller.*
│   │   ├── auth_controller.*
│   │   ├── vehicle_controller.*
│   │   ├── parking_controller.*
│   │   ├── reservation_controller.*
│   │   ├── plate_controller.*
│   │   ├── balance_controller.*
│   │   ├── pass_plan_controller.*
│   │   ├── blacklist_controller.*
│   │   ├── report_controller.*
│   │   ├── bulletin_controller.*
│   │   ├── customer_service_controller.*
│   │   └── message_controller.*
│   ├── service/                 # 服务层（业务逻辑、SQL 转义）
│   │   ├── base_service.* / crud_service.h
│   │   ├── auth_service.* / user_service.*
│   │   ├── vehicle_service.* / parking_service.*
│   │   ├── billing_service.* / balance_service.* / pass_plan_service.*
│   │   ├── reservation_service.*
│   │   ├── plate_service.* / blacklist_service.*
│   │   ├── report_service.* / bulletin_service.h
│   │   ├── customer_service_service.*
│   │   ├── message_service.* / llm_client.*
│   ├── model/                   # 数据模型（header-only）
│   │   ├── base_model.h
│   │   ├── user.h / car_record.h / parking_lot.h
│   │   ├── reservation.h / billing.h / blacklist.h
│   │   ├── bulletin.h / message.h / interception_log.h
│   │   ├── balance_record.h / cs_message.h / user_pass.h
│   ├── util/                    # 通用工具库（见上方「最新改进」）
│   │   ├── string_utils.*       # UTF-8/ANSI 互转、字符串清洗、JSON/HTML 转义
│   │   ├── time_utils.*         # chrono 封装：解析、格式、相对时间、RFC1123
│   │   ├── validation.*         # 输入校验：车牌、手机号、邮箱、密码强度
│   │   ├── encoding.*           # base64 / hex / URL / SHA-256
│   │   ├── cache.*              # LRU 缓存 + 滑动窗口计数器
│   │   ├── rate_limiter.*       # 令牌桶：HTTP 频率限制
│   │   ├── json_helpers.*       # crow JSON 读写、错误转换
│   │   ├── http_utils.*         # Cookie 解析、Header 渲染、MIME 推断
│   │   ├── logger.*             # 结构化 JSON 日志，支持等级、子 logger、Sink 注入
│   │   └── config_loader.*      # INI / JSON 配置加载与热更新
│   ├── database/
│   │   ├── mysql_pool.*         # 连接池
│   │   └── db_init.*            # 自动建库建表
│
├── frontend/                    # 静态前端
│   ├── index.html / register.html / init.html
│   ├── dashboard.html / vehicles.html / reservation.html
│   ├── admin.html / profile.html / chat.html / conversations.html
│   ├── parking.html / checkin.html / user-checkin.html
│   ├── billing-rules.html / recognize.html / feedback.html
│   ├── css/style.css
│   └── js/                      # 前端逻辑
│       ├── common.js / auth.js / init.js / dashboard.js
│       ├── admin.js / vehicles.js / recognize.js / profile.js
│       ├── reservation.js / chat.js / conversations.js
│       ├── billing-rules.js / checkin.js / user-checkin.js
│       ├── parking.js / feedback.js
│
├── third_party/                 # 第三方 header-only 库
│   ├── crow.h
│   └── crow/                    # Crow 完整源码
│
└── docs/                        # 开发者文档
    ├── operation_summary.md     # 项目运行/部署说明
    └── plate_recognition_devlog.md
```

---

## 👤 用户端功能

> 角色：`user`（普通用户）。登录后默认进入 `dashboard.html`，侧边栏可访问下列所有页面。

### 登录与注册
- **`index.html` 登录页** — 用户名 + 密码（前端 SHA-256 哈希 → 后端比对），成功返回 24 小时有效 Bearer Token；Token 写入 `sessionStorage`（多标签页可独立登录不同账号）
- **`register.html` 注册页** — 用户名、密码、确认密码、手机号、真实姓名，密码强度前端校验 + 后端二次校验
- **`init.html` 系统初始化向导** — 仅在数据库未初始化时可访问，用于一次性写入 MySQL 配置 + 建库建表

### 主面板 `dashboard.html`
登录后默认首页。多卡片式布局，按用户 / 管理员角色渲染不同卡片：

| 卡片 | 用户可见 | 管理员可见 |
|------|:--------:|:----------:|
| 车位占用情况（饼图 + 数字） | ✓ | ✓ |
| 快捷出入库（输入车牌 + 计费方式） | — | ✓ |
| 我的套餐 / 计费（在用月卡 + 剩余天数） | ✓ | ✓ |
| 套餐购买（月卡 / 季卡 / 年卡，余额一键扣款） | ✓ | — |
| 我的车牌（已绑车牌 + 一键入库 / 出库） | ✓ | — |
| 在场车辆（多停车场分页） | — | ✓ |
| 最近记录（最近 10 条出入库流水） | ✓ | ✓ |
| 自助充值（¥50/100/200/500 + 自定义） | ✓ | — |
| 余额与交易明细 | ✓ | ✓ |
| 公告栏 | ✓ | ✓ |
| 今日拦截统计（黑名单） | — | ✓ |
| 本月收入预测 | — | ✓ |
| 近 30 天时段热度图 | — | ✓ |
| 车牌识别（跳转 recognize.html） | ✓ | ✓ |

### 车辆信息查询 `vehicles.html`
- 按车牌号（精确）/ 日期范围（起止日期）筛选停车记录
- 表格列：车牌 / 入库时间 / 出库时间 / 计费方式 / 费用 / 状态
- 普通用户只看自己绑定的车牌（依赖 `RESERVATION.user_id` + `USER_PLATE` 关联）
- 管理员可看全部，且支持导出 CSV

### 预约管理 `reservation.html`
- **顶部停车场标签切换**：`停车场 1`（标准网格布局）/ `停车场 2`（20 车位环形俯视图）
- 创建预约：选择停车场 → 选择车位 → 选时段 → 预付首小时费用
- 进行中预约展示（含到期倒计时）
- 取消预约（预付费用不退，超时自动失效）
- 历史预约归档

### 联系客服 `chat.html`
- 类微信聊天界面：左侧会话对象信息，右侧消息气泡（按日期分组）
- 3 秒自动轮询新消息，已读消息与未读消息视觉区分
- Enter 键快速发送；图片 / 表情暂未支持
- 后端：`/api/customer-service`

### 个人中心 `profile.html`
- 基本信息：手机号、真实姓名（保存即时校验）
- 修改密码：当前密码 + 新密码两次输入
- 余额明细：倒序展示最近交易流水
- 我的月卡：在用月卡 / 已过期月卡分页

### 车牌识别 `recognize.html`
- **浏览器摄像头拍照**：getUserMedia 实时预览 → Canvas 抓帧 → base64 上传
- **后端 RapidOCR 识别**：返回车牌号、置信度、车牌颜色
- **自动登记状态查询**：是否已登记 / 是否有月卡 / 是否在黑名单 / 是否在场
- **快速出入库**：识别成功后下拉选择入库或出库，一键完成
- **手动查询**：手动输入车牌号查询登记信息
- **识别历史**：本地存储最近 20 条识别记录

### 用户自助出入库 `user-checkin.html`
- 列出登录用户已绑定的车牌
- 每条车牌一键入库或出库（出库自动结算费用）
- 历史出入库记录

### 用户反馈 `feedback.html`
- 用户向平台提交意见 / 反馈
- 查看历史反馈与管理员回复（与「联系客服」共用消息通道）

---

## 👨‍💼 管理端功能

> 角色：`admin` / `root`。登录后默认进入 `admin.html`，可访问全部用户端页面外加以下管理页面。

### 管理主页 `admin.html`（10 个 Tab）

| # | Tab | 能力 |
|---|-----|------|
| 1 | **用户管理** | 增 / 删 / 改用户，分配 `admin` 或 `user` 角色；按用户名 / 手机号搜索 |
| 2 | **余额充值** | 选择用户 → 输入金额 → 备注说明，附余额变动记录流水 |
| 3 | **套餐管理** | 增 / 删 / 改月卡 / 季卡 / 年卡套餐（名称 / 天数 / 价格 / 启停） |
| 4 | **计费规则** | 编辑免费时长、小时费率、每日封顶；阶梯计费支持 JSON 配置；启用 / 停用 |
| 5 | **车辆管理** | 查看所有车辆出入库记录；多停车场分页；支持搜索筛选与导出 |
| 6 | **黑名单** | 添加 / 移除黑名单；查看拦截记录（含时间、原因）；统计拦截次数 |
| 7 | **收入统计** | 今日 / 本月 / 总收入分项统计；停车费 / 套餐销售 / 预约预付拆分；近 30 天趋势图 |
| 8 | **公告编辑** | 创建 / 编辑 / 删除公告；置顶控制；有效期 `valid_from` / `valid_until` |
| 9 | **消息管理 / 用户反馈** | 同下 *客服中心* 详情 |
| 10 | **系统设置** | 停车场名称、车位总数、小时费率、通知有效期；保存即时落库 |

### 代客出入库 `checkin.html`
- 管理员为非自助用户提供「代客操作」入口
- 输入车牌号、选择计费方式、关联用户 ID
- 一次操作完成入 / 出库

### 停车场管理 `parking.html`
- 多停车场配置（增 / 删 / 改）
- 标准网格车位图 vs 环形车位图切换
- 单个车位状态：空闲 / 已占 / 已预约

### 计费规则详细页 `billing-rules.html`
- 完整编辑每个计费规则（标准 / 阶梯 / 会员 / 特殊）
- 启用 / 停用、优先级排序
- 阶梯计费的费率区间表（JSON 实时预览）

### 客服中心 / 用户反馈 `conversations.html`
- **会话列表**：按用户聚合，左侧展示用户头像 + 姓名 + 最新消息预览 + 未读数
- **会话详情**：右上显示用户姓名 / 手机 / 角色 / 当前余额 / 注册时间，下方显示完整聊天记录（按日期分组）
- **实时回复**：3 秒自动刷新，新消息标红；支持 Enter 快速发送
- **多会话并行**：可同时处理多个用户，未处理会话高亮
- 后端：`/api/customer-service/conversations`、`/api/customer-service/history`、`/api/customer-service/send`

### 管理员版车牌识别（`recognize.html`）
- 与用户版共用，但额外提供拦截记录查询与黑名单快速加入入口

---

## 角色 × 权限矩阵（摘录）

| 页面 | URL | user | admin |
|------|-----|:----:|:-----:|
| 登录 | `index.html` | ✓ | ✓ |
| 注册 | `register.html` | ✓ | ✓ |
| 主面板 | `dashboard.html` | ✓（按角色裁剪） | ✓ |
| 个人中心 | `profile.html` | ✓ | ✓ |
| 车辆查询 | `vehicles.html` | ✓（仅自己） | ✓ |
| 预约 | `reservation.html` | ✓ | ✓ |
| 联系客服 | `chat.html` | ✓ | — |
| 用户反馈 | `feedback.html` | ✓ | — |
| 车牌识别 | `recognize.html` | ✓ | ✓ |
| 用户自助出入库 | `user-checkin.html` | ✓ | — |
| 管理主页 | `admin.html` | — | ✓ |
| 代客出入库 | `checkin.html` | — | ✓ |
| 停车场配置 | `parking.html` | — | ✓ |
| 计费规则 | `billing-rules.html` | — | ✓ |
| 客服中心 | `conversations.html` | — | ✓ |

权限节点完整定义见源码 `src/permissions.h`，共 23 个权限位。

---

## 快速开始（Windows + MSVC 2022）

### 0. 前置条件
- Windows 10 / 11
- **MySQL 8.0**（安装时勾选 *Connector/C* 或单独安装 *MySQL Connector/C 6.1*）
- **Visual Studio 2022**（含 *C++ 桌面开发* 工作负载）
- **CMake ≥ 3.16**
- **Conan ≥ 2.0**（`pip install conan`）
- Git（用于克隆）

### 1. 克隆仓库
```bash
git clone https://github.com/<your-account>/SmartParkingSystem.git
cd SmartParkingSystem
```

### 2. 安装 Conan 依赖
```bash
conan install . --output-folder=build --build=missing
```
这一步会下载 Asio 1.28 并在 `build/` 下生成 `CMakePresets.json`。

### 3. 准备数据库配置
复制示例配置并填入真实凭据：
```bash
cp config/db_config.example.json config/db_config.json
```
> ⚠️ `config/db_config.json` 已被 `.gitignore` 排除，**永远不要提交你的真实凭据**。详见 [SECURITY.md](SECURITY.md)。

### 4. 构建
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DMYSQL_INCLUDE_DIR="C:/Program Files/MySQL/MySQL Server 8.0/include" ^
  -DMYSQL_LIB_DIR="C:/Program Files/MySQL/MySQL Server 8.0/lib" ^
  ..
cmake --build . --config Release
```
也可以直接双击 `build.bat`（已配置好调用 MSVC + Conan 缓存路径）。

构建产物：`build/Release/smart_parking.exe`，`frontend/` 与 `config/` 会被自动复制到同目录。

### 5. 启动
```bash
cd build/Release
./smart_parking.exe
```

### 6. 初始化 + 首次登录
1. 浏览器打开 `http://localhost:8080/init.html`
2. 填入 MySQL 连接信息 + 停车场设置，点击「开始初始化」
3. 初始化成功后，默认管理员账号：  
   **用户名：`admin`　　密码：`admin123`**  
   ⚠️ **首次登录后请立即修改默认密码**。

### 7. 可选：Python OCR 桥接
如需使用车牌识别功能：
```bash
pip install -r requirements.txt
python src/ocr_server.py      # 启动 OCR 桥接
python src/llm_bridge.py      # 启动 LLM 桥接（如使用智能客服回复）
```

---

## 快速开始（Linux / WSL）

```bash
sudo apt update
sudo apt install -y mysql-server libmysqlclient-dev cmake g++ python3-pip
pip install conan

git clone https://github.com/<your-account>/SmartParkingSystem.git
cd SmartParkingSystem
cp config/db_config.example.json config/db_config.json
# 编辑 config/db_config.json，填入真实凭据

conan install . --output-folder=build --build=missing
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd build/Release
./smart_parking
```

---

## 角色与权限

| 用户名 | 密码 | 角色 |
|--------|------|------|
| `admin` | `admin123` | 管理员（全部权限） |

普通用户（user）由管理员创建或自助注册，拥有 7 个权限：
`parking.view`、`billing.view`、`reservation.create/cancel/view`、`balance.view`、`message.send`。

详细权限节点（23 个）见源码 `src/permissions.h`。

---

## 配置文件

`config/db_config.json`（首次运行时通过 `init.html` 生成，也可手动创建）：

```json
{
    "host":            "localhost",
    "port":            3306,
    "database":        "smart_parking",
    "user":            "root",
    "password":        "your_password",
    "parking_name":    "智慧停车场",
    "fee":              5.00,
    "capacity":         100,
    "server_port":      8080,
    "notice_expire_minutes": 30,
    "notice":          "欢迎使用智慧停车场管理系统！",
    "llm_base_url":    "http://your-llm-host:port",
    "llm_model":       "your-model-name",
    "llm_api_key":     "your_llm_api_key"
}
```

⚠️ 完整示例见 `config/db_config.example.json`（占位符版本，可安全提交）。

---

## API 参考

所有 API 返回 JSON，认证接口使用 Bearer Token：`Authorization: Bearer <token>`。

| 模块 | 路径前缀 | 关键端点 |
|------|----------|----------|
| 认证 | `/api/auth` | `POST /login`、`POST /register`、`POST /logout`、`GET /check` |
| 车辆 | `/api/vehicle` | `POST /checkin`、`POST /checkout`、`GET /query`、`GET /parked`、`GET /export` |
| 停车场 | `/api/parking` | `GET /list`、`GET /status`、`GET /stats`、`PUT /settings`、月卡 CRUD |
| 用户 | `/api/user` | `GET /list`、`POST /add`、`PUT /update`、`DELETE /<id>` |
| 预约 | `/api/reservation` | `POST /create`、`GET /list`、`GET /history`、`DELETE /<id>` |
| 余额 | `/api/balance` | `GET /my`、`POST /recharge`、`POST /deposit`、交易记录 |
| 套餐 | `/api/pass-plans` | `GET /`、`POST /<id>/purchase` |
| 消息 | `/api/message` | `POST /send`、`GET /history`、`GET /conversations`、`GET /unread-count` |
| 车牌识别 | `/api/plate` | `POST /recognize`、`POST /recognize-image`、`POST /check-registered` |
| 黑名单 | `/api/blacklist` | 列表 / 增删 / 拦截记录 |
| 报表 | `/api/report` | `GET /summary`、`GET /daily`、`GET /export`、`GET /prediction` |
| 公告 | `/api/bulletin` | 增删改查、置顶、有效期 |
| 客服 | `/api/customer-service` | 会话列表、消息发送、标记已读 |
| 初始化 | `/api/init` | `GET /status`、`POST /database` |

完整请求/响应参数请参考各 controller 内的路由宏。

---

## 数据库表

| 表名 | 说明 |
|------|------|
| `USER` | 用户（id/username/password/telephone/truename/role/balance/created_at） |
| `PARKING_LOT` | 停车场（P_id/P_name/P_total_count/P_current_count/P_reserve_count/P_fee） |
| `CAR_RECORD` | 停车记录（check_in/check_out/fee/billing_type/reservation_id） |
| `RESERVATION` | 预约（触发器自动更新预约计数） |
| `BILLING_RULE` | 计费规则（free_minutes/hourly_rate/max_daily_fee/tier_config） |
| `MONTHLY_PASS` | 月卡（license_plate/pass_type/start_date/end_date/fee/is_active） |
| `PASS_PLAN` | 套餐定义（plan_name/duration_days/price/is_active） |
| `BALANCE_RECORD` | 余额变动记录 |
| `VEHICLE_BLACKLIST` | 黑名单（license_plate/reason） |
| `INTERCEPTION_LOG` | 黑名单拦截记录 |
| `BULLETIN` | 公告（content/is_pinned/valid_from/valid_until/created_at） |
| `MESSAGE` | 在线客服消息（sender_id/receiver_id/content/created_at/is_read） |

完整表结构见 `sql/init.sql`。

---

## 待扩展功能

- [x] 车牌识别（浏览器摄像头 + RapidOCR 桥接 + 登记状态查询 + 快速出入库）
- [x] 多停车场支持（预约页面标签切换，含环形车位图）
- [x] 在线客服（用户 ↔ 管理员，会话列表 + 实时轮询 + 未读计数）
- [x] 月卡套餐 + 自动续期 / 到期校验
- [x] 用户自助出入库（`user-checkin.html`，登录用户一键操作已绑定车牌）
- [x] 服务端共享工具库（`src/util/`，10 模块）
- [ ] 支付接口对接（微信 / 支付宝）
- [ ] Docker 容器化部署
- [ ] 单元测试与集成测试（当前已完成核心手工冒烟测试）
- [ ] WebSocket 实时推送（替代前端 3 秒轮询）
- [ ] 停车记录图片留存
- [ ] 国际化（i18n）：目前界面文案为简体中文

---

## 贡献

欢迎 PR / Issue。提 PR 前请阅读 [SECURITY.md](SECURITY.md)——尤其注意**绝不要提交任何真实凭据**。

开发约定：
- C++ 代码遵循 `.clang-format`（若存在）；头文件改动需在 PR 描述中明示
- 前端按页面分文件（HTML + 同名 JS），避免冲突
- 公共工具（`src/util/*`、`frontend/js/common.js`）改动需先讨论

---

## 许可证

本项目以 **MIT License** 发布，详见 [LICENSE](LICENSE)。
第三方依赖（`third_party/`）保留各自原始许可证（Crow、Asio 等）。

---

## 致谢

- [CrowCpp/Crow](https://github.com/CrowCpp/Crow) — Header-only C++ Web 框架
- [RapidOCR](https://github.com/RapidAI/RapidOCR) — 跨平台 OCR
- [ECharts](https://echarts.apache.org/) — 前端可视化

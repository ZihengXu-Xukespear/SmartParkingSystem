# SmartParking — 智慧停车管理系统

基于 C++17 和 Crow 框架的高性能 Web 停车管理应用。支持多用户角色、实时车位监控、多级计费规则、月卡套餐、在线预约、多停车场管理、在线客服、黑名单、财务统计等功能。

## 功能特性

### 核心业务
- **车辆出入管理** — 车辆入库/出库登记，黑名单拦截，月卡自动免费通行
- **实时车位监控** — 仪表盘展示总车位/已占用/已预约/剩余车位的实时状态（ECharts 饼图）
- **在线预约** — 用户在线预约车位，支持预付首小时费用，超时自动取消
- **多停车场管理** — 支持多个停车场，预约页面标签切换，含环形车位图（停车场2 20个车位俯视环形布局）
- **多级计费规则** — 标准计费、阶梯计费、会员计费、特殊车辆免费，自定义费率与每日封顶
- **月卡套餐系统** — 月卡/季卡/年卡套餐，用户可自行购买，余额自动扣款，到期自动失效
- **在线客服** — 用户与管理员实时消息沟通，管理员查看分用户会话列表（类微信风格），3秒自动刷新

### 用户与权限
- **多角色权限** — 管理员（admin/root）、普通用户（user）两种角色，细粒度权限控制（23 个权限节点）
- **用户管理** — 注册/登录（SHA-256 + Bearer Token），管理员可增删改用户
- **自助充值** — 用户可在线充值余额（预设金额 ¥50/100/200/500 或自定义）
- **个人中心** — 查看个人信息、修改密码、余额明细、月卡列表
- **我的车辆** — 用户可添加常用车牌号，一键查看停放状态和快捷出入库

### 后台管理
- **用户管理** — 增删改用户，分配角色
- **余额充值** — 管理员可为任意用户充值
- **套餐管理** — 自定义月卡套餐（名称/天数/价格/启用状态）
- **计费规则** — 编辑免费时长、费率、日封顶，阶梯计费支持 JSON 配置
- **月卡管理** — 查看所有月卡，添加/编辑/停用
- **车辆管理** — 查看所有车辆的停放记录，支持搜索筛选和出库操作
- **黑名单管理** — 将违规车辆加入黑名单，禁止入库
- **收入统计** — 今日/本月/总收入，停车费/套餐销售/预约预付分类统计，近30天收入趋势图
- **公告管理** — 创建/编辑/删除公告，支持置顶和有效期

### 技术特性
- **事务保障** — 出入库、预约、套餐购买等关键操作使用数据库事务，避免数据不一致
- **原子操作** — 余额扣减和车位计数使用条件更新，消除并发竞争条件
- **连接池** — 自实现 MySQL 连接池，支持并发访问
- **Token 鉴权** — Bearer Token 认证，24小时过期机制
- **多标签页独立会话** — 使用 sessionStorage 存储 Token，支持同一浏览器多标签页独立登录不同角色
- **车牌识别** — 浏览器摄像头拍照 + RapidOCR (Python) 后端识别，返回车牌号、置信度、颜色，并自动查询登记状态（入场/月卡/黑名单），支持一键快速出入库
- **初始化向导** — 首次运行通过 Web 页面完成数据库配置与表结构创建
- **OpenCV 可选集成** — 编译时检测 OpenCV，启用后支持图像预处理与车牌定位（CMake ENABLE_OPENCV 宏）

## 技术栈

| 层面 | 技术 |
|------|------|
| 语言 | C++17 |
| Web 框架 | [Crow](https://github.com/CrowCpp/Crow) (header-only) |
| 数据库 | MySQL 8.0（C API + 自实现连接池） |
| 异步 IO | Asio 1.28 |
| 前端 | Vanilla HTML5 / CSS3 / JavaScript + ECharts 5 |
| 构建 | CMake 3.16+ + Conan 2 |
| 加密 | 自实现 SHA-256 |

## 项目结构

```
SmartParking/
├── CMakeLists.txt                  # CMake 构建配置（GLOB 收集 src/*.cpp）
├── CMakePresets.json               # CMake 预设（Visual Studio 2022）
├── conanfile.txt                   # Conan 依赖声明
├── config/
│   └── db_config.example.json      # 数据库配置示例
├── sql/
│   └── init.sql                    # 数据库初始化 SQL（手动执行用）
├── src/
│   ├── main.cpp                    # 入口：启动服务器、注册路由、静态文件服务
│   ├── config.h / .cpp             # 运行时配置（单例，JSON 持久化，线程安全）
│   ├── sha256.h                    # SHA-256 自实现（header-only）
│   ├── permissions.h               # 权限节点与角色-权限映射
│   ├── controller/                 # 控制器层（路由注册 + 请求处理）
│   │   ├── base_controller.h/cpp   #   基类：鉴权、权限检查、序列化辅助
│   │   ├── auth_controller.h/cpp   #   登录/注册/登出
│   │   ├── vehicle_controller.h/cpp#   车辆出入库、查询、导出
│   │   ├── parking_controller.h/cpp#   停车场状态、设置、计费规则、月卡
│   │   ├── user_controller.h/cpp   #   用户 CRUD
│   │   ├── reservation_controller.h/cpp# 预约创建/取消
│   │   ├── plate_controller.h/cpp  #   车牌识别（摄像头+OCR+登记查询）
│   │   ├── balance_controller.h/cpp#   余额查询、充值、交易记录
│   │   ├── pass_plan_controller.h/cpp # 套餐管理、购买
│   │   ├── blacklist_controller.h/cpp # 黑名单 CRUD + 拦截记录
│   │   ├── report_controller.h/cpp #   收入统计+导出+预测
│   │   ├── bulletin_controller.h/cpp#  公告 CRUD（创建/编辑/删除/置顶）
│   │   └── message_controller.h/cpp #  在线客服消息
│   ├── service/                    # 服务层（业务逻辑）
│   │   ├── base_service.h/cpp      #   基类：连接池、SQL 转义辅助
│   │   ├── crud_service.h          #   通用 CRUD 模板（header-only 模板）
│   │   ├── auth_service.h/cpp      #   认证、Token 管理
│   │   ├── vehicle_service.h/cpp   #   出入库逻辑、计费计算
│   │   ├── parking_service.h/cpp   #   停车场数据
│   │   ├── billing_service.h/cpp   #   计费规则、月卡 CRUD
│   │   ├── user_service.h/cpp      #   用户 CRUD
│   │   ├── reservation_service.h/cpp#  预约业务
│   │   ├── plate_service.h/cpp     #   车牌验证、识别、登记查询
│   │   ├── balance_service.h/cpp   #   余额原子操作
│   │   ├── pass_plan_service.h/cpp #   套餐购买
│   │   ├── blacklist_service.h/cpp #   黑名单
│   │   ├── report_service.h/cpp    #   财务统计
│   │   └── bulletin_service.h      #   公告服务（header-only）
│   │   └── message_service.h/cpp   #   在线客服消息
│   ├── model/                      # 数据模型（header-only，简单小巧）
│   │   ├── base_model.h            #   抽象基类
│   │   ├── user.h                  #   用户模型
│   │   ├── car_record.h            #   停车记录
│   │   ├── parking_lot.h           #   停车场
│   │   ├── reservation.h           #   预约
│   │   ├── billing.h               #   计费规则、月卡、套餐
│   │   ├── balance_record.h        #   余额变动记录
│   │   ├── blacklist.h             #   黑名单
│   │   ├── bulletin.h              #   公告模型
│   │   ├── interception_log.h      #   黑名单拦截记录
│   │   └── message.h               #   消息模型
│   ├── plate_recognizer.h/cpp     #   车牌识别引擎（图像预处理+定位+OCR）
│   ├── ocr_bridge.py              #   Python OCR 桥接（RapidOCR）
│   └── database/
│       ├── mysql_pool.h/cpp        # 连接池
│       └── db_init.h/cpp           # 自动建库建表
├── frontend/
│   ├── index.html                  # 登录页
│   ├── register.html               # 注册页
│   ├── init.html                   # 初始化向导
│   ├── dashboard.html              # 主面板（车位状态 + 出入库 + 图表 + 我的车辆 + 充值）
│   ├── vehicles.html               # 车辆记录查询
│   ├── reservation.html            # 预约管理
│   ├── admin.html                  # 管理页面（含用户反馈标签页）
│   ├── profile.html                # 个人中心
│   ├── chat.html                   # 联系客服（用户端在线客服）
│   ├── recognize.html              # 车牌识别（摄像头拍照+OCR+快速出入库）
│   ├── css/style.css               # 统一样式
│   └── js/                         # 前端逻辑
│       ├── common.js               # 公共函数（HTTP 请求、权限检查、格式化）
│       ├── auth.js                 # 登录逻辑
│       ├── init.js                 # 初始化向导
│       ├── dashboard.js            # 主面板（状态、出入库、套餐、充值、我的车辆）
│       ├── admin.js                # 管理页面（用户、计费、月卡、黑名单、统计）
│       ├── vehicles.js             # 车辆查询
│       ├── recognize.js            # 车牌识别（摄像头、OCR、快速出入库）
│       └── profile.js              # 个人中心
└── third_party/                    # 第三方库（header-only）
    └── crow.h, crow/               # Crow Web 框架
```

## 团队协作指南

### 模块划分

项目按业务垂直分为 6 个独立模块，每人负责 2-4 个文件：

| 人员 | 模块 | 负责文件 | 前端页面 |
|------|------|----------|----------|
| **A** | 基础架构 | `CMakeLists.txt`, `config.h/cpp`, `main.cpp`, `database/` 全部 | — |
| **B** | 用户与权限 | `auth_service`, `user_service`, `auth_controller`, `user_controller`, `permissions.h` | `index.html`, `register.html`, `profile.html/js` |
| **C** | 停车核心 | `vehicle_service`, `parking_service`, `vehicle_controller`, `parking_controller` | `dashboard.html/js`（出入库+车位部分）, `vehicles.html/js` |
| **D** | 计费与余额 | `balance_service`, `billing_service`, `pass_plan_service`, `balance_controller`, `billing_controller` | `admin.html`（计费/月卡/套餐标签页）, `dashboard.js`（充值弹窗） |
| **E** | 增值功能 | `reservation_service`, `blacklist_service`, `report_service`, `reservation_controller` 等 | `reservation.html`, `admin.html`（黑名单/报表/公告） |
| **F** | 前端统一 | `common.js`, `style.css`, 全局布局、组件规范 | 所有页面的侧边栏、导航、交互规范 |

### 分支策略

```
main                        ← 稳定发布分支，只有 PR 合入
├── feat/vehicle-checkin    ← C 开发：车辆入库功能
├── feat/user-profile       ← B 开发：个人中心
├── fix/balance-overcharge  ← D 修复：余额多扣
└── chore/cmake-update      ← A 更新：构建系统
```

- **功能分支**：从 `main` 创建 `feat/<模块>-<功能名>`
- **Bug 修复**：从 `main` 创建 `fix/<问题描述>`
- **合入流程**：功能开发完成 → 创建 PR → 至少 1 人 Review → 合并到 `main`

### 日常开发流程

```
# 1. 拉取最新代码
git checkout main
git pull

# 2. 创建功能分支
git checkout -b feat/vehicle-export

# 3. 开发，频繁提交
git add src/controller/vehicle_controller.cpp
git commit -m "feat: 添加车辆导出 CSV 功能"

# 4. 推送并创建 PR
git push origin feat/vehicle-export
# 浏览器中创建 Pull Request

# 5. Review 通过后合并到 main
```

### 避免冲突的约定

1. **.h 文件改动需要通知所有人** — 头文件声明变更会影响所有包含它的 .cpp
2. **Model 层大家一起维护** — 改模型字段需要在群里同步
3. **前端各改各的页面** — HTML 和 JS 按页面分，基本不会冲突
4. **公共工具（common.js / base_service）先讨论再改** — 涉及接口约定的改动需要达成一致
5. **每天至少 git pull 一次** — 及时发现冲突，及时解决

### 示例：多人并行开发

假设你们要做三个功能：

- A 改数据库连接池，需要改 `mysql_pool.h/cpp`
- C 改车辆入库逻辑，需要改 `vehicle_service.h/cpp`
- D 新增计费规则类型，需要改 `billing_service.h/cpp`

三个人各自从 main 开分支，改自己的 .cpp 文件。A 改了 mysql_pool.h 的接口声明，C 和 D 合代码时如果发现调用处需要更新，改对应的一两行就行。**不会出现两个人同时改同一个 300 行的大文件的情况。**

## 构建说明

### 从零构建

```bash
conan install . --output-folder=build --build=missing
cmake -B build -S . --preset conan-default
cmake --build build --config Release
```

### 增量构建（开发时）

```bash
cmake --build build --config Release
```

CMake 会自动检测修改过的 .cpp 文件，只重新编译对应的文件，然后链接。改动一个小的 service 文件通常在几秒内完成。

### 只编译某个文件（检查语法）

```bash
cl /c src/service/vehicle_service.cpp /I src /I third_party /I "C:/Program Files/MySQL/MySQL Server 8.0/include"
```

## 环境要求

- **操作系统**: Windows 10/11 或 Linux
- **编译器**: MSVC 2022（Windows）或 GCC 9+ / Clang 10+（Linux）
- **CMake**: >= 3.16
- **Conan**: >= 2.0
- **MySQL**: >= 8.0（需安装 C 开发库 / Connector/C）
- **Git**: 用于克隆仓库

## 快速开始（Windows + MSVC 2022）

### 1. 安装 MySQL 8.0

安装 MySQL Server 8.0，安装时勾选 **"Development Libraries"** 或装完后单独安装 **Connector/C 6.1**：

- 方式一：安装 [MySQL Installer](https://dev.mysql.com/downloads/windows/installer/8.0/)，选择 **"Developer Default"** 或自定义并勾选 **"MySQL Connector/C"**
- 方式二：仅安装 [MySQL Connector/C](https://dev.mysql.com/downloads/connector/c/)（如果已有 MySQL Server）

安装后确认以下目录存在（版本号可能略有不同）：

- 头文件: `C:\Program Files\MySQL\MySQL Server 8.0\include`
- 库文件: `C:\Program Files\MySQL\MySQL Server 8.0\lib`
- DLL 文件: `C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll`

> 如果路径不同，构建前需修改 `CMakeLists.txt` 中的 `MYSQL_INCLUDE_DIR` 和 `MYSQL_LIB_DIR`。

### 2. 安装 Conan 2

```bash
pip install conan
conan --version  # 确认 >= 2.0
```

如果 `pip` 不可用，先安装 Python 3.8+，参见 [python.org](https://www.python.org/downloads/)。

### 3. 克隆项目

```bash
git clone https://github.com/MC-June/SmartParking.git
cd SmartParking
```

### 4. 安装 Conan 依赖

```bash
conan install . --output-folder=build --build=missing
```

这将下载 Asio 1.28 并生成 CMake 预设文件到 `build/` 目录。

### 5. 配置 CMake

```bash
cmake --preset conan-default
```

如果 MySQL 路径与 CMakeLists.txt 默认值不同，可以先设置环境变量：

```bash
set MYSQL_INCLUDE_DIR=C:/Program Files/MySQL/MySQL Server 8.0/include
set MYSQL_LIB_DIR=C:/Program Files/MySQL/MySQL Server 8.0/lib
cmake --preset conan-default
```

### 6. 构建

```bash
cmake --build --preset conan-release
```

构建完成后，可执行文件位于 `build/Release/smart_parking.exe`。构建系统会自动将 `frontend/` 和 `config/` 目录复制到可执行文件所在目录。

### 7. 运行

```bash
cd build/Release
smart_parking.exe
```

启动后终端会显示：

```
==========================================
    Smart Parking 停车管理系统 v1.0
==========================================

[WARN] 数据库连接失败，请检查配置

[OK] 服务启动于 http://localhost:8080
[OK] 前端页面: http://localhost:8080/index.html
[OK] 初始化向导: http://localhost:8080/init.html
```

首次运行由于没有配置文件，会先进入初始化向导。

> **注意**: 程序启动时会从 `config/db_config.json` 加载配置。如果文件不存在或数据库连接失败，会显示警告但仍会启动服务器。未完成初始化前请不要关闭终端窗口。

## 初始化（第一次使用）

1. 浏览器打开 `http://localhost:8080/init.html`
2. 填写 MySQL 连接信息：
   - **主机**: `localhost`
   - **端口**: `3306`
   - **数据库名**: `smart_parking`
   - **用户名**: `root`
   - **密码**: 你的 MySQL root 密码
   - **停车场名称**: 如 `停车场1`
   - **每小时费率**: 如 `5.00`
   - **总车位数**: 如 `100`
   - **服务端口**: `8080`
3. 点击 **"初始化数据库"**
4. 等待页面显示"初始化成功"，然后点击"进入系统"
5. 使用默认管理员账号登录：**用户名** `admin`，**密码** `admin123`

## 快速开始（Linux）

### 1. 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y mysql-server mysql-client libmysqlclient-dev cmake g++ python3-pip
pip install conan

# 启动 MySQL
sudo systemctl start mysql
sudo systemctl enable mysql
```

### 2. 克隆、构建、运行

```bash
git clone https://github.com/MC-June/SmartParking.git
cd SmartParking

conan install . --output-folder=build --build=missing
cmake --preset conan-default
cmake --build --preset conan-release

cd build/Release
./smart_parking
```

## 角色与权限

初始化完成后使用以下账号登录：

| 用户名 | 密码 | 角色 | 说明 |
|--------|------|------|------|
| `admin` | `admin123` | **管理员** | 全部权限，包括系统初始化 |
| — | — | **普通用户** | 查看车位、预约、余额、联系客服、购买套餐 |

管理员可在 **管理页面 → 用户管理** 中添加不同角色的用户。

### 权限节点

共 23 个权限节点。管理员（admin/root）拥有全部权限，普通用户（user）拥有以下 7 个权限：`parking.view`、`billing.view`、`reservation.create`、`reservation.view`、`reservation.cancel`、`balance.view`、`message.send`。

| 权限 | 说明 | admin/root | user |
|------|------|:----------:|:----:|
| `system.init` | 系统初始化 | ✓ | |
| `user.view` | 查看用户列表 | ✓ | |
| `user.manage` | 增删改用户 | ✓ | |
| `parking.view` | 查看停车场状态 | ✓ | ✓ |
| `parking.settings` | 修改停车场设置 | ✓ | |
| `billing.view` | 查看计费规则 | ✓ | ✓ |
| `billing.manage` | 编辑计费规则 | ✓ | |
| `vehicle.checkin` | 车辆入库 | ✓ | |
| `vehicle.checkout` | 车辆出库 | ✓ | |
| `vehicle.query` | 查询停车记录 | ✓ | |
| `vehicle.delete` | 删除记录 | ✓ | |
| `vehicle.export` | 导出数据 | ✓ | |
| `reservation.create` | 创建预约 | ✓ | ✓ |
| `reservation.view` | 查看预约 | ✓ | ✓ |
| `reservation.cancel` | 取消预约 | ✓ | ✓ |
| `plate.recognize` | 车牌识别 | ✓ | |
| `balance.view` | 查看余额 | ✓ | ✓ |
| `balance.manage` | 余额充值管理 | ✓ | |
| `passplan.manage` | 套餐管理 | ✓ | |
| `vehicle.blacklist` | 黑名单管理 | ✓ | |
| `report.view` | 查看统计报表 | ✓ | |
| `notice.manage` | 编辑公告 | ✓ | |
| `message.send` | 发送/查看消息 | ✓ | ✓ |
| `message.manage` | 管理用户反馈 | ✓ | |

## 用户端功能

### 主面板（Dashboard）
- **车位状态** — 总车位、已占用、已预约、剩余车位实时统计，ECharts 饼图可视化
- **车辆入库/出库** — 操作员/管理员可直接输入车牌号操作，支持选择计费方式
- **套餐购买** — 浏览月卡/季卡/年卡套餐，余额一键购买
- **自助充值** — 选择预设金额（¥50/100/200/500）或自定义金额充值
- **我的车辆** — 添加常用车牌号，查看停放状态，一键入库/出库
- **余额与交易记录** — 查看余额和最近交易明细
- **最近记录** — 查看最近车辆出入记录
- **公告栏** — 查看系统公告
- **联系客服** — 在线与管理员沟通

### 车辆信息查询
- 按车牌号、日期范围搜索停车记录
- 查看每次出入库时间、费用、计费方式

### 预约管理
- 在线预约车位，预付首小时费用
- **多停车场标签切换** — 预约页面顶部标签切换不同停车场（停车场1 标准网格布局 / 停车场2 环形布局）
- 查看预约列表及剩余有效时间
- 取消预约（预付不退）

### 联系客服
- 与管理员实时在线沟通（类微信聊天界面）
- 自动 3 秒轮询新消息
- 消息按日期分组，区分发送/接收方向
- 支持 Enter 键快速发送

### 个人中心
- 查看和修改个人信息（手机号、真实姓名）
- 修改密码
- 余额明细
- 月卡列表

### 车牌识别
- **摄像头拍照识别** — 开启设备摄像头，实时预览，一键拍照上传
- **OCR 车牌识别** — 后端 RapidOCR 识别车牌号、置信度、车牌颜色
- **登记状态查询** — 识别后自动查询车辆是否已登记、是否在场内、是否有月卡、是否在黑名单
- **快速出入库** — 识别成功后一键完成入库或出库操作
- **手动查询** — 手动输入车牌号查询登记信息
- **识别历史** — 本地保存最近 20 条识别记录

### 管理页面
- **用户管理** — 添加/编辑/删除用户，分配角色
- **余额充值** — 选择用户，输入金额，备注说明
- **套餐管理** — 添加/编辑/删除月卡套餐
- **计费规则** — 编辑免费时长、费率、日封顶、启用/禁用
- **车辆管理** — 查看所有车辆状态，出库操作，筛选
- **月卡管理** — 查看/添加/编辑/停用月卡
- **黑名单** — 添加/移除黑名单车辆
- **收入统计** — 今日/本月/总收入趋势图
- **公告管理** — 创建/编辑/删除公告，支持置顶和有效期设置
- **用户反馈** — 查看所有用户会话列表（按最新消息排序），选择会话查看用户基本信息（姓名/手机/角色/余额/注册时间）和完整聊天记录，实时回复用户（3秒自动刷新，未读消息计数）

## 构建选项

### Debug 构建

```bash
cmake --build --preset conan-debug
```

### 自定义 MySQL 路径

```bash
cmake --preset conan-default \
  -DMYSQL_INCLUDE_DIR="D:/MySQL/include" \
  -DMYSQL_LIB_DIR="D:/MySQL/lib"
```

## 常见问题

### build/CMakePresets.json 不存在

Conan 安装步骤被跳过或失败。运行：

```bash
conan install . --output-folder=build --build=missing
```

这会生成 `build/CMakePresets.json`。

### 找不到 mysqlclient.lib / libmysql.dll

确认 MySQL Connector/C 已安装，且路径匹配 `CMakeLists.txt` 中的默认值：

```bash
cmake --preset conan-default \
  -DMYSQL_INCLUDE_DIR="C:/Program Files/MySQL/MySQL Connector C 6.1/include" \
  -DMYSQL_LIB_DIR="C:/Program Files/MySQL/MySQL Connector C 6.1/lib/vs14"
```

### 运行时提示 "数据库连接失败"

- 检查 MySQL 服务是否在运行
- 检查用户名密码是否正确
- 检查 `config/db_config.json` 是否存在且配置正确
- 如果尚未初始化，打开 `http://localhost:8080/init.html` 完成初始化

### 前端页面无法加载（404）

确保工作目录正确，程序需要能访问到 `frontend/` 和 `config/` 目录。从 `build/Release/` 目录运行可执行文件。

### 端口被占用

修改 `config/db_config.json` 中的 `server_port` 字段，或重新通过初始化向导设置。修改后重启程序。

## 配置文件

配置文件 `config/db_config.json` 会在初始化时自动生成，也可手动创建：

```json
{
    "host": "localhost",
    "port": 3306,
    "database": "smart_parking",
    "user": "root",
    "password": "your_password",
    "parking_name": "停车场1",
    "fee": 5.00,
    "capacity": 100,
    "server_port": 8080,
    "notice_expire_minutes": 30,
    "notice": "欢迎使用停车场管理系统！\n请遵守停车场管理规定，文明停车。"
}
```

## API 参考

所有 API 返回 JSON 格式，认证接口使用 Bearer Token（`Authorization: Bearer <token>`）。

### 认证 `/api/auth`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| POST | `/api/auth/login` | 公开 | 用户登录，返回 Token |
| POST | `/api/auth/register` | 公开 | 用户注册 |
| POST | `/api/auth/logout` | 认证 | 登出 |
| GET | `/api/auth/check` | 认证 | 校验 Token 有效性 |

### 车辆 `/api/vehicle`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| POST | `/api/vehicle/checkin` | `vehicle.checkin` | 车辆入库 |
| POST | `/api/vehicle/checkout` | `vehicle.checkout` | 车辆出库（计费+扣款） |
| GET | `/api/vehicle/query` | `vehicle.query` | 查询停车记录 |
| GET | `/api/vehicle/parked` | `vehicle.query` | 当前在场车辆列表 |
| GET | `/api/vehicle/status` | 认证 | 所有车辆停放状态 |
| DELETE | `/api/vehicle/<id>` | `vehicle.delete` | 删除记录 |
| GET | `/api/vehicle/export` | `vehicle.export` | 导出 CSV |

### 停车场 `/api/parking`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/parking/list` | 认证 | 停车场列表 |
| GET | `/api/parking/status` | `parking.view` | 停车场实时状态（含 notice_expire_minutes） |
| GET | `/api/parking/stats` | `parking.view` | 饼图统计数据 |
| PUT | `/api/parking/settings` | `parking.settings` | 更新费率/容量 |
| GET | `/api/parking/billing-rules` | `billing.view` | 获取计费规则列表 |
| PUT | `/api/parking/billing-rules/<id>` | `billing.manage` | 更新计费规则 |
| GET/POST/PUT/DELETE | `/api/parking/monthly-passes` | 视操作 | 月卡 CRUD |

### 用户 `/api/user`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/user/list` | `user.view` | 用户列表 |
| POST | `/api/user/add` | `user.manage` | 添加用户 |
| PUT | `/api/user/update` | `user.manage` | 更新用户 |
| DELETE | `/api/user/<id>` | `user.manage` | 删除用户 |

### 预约 `/api/reservation`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| POST | `/api/reservation/create` | `reservation.create` | 创建预约 |
| GET | `/api/reservation/list` | `reservation.view` | 预约列表 |
| GET | `/api/reservation/history` | `reservation.view` | 预约历史 |
| GET | `/api/reservation/spots` | `parking.view` | 车位状态（?P_name=停车场名） |
| DELETE | `/api/reservation/<id>` | `reservation.cancel` | 取消预约 |

### 余额 `/api/balance`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/balance` | `balance.view` | 我的余额和交易记录 |
| GET | `/api/balance/my` | 认证 | 当前用户余额信息 |
| GET | `/api/balance/<id>` | `balance.manage` | 指定用户余额 |
| POST | `/api/balance/recharge` | `balance.manage` | 管理员充值 |
| POST | `/api/balance/deposit` | 认证 | 自助充值（1~10000元） |
| POST | `/api/balance/calculate-fee` | `balance.manage` | 预估停车费用 |
| POST | `/api/balance/parking-deduct` | 认证 | 停车费扣款 |
| GET | `/api/balance/transactions` | `balance.manage` | 全部交易记录 |

### 套餐 `/api/pass-plans`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/pass-plans` | 认证 | 可用套餐列表 |
| POST | `/api/pass-plans` | `passplan.manage` | 添加套餐 |
| PUT | `/api/pass-plans/<id>` | `passplan.manage` | 更新套餐 |
| DELETE | `/api/pass-plans/<id>` | `passplan.manage` | 删除套餐 |
| POST | `/api/pass-plans/<id>/purchase` | 认证 | 购买套餐（余额扣款） |

### 消息 `/api/message`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| POST | `/api/message/send` | `message.send` | 发送消息（receiver_id=0 为发给管理员） |
| GET | `/api/message/conversations` | `message.manage` | 管理员获取会话列表（含用户信息+未读数） |
| GET | `/api/message/history` | `message.send` | 获取与指定用户的聊天记录（?user_id=X） |
| POST | `/api/message/mark-read` | `message.send` | 标记消息为已读 |
| GET | `/api/message/unread-count` | `message.send` | 获取当前用户未读消息数 |

### 车牌识别 `/api/plate`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| POST | `/api/plate/recognize` | `plate.recognize` | 车牌识别（文本） |
| POST | `/api/plate/recognize-image` | `plate.recognize` | 摄像头拍照识别（base64 图片）+ 登记状态查询 |
| POST | `/api/plate/check-registered` | 认证 | 手动查询车牌登记状态 |
| POST | `/api/plate/validate` | 认证 | 车牌格式校验 |

### 黑名单 `/api/blacklist`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/blacklist` | `vehicle.blacklist` | 黑名单列表 |
| POST | `/api/blacklist` | `vehicle.blacklist` | 添加黑名单 |
| DELETE | `/api/blacklist/<id>` | `vehicle.blacklist` | 移除黑名单 |
| GET | `/api/blacklist/interceptions` | `vehicle.blacklist` | 拦截记录列表 |
| GET | `/api/blacklist/interceptions/count` | `vehicle.blacklist` | 拦截次数统计 |

### 报表 `/api/report`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/report/summary` | `report.view` | 收入汇总 |
| GET | `/api/report/daily` | `report.view` | 近30天收入趋势 |
| GET | `/api/report/export` | `report.view` | 导出收入报表 CSV |
| GET | `/api/report/prediction` | `report.view` | 收入预测数据 |

### 公告 `/api/bulletin`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/bulletin` | 认证 | 获取当前有效公告 |
| GET | `/api/bulletin/all` | `notice.manage` | 所有公告列表 |
| POST | `/api/bulletin` | `notice.manage` | 创建公告 |
| PUT | `/api/bulletin/<id>` | `notice.manage` | 更新公告 |
| DELETE | `/api/bulletin/<id>` | `notice.manage` | 删除公告 |

### 系统 `/api/init`

| 方法 | 路径 | 权限 | 说明 |
|------|------|------|------|
| GET | `/api/init/status` | 公开 | 系统初始化状态 |
| POST | `/api/init/database` | 公开/root | 初始化数据库 |

## 数据库表

| 表名 | 说明 |
|------|------|
| `USER` | 用户表（id/username/password/telephone/truename/role/balance/created_at） |
| `PARKING_LOT` | 停车场（P_id/P_name/P_total_count/P_current_count/P_reserve_count/P_fee） |
| `CAR_RECORD` | 停车记录（含 check_in/check_out/fee/billing_type/reservation_id） |
| `RESERVATION` | 预约记录（触发器自动更新预约计数） |
| `BILLING_RULE` | 计费规则（free_minutes/hourly_rate/max_daily_fee/tier_config） |
| `MONTHLY_PASS` | 月卡（license_plate/pass_type/start_date/end_date/fee/is_active） |
| `PASS_PLAN` | 套餐定义（plan_name/duration_days/price/is_active） |
| `BALANCE_RECORD` | 余额变动记录（user_id/amount/type/description/balance_after） |
| `VEHICLE_BLACKLIST` | 黑名单（license_plate/reason） |
| `INTERCEPTION_LOG` | 黑名单拦截记录（license_plate/reason/intercepted_at） |
| `BULLETIN` | 公告（content/is_pinned/valid_from/valid_until/created_at） |
| `MESSAGE` | 在线客服消息（sender_id/receiver_id/content/created_at/is_read） |

## 待扩展功能

- [x] 车牌识别（浏览器摄像头 + RapidOCR Python 桥接 + 登记状态查询 + 快速出入库）
- [x] 多停车场支持（预约页面标签切换，含环形车位图）
- [ ] 支付接口对接（微信/支付宝）
- [ ] Docker 容器化部署
- [ ] 单元测试与集成测试
- [ ] WebSocket 实时推送（替代前端轮询）
- [ ] 停车记录图片留存

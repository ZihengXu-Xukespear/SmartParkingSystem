# SmartParking 团队协作指南

## 目录

1. [项目架构概览](#1-项目架构概览)
2. [模块划分与人员分工](#2-模块划分与人员分工)
3. [Git 分支策略](#3-git-分支策略)
4. [日常开发流程](#4-日常开发流程)
5. [代码审查规范](#5-代码审查规范)
6. [接口约定](#6-接口约定)
7. [避免冲突的实践](#7-避免冲突的实践)
8. [构建与测试](#8-构建与测试)
9. [常见场景处理](#9-常见场景处理)

---

## 1. 项目架构概览

```
┌─────────────────────────────────────────────────────────┐
│                   前端页面（共同维护）                  │
│  login │ dashboard │ admin │ profile │ reservation ...  │
└──────────────┬──────────────────────┬───────────────────┘
               │ HTTP JSON API        │
               ▼                      ▼
┌──────────────────────┐  ┌──────────────────────────────┐
│    Controller 层      │  │    前端逻辑 (JS)               │
│ 路由注册 + 请求/响应   │  │  HTTP 请求封装 + DOM 操作      │
│ 权限检查 + 参数解析    │  │  权限检查 + 状态管理            │
└──────────┬───────────┘  └──────────────────────────────┘
           │ 调用 Service
           ▼
┌─────────────────────────────────────────────────────────┐
│                     Service 层                           │
│  业务逻辑、数据校验、事务管理、跨服务调用                    │
├──────────┬──────────┬──────────┬──────────┬─────────────┤
│  Auth    │ Vehicle  │ Parking  │ Balance  │ Reservation │
│  User    │ Billing  │ PassPlan │ Blacklist│ Report      │
│  Plate   │          │          │          │             │
└──────────┴──────────┴──────────┴──────────┴─────────────┘
           │ 调用 MySQL Pool
           ▼
┌─────────────────────────────────────────────────────────┐
│                   Infrastructure                         │
│         MySQL Pool │ Config │ DB Init │ CMake            │
└─────────────────────────────────────────────────────────┘
```

### 文件结构（每个模块 = .h 声明 + .cpp 实现）

```
src/service/vehicle_service.h    →  公开接口声明
src/service/vehicle_service.cpp  →  具体实现（唯一会被编译的文件）
```

- **.h 文件**: 类声明、方法签名（改动会影响所有人）
- **.cpp 文件**: 方法实现（各自修改，互不冲突）

### 文件依赖关系

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Controller  │────>│   Service   │────>│   Model     │
│ (薄层)       │     │  (业务逻辑)  │     │ (数据结构)   │
└─────────────┘     └──────┬──────┘     └─────────────┘
                           │
                    ┌──────▼──────┐
                    │  BaseService │
                    │  MySQL Pool  │
                    │  Config      │
                    └─────────────┘
```

---

## 2. 模块划分与人员分工

###  A — 基础架构（Infrastructure）

**职责**: 搭建和维护项目骨架，确保其他人能在这之上开发

| 文件 | 类型 | 说明 |
|------|------|------|
| `CMakeLists.txt` | 构建 | 编译选项、链接库、文件收集 |
| `src/main.cpp` | 入口 | 服务器启动、路由注册调度、静态文件服务 |
| `src/config.h/cpp` | 配置 | AppConfig 单例，JSON 持久化 |
| `src/database/mysql_pool.h/cpp` | 数据库 | 连接池实现 |
| `src/database/db_init.h/cpp` | 数据库 | 建库建表、种子数据 |
| `src/service/base_service.h/cpp` | 公共 | SQL 转义、查询辅助 |
| `src/service/crud_service.h` | 公共 | CRUD 模板（header-only） |
| `src/controller/base_controller.h/cpp` | 公共 | 鉴权、响应辅助、序列化 |
| `src/model/*.h` | 模型 | 所有数据模型（多人共同维护） |
| `third_party/` | 依赖 | 第三方库管理 |

**需要了解**: CMake、MySQL C API、Crow 框架、Windows/Linux 编译差异

**开发指南**:
```bash
# 改完配置可以只编译这个文件
cmake --build build --config Release
```

---

###  B — 用户与权限（Auth & User）

**职责**: 用户认证、授权、个人信息管理

| 文件 | 说明 |
|------|------|
| `src/service/auth_service.h/cpp` | 登录、注册、Token 管理、权限检查 |
| `src/service/user_service.h/cpp` | 用户 CRUD |
| `src/controller/auth_controller.h/cpp` | 登录/注册/登出/Token 校验 API |
| `src/controller/user_controller.h/cpp` | 用户列表/添加/更新/删除 API |
| `src/permissions.h` | 权限节点定义、角色权限映射 |
| `src/sha256.h` | 密码哈希 |

**前端页面**:
- `frontend/index.html` — 登录页
- `frontend/register.html` — 注册页
- `frontend/profile.html` + `js/profile.js` — 个人中心

**提供的接口给其他人用**:
- `AuthService::instance().hasPermission(token, permission)` → C/D/E 用来权限校验
- `AuthService::instance().getUserId(token)` → C 的 checkOut 需要知道谁在操作
- `UserService::instance().listUsers()` → D 的充值需要选择用户

**典型改动场景**:
```bash
# 1. 改登录逻辑 → 改 auth_service.cpp
# 2. 新增角色 → 改 permissions.h + auth_service.cpp
# 3. 改个人信息页 → 改 profile.html + profile.js
```

---

###  C — 停车核心（Parking Core）

**职责**: 车辆出入库、车位管理、停车记录查询

| 文件 | 说明 |
|------|------|
| `src/service/vehicle_service.h/cpp` | 入库/出库、计费计算、记录查询 |
| `src/service/parking_service.h/cpp` | 停车场状态、设置更新 |
| `src/controller/vehicle_controller.h/cpp` | 出入库/查询/导出 API |
| `src/controller/parking_controller.h/cpp` | 状态/设置/计费规则/月卡 API |

**前端页面**:
- `frontend/dashboard.html` + `js/dashboard.js` — 出入库操作部分
- `frontend/vehicles.html` + `js/vehicles.js` — 停车记录查询

**依赖其他模块**:
- 入库前调 B 的 `BlacklistService::isBlacklisted(plate)` 检查黑名单
- 出库时调 D 的 `BalanceService::deduct(userId, fee, ...)` 扣停车费
- 出库时检查月卡状态（调 D 的计费规则）

**提供的接口给其他人用**:
- `ParkingService::instance().getStatus()` → E 的预约需要知道车位状态
- `VehicleService::instance().queryRecords()` → F 的前端展示需要

**典型改动场景**:
```bash
# 1. 改计费算法 → 改 vehicle_service.cpp 的 calculateFee()
# 2. 改入库校验逻辑 → 改 vehicle_service.cpp 的 checkIn()
# 3. 改车位状态展示 → 改 dashboard.js
```

---

###  D — 计费与余额（Billing & Balance）

**职责**: 余额管理、计费规则、套餐月卡

| 文件 | 说明 |
|------|------|
| `src/service/balance_service.h/cpp` | 余额原子操作（扣款/充值/退款） |
| `src/service/billing_service.h/cpp` | 计费规则 CRUD、月卡 CRUD |
| `src/service/pass_plan_service.h/cpp` | 套餐定义、购买逻辑 |
| `src/controller/balance_controller.h/cpp` | 余额查询/充值/交易记录 API |
| `src/controller/pass_plan_controller.h/cpp` | 套餐 CRUD、购买 API |

**前端页面**:
- `frontend/admin.html` + `js/admin.js` — 计费规则/月卡/套餐管理标签页
- `frontend/dashboard.html` + `js/dashboard.js` — 充值弹窗、余额展示（和 C 共享 dashboard）

**依赖其他模块**:
- 充值/扣款需要 B 的 `UserService` 验证用户存在
- 套餐购买需要搭 C 的 `ParkingService` 获取车位信息

**提供的接口给其他人用**:
- `BalanceService::instance().deduct(userId, amount, ...)` → C 的出库、E 的预约
- `BalanceService::instance().refund(userId, amount, ...)` → C/E 的失败退款
- `BillingService::instance().getMonthlyPasses()` → 前端展示

**典型改动场景**:
```bash
# 1. 改计费规则 → 改 billing_service.cpp
# 2. 改充值逻辑 → 改 balance_service.cpp + balance_controller.cpp
# 3. 改套餐定价 → 无需改代码，在管理页面编辑即可
```

---

###  E — 增值功能（Value-Added）

**职责**: 预约、黑名单、报表、公告

| 文件 | 说明 |
|------|------|
| `src/service/reservation_service.h/cpp` | 预约创建/取消 |
| `src/service/blacklist_service.h/cpp` | 黑名单 CRUD |
| `src/service/report_service.h/cpp` | 收入统计 |
| `src/controller/reservation_controller.h/cpp` | 预约 API |
| `src/controller/blacklist_controller.h/cpp` | 黑名单 API |
| `src/controller/report_controller.h/cpp` | 报表 API |
| `src/controller/bulletin_controller.h/cpp` | 公告 API |

**前端页面**:
- `frontend/reservation.html` — 预约管理
- `frontend/admin.html` + `js/admin.js` — 黑名单/报表/公告标签页

**依赖其他模块**:
- 预约需要 C 的 `ParkingService::getStatus()` 检查车位
- 预约扣费需要 D 的 `BalanceService::deduct()`
- 黑名单检查被 C 的 `VehicleService::checkIn()` 调用

**提供的接口给其他人用**:
- `BlacklistService::instance().isBlacklisted(plate)` → C 的入库检查
- `ReportService::instance().getSummary()` → 前端展示

**典型改动场景**:
```bash
# 1. 改预约过期时间 → 改 reservation_service.cpp
# 2. 新增报表指标 → 改 report_service.cpp + report_controller.cpp
# 3. 改黑名单校验逻辑 → 改 blacklist_service.cpp
```

---

###  F — 前端基础设施（Frontend Foundation）

**职责**: 前端公共组件、样式规范、协调前端风格

| 文件 | 说明 |
|------|------|
| `frontend/css/style.css` | 全局样式（颜色、布局、组件） |
| `frontend/js/common.js` | 公共函数（HTTP 封装、权限检查、格式化） |
| 所有 HTML 页面的侧边栏和导航 | 统一布局 |

**不负责具体业务功能**，而是：

1. **制定规范** — CSS 命名规则、JS 编码风格、API 调用约定
2. **抽象公共组件** — 表格、弹窗、表单、提示框的统一样式
3. **审查所有人的前端 PR** — 确保风格一致
4. **维护构建工具** — 如果有前端构建流程的话

**典型工作**:
```javascript
// common.js — 给所有人用的公共函数
// B/C/D/E 都要调用这些，不能随便改签名
async function get(url) { ... }
async function post(url, body) { ... }
function showError(containerId, message) { ... }
function hasPerm(permission) { ... }
```

**提供的规范给其他人用**:
- CSS class 命名约定（`.btn-primary`, `.card`, `.form-group`）
- JS 函数的调用方式和返回值格式
- 页面布局模板（侧边栏 + 内容区的 HTML 结构）

---

## 3. Git 分支策略

### 分支结构

```
main                  ← 稳定分支，只有通过 PR 合入
│
├── feat/auth-xxx     ← B 的功能分支
├── feat/vehicle-xxx  ← C 的功能分支
├── feat/billing-xxx  ← D 的功能分支
├── feat/reserve-xxx  ← E 的功能分支
│
├── fix/xxx           ← 任何人修 bug 用
└── docs/xxx          ← 文档更新
```

### 命名规范

```
feat/<模块>-<功能描述>
  └── feat/vehicle-csv-export
  └── feat/auth-wechat-login

fix/<简要问题>
  └── fix/balance-overflow
  └── fix/plate-validation-null

docs/<内容>
  └── docs/api-changelog
  └── docs/deployment-guide
```

### 分支生命周期

```
创建分支 → 开发 → 提交 → 推送 → 创建 PR → Review → 合并到 main → 删除分支
  │                                              │
  └── 每天 git pull main 保持同步              └── 使用 --no-ff 保留历史
```

---

## 4. 日常开发流程

### 完整的一天

```bash
# 早晨：拉取最新代码
git checkout main
git pull

# 创建功能分支
git checkout -b feat/vehicle-duration-display

# 开发...
# 改自己的文件：src/service/vehicle_service.cpp
# 改对应的前端：frontend/js/dashboard.js

# 阶段性提交
git add src/service/vehicle_service.cpp
git commit -m "feat: 停车时长显示（当前停放时间）"

git add frontend/js/dashboard.js
git commit -m "feat: 前端展示停车时长"
# 注意：每个 commit 聚焦一个改动点，方便以后回滚

# 有人改了 main？同步一下
git fetch origin
git rebase origin/main
# 如果有冲突，解决后 git rebase --continue

# 推送并创建 PR
git push origin feat/vehicle-duration-display
# 浏览器打开自动提示的链接，创建 Pull Request
```

### Commit Message 规范

```
<type>: <简短描述（50字以内）>

<可选：详细说明（72字换行）>

<可选：关联 Issue>
```

**type 类型**:

| type | 说明 | 例子 |
|------|------|------|
| `feat` | 新功能 | `feat: 添加微信支付充值` |
| `fix` | Bug 修复 | `fix: 预约过期时间计算错误` |
| `refactor` | 重构 | `refactor: 拆分 calculateFee 为多个小函数` |
| `perf` | 性能优化 | `perf: ReportService 合并 SQL 查询` |
| `style` | 样式 | `style: 按钮 hover 效果统一` |
| `docs` | 文档 | `docs: 更新 API 文档` |
| `chore` | 杂项 | `chore: 更新 .gitignore` |
| `test` | 测试 | `test: 添加余额扣减单元测试` |

### 好的 vs 坏的 Commit Message

```
✅ feat: 添加 CSV 导出功能
   - 支持按日期范围筛选导出
   - CSV 字段增加了双引号转义

❌ 改了点东西
❌ update
❌ 修复bug
```

---

## 5. 代码审查规范

### Review Checklist

每个 PR 至少 1 人 Review 后才能合并。Reviewer 检查以下内容：

**安全性**:
- [ ] SQL 查询使用了 `escape()` 或 `quote()`（没有字符串拼接注入风险）
- [ ] 权限检查是否正确（该有的权限有没有漏）
- [ ] 用户输入是否做了基本校验

**正确性**:
- [ ] 边界情况处理了（空值、负数、超长字符串）
- [ ] 事务使用正确（关键操作有没有包裹 Transaction）
- [ ] 资源释放了（`mysql_free_result` 有没有漏）

**可维护性**:
- [ ] 函数长度是否合理（一般不超过 50 行）
- [ ] 变量命名是否清晰
- [ ] 注释是否有必要（不要写 "定义变量 i"，写 "为什么这么处理"）

**一致性**:
- [ ] 代码风格和项目现有的一致
- [ ] API 返回值格式和已有接口一致

### Review 流程

```
提交者: 创建 PR，写好描述，@ 指定 Reviewer
                   ↓
Reviewer: 阅读 diff，逐行 Check
                   ↓
Reviewer: 留下评论（必须/建议/疑问）
                   ↓
提交者: 根据评论修改代码
                   ↓
提交者: @ Reviewer 重新审查
                   ↓
Reviewer: Approve ✅
                   ↓
提交者: 点击 Merge
```

---

## 6. 接口约定

### Service 层调用规则

跨模块调用时，始终通过 `XxxService::instance()` 单例访问：

```cpp
// ✅ 正确：通过单例调用其他 Service
BalanceService::instance().deduct(userId, fee, "checkout", desc, error);

// ❌ 错误：直接创建 Service 实例
BalanceService bs;
bs.deduct(...);
```

### API 返回格式

所有 API 返回统一的 JSON 格式：

```json
// 成功
{ "message": "操作成功" }

// 带数据
{ "records": [...], "total": 42 }

// 错误
{ "error": "用户名或密码错误" }
```

HTTP 状态码含义：

| 状态码 | 含义 | 说明 |
|--------|------|------|
| 200 | 成功 | 请求正常处理 |
| 400 | 参数错误 | 缺少参数或格式错误 |
| 401 | 未认证 | Token 缺失或无效 |
| 403 | 权限不足 | 无权执行此操作 |
| 404 | 未找到 | 资源不存在 |

### 错误处理约定

```cpp
// ✅ Service 方法通过 `error` 传出参数返回错误信息
bool checkIn(const std::string& plate, const std::string& billing_type, 
             std::string& error);  // ← 错误信息由此传出

// ✅ Controller 使用标准错误响应
return BaseController::errorResponse(400, error);

// ❌ 不要直接返回原始错误
return crow::response(500, mysql_error(conn));
```

### 事务使用规则

涉及多个写操作的 Service 方法必须使用事务：

```cpp
auto conn = getConnection();
Transaction tx(conn->get());

// ... 多个数据库操作 ...

if (!tx.commit()) { 
    // 失败处理 -- 自动回滚
    return false; 
}
return true;
```

**关键的事务边界**（这些方法已包裹事务，改的时候注意不要拆散）：
- `VehicleService::checkIn` — 插入记录 + 更新计数
- `VehicleService::checkOut` — 更新记录 + 扣款 + 更新计数
- `ReservationService::create` — 扣款 + 插入预约
- `PassPlanService::purchase` — 扣款 + 创建/续费月卡

---

## 7. 避免冲突的实践

### 黄金法则

> **改 .cpp 不用问人，改 .h 先打招呼**

### 具体规则

| 场景 | 做法 |
|------|------|
| 只改自己的 .cpp | 直接改，随意提交 |
| 改自己模块的 .h | 群通知"我改了 XXX 的接口"，其他人检查是否需要更新调用 |
| 改公共 .h（base_service、config 等） | 先在群里讨论方案，达成一致再改 |
| 改 Model（user.h、car_record.h 等） | 群里通知，因为影响所有 Service 的 SQL 查询字段 |
| 改 common.js | 必须和 F 沟通，因为 B/C/D/E 都在用 |
| 改 style.css | 必须和 F 沟通，影响所有页面外观 |

### 处理冲突

```bash
# pull 时发现冲突
git pull
# 输出: CONFLICT in src/service/vehicle_service.cpp

# 查看冲突
git diff

# 手动解决冲突后
git add src/service/vehicle_service.cpp
git commit -m "fix: 合并冲突 - 保留两处修改"

# 或者用工具
git mergetool  # 会打开图形化合并工具
```

### 日常习惯

```
✅ 每天至少 pull 一次
✅ 做新功能前先 pull
✅ 频繁提交小改动（而不是攒一周一次提）
✅ 改接口前先问一声
✅ 用 feature 分支开发

❌ 一个分支做多个不相关的功能
❌ 直接往 main 提交（除非是紧急修复）
❌ 改别人的 .cpp 文件（除非提前沟通了）
❌ 一个 commit 改 20 个文件
```

---

## 8. 构建与测试

### 本地构建

```bash
# 完整构建
conan install . --output-folder=build --build=missing
cmake -B build -S . --preset conan-default
cmake --build build --config Release

# 增量构建（日常开发用这个）
cmake --build build --config Release
```

### 增量编译速度

拆分后增量编译很快，因为每个 .cpp 独立编译：

```
改 vehicle_service.cpp → 只编译这个文件（不到 1 秒）→ 链接（几秒）
改 auth_service.h     → 所有包含它的 .cpp 都要重新编译（十几秒）
改 crud_service.h     → 几乎不变（模板在 header 里，但很少改）
```

### 运行测试

```bash
# 启动服务
cd build/Release
./smart_parking.exe

# 终端会显示启动信息，然后打开浏览器访问
# http://localhost:8080
```

---

## 9. 常见场景处理

### 场景 1：C 要新增一个 API

1. 在自己的 `vehicle_controller.cpp` 添加路由
2. 在 `vehicle_service.cpp` 实现业务逻辑
3. 其他人不需要做任何事 ❌

```cpp
// C 自己改 vehicle_controller.cpp
CROW_ROUTE(app, "/api/vehicle/duration").methods("GET"_method)(...)
```

### 场景 2：D 改了计费规则的数据结构

1. 改 billing.h（新增字段）
2. 群里通知"billing.h 加了 tier_config 字段"
3. C 的 `vehicle_service.cpp` 如果读计费规则，需要更新 SQL
4. E 的报表如果需要新字段，也更新

### 场景 3：新人加入

1. 确定他负责哪个模块（参考上面的分工表）
2. 告诉他这个模块的 .h 和 .cpp 在哪里
3. 让他先建一个 feature 分支改点小功能
4. 第一个 PR 由老手 Review，帮助熟悉规范

### 场景 4：紧急修复线上 Bug

```bash
# 直接从 main 开 fix 分支
git checkout main
git checkout -b fix/checkin-null-plate

# 修完直接提 PR
git add src/service/vehicle_service.cpp
git commit -m "fix: 车牌号为空时入库崩溃"
git push origin fix/checkin-null-plate
# PR 可以少 Review 时间（因为是紧急修复）
```

### 场景 5：两个人都改了同一个文件

虽然现在每人负责不同的文件，但万一发生：

```bash
# 方法 1：用 git 命令行解决
git pull origin main
# 看到冲突提示后，手动编辑冲突文件
# 查找 <<<<<<<, =======, >>>>>>> 标记
# 修改后
git add <冲突文件>
git commit -m "fix: 解决合并冲突"

# 方法 2：如果冲突太大，放弃自己修改
git checkout --ours <冲突文件>
# 然后再重新改
```

### 场景 6：需要调试其他人的模块

如果 C 发现出库时余额扣减有问题：

```cpp
// C 在自己的代码里加调试输出（不要改 D 的代码）
bool VehicleService::checkOut(...) {
    double bal = BalanceService::instance().getBalance(userId);
    std::cout << "[DEBUG] 用户 " << userId << " 当前余额: " << bal << "\n";  
    // 这个输出只在 C 的分支上，不会影响 D
    ...
}
```

排查出问题后，在 PR 的 review 里告诉 D 具体问题。

---

## 附录：关键接口速查

### Service 单例获取

```cpp
AuthService::instance()          // 认证
UserService::instance()          // 用户
VehicleService::instance()       // 车辆
ParkingService::instance()       // 停车场
BalanceService::instance()       // 余额
BillingService::instance()       // 计费
PassPlanService::instance()      // 套餐
ReservationService::instance()   // 预约
BlacklistService::instance()     // 黑名单
ReportService::instance()        // 报表
PlateService::instance()         // 车牌
```

### 跨模块调用关系

```
VehicleService::checkIn     →  BlacklistService::isBlacklisted(plate)
VehicleService::checkOut    →  BalanceService::deduct(userId, fee, ...)
ReservationService::create  →  BalanceService::deduct(userId, fee, ...)
                            →  ParkingService::getStatus() (间接)
PassPlanService::purchase   →  BalanceService::deduct(userId, price, ...)
                            →  BalanceService::refund(userId, price, ...) (失败时)
```

### 常用的 crow 工具

```cpp
BaseController::parseBody(req)                    // 解析 JSON 请求体
BaseController::authenticate(req)                 // 获取 {userId, role}
BaseController::checkPermission(req, "perm")      // 检查权限
BaseController::errorResponse(code, msg)          // 错误响应
BaseController::successResponse(msg)              // 成功响应
BaseController::toJsonArray(vec)                  // 列表序列化
```

### 数据库连接相关

```cpp
auto conn = getConnection();           // 从池拿连接
MYSQL* mysql = conn->get();            // 获取原始 MYSQL*
Transaction tx(mysql);                 // 开始事务
tx.commit();                           // 提交（析构自动回滚）
escape(mysql, str)                     // SQL 转义
quote(mysql, str)                      // 转义 + 加引号
executeQuery(mysql, sql)               // 执行 SQL（返回 bool）
executeQueryAffected(mysql, sql)       // 执行 SQL（返回影响行数）
```

# 完整操作总结

从零开始为 SmartParkingSystem 集成 OpenCV 车牌识别模块的完整过程。

---

## 第一阶段：项目初始状态

项目已是完整的管理系统（Crow + MySQL + 前端），采用 `.h/.cpp` 多文件结构，含 auth/parking/vehicle/reservation 等模块，**尚无 OpenCV 和车牌识别功能**。

---

## 第二阶段：新增 OpenCV 车牌识别模块

### 步骤 1 — 环境搭建（OpenCV 安装）
- **问题**: 系统中没有 OpenCV，编译会失败
- **操作**: 下载 OpenCV 4.10.0 Windows 安装包，解压到 `D:/opencv/opencv/`
- **验证**: 确认 OpenCV 目录结构完整（include、lib、dll）

### 步骤 2 — 创建核心识别类
- 新建 `src/plate_recognizer.h` — `PlateRecognizer` 单例类
- 新建 `src/plate_recognizer.cpp` — 完整实现：
  - base64 解码 → OpenCV `imdecode` 解码
  - 图像预处理（灰度 → 高斯模糊 → 自适应阈值 → 形态学闭运算）
  - 车牌区域检测（轮廓面积 1000-50000、宽高比 1.8-4.5、矩形度 >0.4）
  - HSV 颜色备选检测（蓝色 100-124°、绿色 35-77°）
  - 颜色分析（蓝/绿/黄牌）
  - 字符识别管道（CLAHE 增强 → 二值化 → 轮廓分割 → 模板匹配）

### 步骤 3 — 修改 CMakeLists.txt
- 添加 `find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)`
- 添加 OpenCV include 目录和链接库

### 步骤 4 — 改造 PlateService
- **`plate_service.h`**: 新增 `PlateRegistrationInfo` 结构体（含 is_registered、in_parking、has_monthly_pass、is_blacklisted 等字段）
- **`plate_service.cpp`**: `recognize()` 委托给 `PlateRecognizer`；新增 `checkRegistration()` 执行四表联合查询（CAR_RECORD / MONTHLY_PASS / VEHICLE_BLACKLIST）

### 步骤 5 — 改造 PlateController
- 新增 `POST /api/plate/recognize-image` — 接收 base64 → 识别 → 查数据库 → 返回完整结果
- 新增 `POST /api/plate/check-registered` — 手动查询车牌登记状态
- 保持原有 `POST /api/plate/recognize` 和 `POST /api/plate/validate` 不变

### 步骤 6 — 创建前端页面
- 新建 `frontend/recognize.html` — 全屏布局 + 侧边栏 + 摄像头预览 + 结果显示
- 新建 `frontend/js/recognize.js` — `getUserMedia` 调摄像头、canvas 截帧、API 调用、localStorage 历史记录

### 步骤 7 — 集成到现有前端
- `dashboard.html`: 添加车牌识别入口卡片 + 侧边导航链接
- `common.js`: 添加 `plate.recognize` 权限隐藏逻辑
- 其余 5 个页面（vehicles、reservation、admin、profile、init）：统一添加侧边导航链接

### 步骤 8 — 编译错误修复
1. **`cv::imdecode` 未声明**: 缺少 `#include <opencv2/imgcodecs.hpp>` → 添加
2. **`ConnGuard::operator!` 不支持**: `!(*conn)` 编译失败 → 改为 `!conn->get()`
3. **EXE 被锁定**: 服务器进程未关闭 → 停止进程后重建
4. **变量名笔误**: `letter_templates_[l]` 应为 `letter_templates_[i]` → 修复

---

## 第三阶段：实际测试

- **启动服务器**，用户用 root 账号登录测试
- **结果**: 能检测到车牌区域和颜色，但**字符识别几乎全部失败**
- 问题根源: Hershey 字体与真实车牌字体差异过大，像素级 `matchTemplate` 对此极度敏感

---

## 第四阶段：第 2 轮改进

针对"明明图片清晰，但识别不了字符"的反馈：

1. 新增 **CLAHE** 对比度增强 → 改善光照不均
2. 车牌区域统一缩放到**高度 80px**
3. 双阈值二值化策略：Otsu 和自适应阈值**自动选优**（选更接近 25% 文字占比的）
4. 切换为**轮廓法**字符分割，替代垂直投影法
5. 增加**重叠检测合并** → 避免重复识别同一字符
6. 增加**孔洞数**拓扑特征预过滤

**结果**: 仍有改善空间，用户反馈"依然是识别准确率太低"

---

## 第五阶段：第 3 轮改进

1. **彻底弃用** `cv::matchTemplate`，改用 `cv::matchShapes`（Hu 不变矩）
   - Hu 矩对缩放、旋转、字体风格不敏感 → 根本解决字体差异问题
2. 模板和字符轮廓**预计算**，匹配时直接比较轮廓
3. **孔洞数严格过滤**：每个字符预计算孔洞数（8→2 个孔，0/A/P→1 个孔，1→0 个孔），快速排除不可能候选
4. 分割字符加 **4px 黑色边框**再归一化到 28×44，保留更多形状信息

---

## 第六阶段：文档与提交

1. 用户要求"每一步改进都说明" → 创建 `docs/plate_recognition_devlog.md`
   - 记录 3 次迭代的改进内容、结果、失败原因
   - 架构流程图、当前局限、未来可集成方案（EasyPR / PaddleOCR / 自定义 CNN）

2. **Git 提交**（2 个 commits）:
   - `368cb87` — feat: 集成 OpenCV 车牌识别并支持数据库比对（14 文件，+1229 行）
   - `a1383dd` — docs: 添加车牌识别模块开发记录（1 文件，+102 行）

3. **推送到 GitHub**

---

## 最终架构

```
浏览器摄像头 → getUserMedia → canvas.toDataURL(base64)
    ↓ POST /api/plate/recognize-image
服务端 PlateRecognizer:
    base64解码 → imdecode → detectPlateCandidates()
        ├─ HSV颜色检测 (蓝/绿色范围)
        └─ 边缘检测 + 轮廓分析 (面积/宽高比/矩形度)
    ↓
    analyzePlateColor() → 蓝/绿/黄牌识别
    ↓
    recognizeCharacters():
        ├─ preprocessPlateRegion() → CLAHE + 缩放 + 高斯模糊
        ├─ binarizePlate() → Otsu/自适应阈值自动选优
        ├─ segmentCharacters() → 轮廓法分割 + 重叠合并
        └─ matchCharacter() → Hu矩形状匹配 + 孔洞数过滤
    ↓
    checkRegistration() → 四表联合查询
        ├─ CAR_RECORD (入场记录/是否在场)
        ├─ MONTHLY_PASS (月卡有效期)
        └─ VEHICLE_BLACKLIST (黑名单)
    ↓
返回 JSON: { plate_number, confidence, color, registration: {...} }
```

## 当前局限

- 中文省份字符无法识别（输出 `?` 占位）— 需 ML 模型
- Hu 矩形状匹配无训练数据，对不清晰/倾斜/光照差的车牌仍不稳定
- 字符分割依赖二值化质量，复杂背景可能引入噪点

# 车牌识别模块开发记录

## 概述

基于 OpenCV 4.x 的车牌识别模块，集成在 SmartParking 系统中。支持摄像头拍照识别、与数据库已登记车辆比对。

---

## 迭代过程

### 第 1 版 — 基础框架搭建

**目标**: 实现从摄像头拍照 → 服务器识别 → 数据库比对的完整流程

**实现**:
- 新建 `PlateRecognizer` 类，实现 base64 解码 → OpenCV 解码 → 图像预处理
- 车牌检测: 自适应阈值 → 边缘检测 → 轮廓筛选(按面积/宽高比/矩形度)
- 颜色检测作为备选方案 (HSV 蓝色/绿色范围)
- 字符分割: 垂直投影法 + Otsu 二值化
- 字符识别: 使用 Hershey 字体生成模板 → `matchTemplate` 像素级模板匹配
- 模板加粗 (dilate 2 次) 以接近真实车牌字体

**新增文件**:
- `src/plate_recognizer.h`
- `src/plate_recognizer.cpp`
- `frontend/recognize.html`
- `frontend/js/recognize.js`

**结果**: ❌ 能检测到车牌区域和颜色，但字符识别几乎全部失败
**原因**: Hershey 字体与真实车牌字体差异过大，像素级模板匹配对字体风格极度敏感

---

### 第 2 版 — 改进预处理和模板

**目标**: 提高字符识别率

**改进**:
- 新增 CLAHE 对比度增强 → 改善光照不均
- 车牌区域统一缩放到高度 80px → 保证处理一致性
- 双阈值二值化策略: Otsu 和自适应阈值自动选优
- 切换到轮廓法字符分割 (比投影法更鲁棒)
- 增加重叠检测 → 避免重复识别同一字符
- 模板匹配时按宽高比预筛候选

**结果**: ❌ 仍不能可靠识别字符，根本问题未解决

---

### 第 3 版 — Hu矩形状匹配(当前)

**目标**: 用形状匹配彻底取代像素模板匹配

**改进**:
- 弃用 `cv::matchTemplate`，改用 `cv::matchShapes` (Hu 不变矩)
- Hu 矩对缩放、旋转、字体风格不敏感 → 解决字体差异问题
- 孔洞数拓扑过滤: 预先计算每个字符的孔洞数(如 8→2, 0/A/P→1, 1→0) 快速排除不可能候选
- 模板和字符的轮廓预计算，匹配时直接比较轮廓
- 分割字符加 4px 边框再归一化，保留更多形状信息

**结果**: ⚠️ 准确率相比前两版有提升，但对不清晰或有遮挡的车牌仍不稳定

---

## 架构说明

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
    PlateService.checkRegistration() → 四表联合查询
        ├─ CAR_RECORD (是否曾有入场记录/当前是否在场)
        ├─ MONTHLY_PASS (月卡是否有效)
        └─ VEHICLE_BLACKLIST (是否黑名单)
    ↓
返回 JSON: { plate_number, confidence, color, registration: {...} }
```

## 当前局限

1. 中文省份字符无法识别 (输出 `?` 占位) — 需 ML 模型
2. 对倾斜角度过大、光照极差的车牌仍然不可靠
3. 字符分割依赖二值化质量，复杂背景可能引入噪点
4. 仅使用 OpenCV 内置功能，没有训练数据

## 未来可集成方案

- **EasyPR** — 专为中国车牌设计的开源识别库，支持中文省份字符
- **PaddleOCR** — 百度飞桨 OCR 引擎，支持端到端车牌识别
- **自定义 CNN** — 训练专用车牌字符分类器

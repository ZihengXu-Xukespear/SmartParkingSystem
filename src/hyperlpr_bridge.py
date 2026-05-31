# -*- coding: utf-8 -*-
"""
HyperLPR3 bridge for SmartParking.
Uses HyperLPR3 - specialized Chinese license plate recognition.

Usage: python hyperlpr_bridge.py <image_path>
Output: JSON with plate results (always UTF-8)
"""
import sys
import os
import json

# Force UTF-8 output regardless of system locale (Windows uses GBK by default)
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

# Use HF mirror for faster model downloads in China
os.environ['HF_ENDPOINT'] = 'https://hf-mirror.com'

def detect_plate_color(img, best_result):
    """Detect plate color from image using HyperLPR3's bbox (index 3)."""
    try:
        import cv2, numpy as np
        h, w = img.shape[:2]

        # HyperLPR3 format: [plate_text, confidence, flag, bbox]
        # bbox is [x1, y1, x2, y2]
        if len(best_result) > 3 and best_result[3] is not None:
            bbox = best_result[3]
            if isinstance(bbox, (list, np.ndarray)) and len(bbox) == 4:
                x1, y1, x2, y2 = int(bbox[0]), int(bbox[1]), int(bbox[2]), int(bbox[3])
                # Take inner 50% of plate region for clean color sample
                cx, cy = (x1 + x2) // 2, (y1 + y2) // 2
                bw, bh = abs(x2 - x1) // 3, abs(y2 - y1) // 3
                x1, y1 = max(0, cx - bw), max(0, cy - bh)
                x2, y2 = min(w, cx + bw), min(h, cy + bh)
                if x2 > x1 and y2 > y1:
                    roi = img[y1:y2, x1:x2]
                else:
                    roi = img[h//3:2*h//3, w//3:2*w//3]
            else:
                roi = img[h//3:2*h//3, w//3:2*w//3]
        else:
            roi = img[h//3:2*h//3, w//3:2*w//3]

        if roi.size == 0:
            return "unknown"

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        total = roi.shape[0] * roi.shape[1]

        blue_mask = cv2.inRange(hsv, (90, 30, 20), (140, 255, 255))
        green_mask = cv2.inRange(hsv, (30, 30, 20), (90, 255, 255))
        yellow_mask = cv2.inRange(hsv, (15, 30, 50), (40, 255, 255))

        bp = 100.0 * cv2.countNonZero(blue_mask) / total
        gp = 100.0 * cv2.countNonZero(green_mask) / total
        yp = 100.0 * cv2.countNonZero(yellow_mask) / total

        if bp >= gp and bp >= yp and bp > 0.5:
            return "蓝牌"
        if gp >= bp and gp >= yp and gp > 0.5:
            return "绿牌（新能源）"
        if yp >= bp and yp >= gp and yp > 0.5:
            return "黄牌"
        return "未知"
    except Exception:
        return "未知"

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No image path provided"}))
        sys.exit(1)

    image_path = sys.argv[1]
    if not os.path.exists(image_path):
        print(json.dumps({"error": f"File not found: {image_path}"}))
        sys.exit(1)

    try:
        import hyperlpr3
        import cv2
    except ImportError as e:
        print(json.dumps({"error": f"Import error: {e}. Run: pip install hyperlpr3"}))
        sys.exit(1)

    try:
        img = cv2.imread(image_path)
        if img is None:
            print(json.dumps({"error": f"Failed to read image: {image_path}"}))
            sys.exit(1)

        # Debug: save a copy for inspection
        debug_path = os.path.join(os.path.dirname(image_path), "sp_debug_input.jpg")
        cv2.imwrite(debug_path, img)

        # HyperLPR3 does detection + recognition in one call
        catcher = hyperlpr3.LicensePlateCatcher()
        results = catcher(img)

        if not results:
            print(json.dumps({
                "plate_number": "",
                "confidence": 0,
                "color": "未知",
                "message": "未检测到车牌"
            }, ensure_ascii=False))
            sys.exit(0)

        # Take the highest confidence result
        best = max(results, key=lambda r: r[1])
        raw_plate = best[0] or ""  # HyperLPR3 returns Unicode str
        plate_number = raw_plate.replace(' ', '')
        confidence = float(best[1])
        # Detect plate color from image (more reliable than HyperLPR3's field)
        color = detect_plate_color(img, best)

        print(json.dumps({
            "plate_number": plate_number,
            "confidence": round(confidence, 4),
            "color": color,
            "message": "识别成功" if confidence > 0.5 else "识别成功（置信度较低）",
            "all_results": [(r[0], float(r[1])) for r in results[:3]]
        }, ensure_ascii=False))

    except Exception as e:
        print(json.dumps({"error": str(e)}, ensure_ascii=False))
        sys.exit(1)

if __name__ == '__main__':
    main()

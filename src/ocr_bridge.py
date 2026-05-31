"""
OCR bridge for SmartParking license plate recognition.
Uses RapidOCR (ONNX-based, offline) for Chinese + English text recognition.

Usage: python ocr_bridge.py <image_path>
Output: JSON with recognized text to stdout
"""
import sys
import json
import os

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No image path provided"}))
        sys.exit(1)

    image_path = sys.argv[1]
    if not os.path.exists(image_path):
        print(json.dumps({"error": f"File not found: {image_path}"}))
        sys.exit(1)

    try:
        from rapidocr import RapidOCR
    except ImportError:
        print(json.dumps({"error": "RapidOCR not installed. Run: pip install rapidocr onnxruntime"}))
        sys.exit(1)

    # Initialize engine (models are cached after first load)
    engine = RapidOCR()

    try:
        output = engine(image_path)

        # Collect all recognized text lines
        texts = []
        if output.txts:
            texts = list(output.txts)

        # Flatten to single string, remove spaces for plate format
        combined = ''.join(texts)
        # Remove common OCR noise for license plates
        combined = combined.replace(' ', '')
        combined = combined.replace('·', '')
        combined = combined.replace('.', '')

        print(json.dumps({
            "text": combined,
            "lines": texts,
            "scores": list(output.scores) if output.scores else [],
            "elapsed_ms": round(output.elapse * 1000, 1)
        }, ensure_ascii=False))

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)

if __name__ == '__main__':
    main()

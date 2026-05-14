"""
Persistent OCR daemon for SmartParking.
Communicates via stdin/stdout protocol to avoid model reload overhead.

Protocol:
  Input (stdin):  <image_path>\n
  Output (stdout): {"text": "...", "lines": [...], "scores": [...], "elapsed_ms": N}\n

Reads lines from stdin, processes each as an image path, writes JSON results to stdout.
Exits when stdin closes or receives empty line.
"""
import sys
import json
import os

def main():
    # Suppress RapidOCR's internal logging to keep stdout clean
    import logging
    for name in ['RapidOCR', 'rapidocr', '']:
        logging.getLogger(name).setLevel(logging.ERROR)
    # Also suppress onnxruntime verbose logs
    logging.getLogger('onnxruntime').setLevel(logging.ERROR)

    try:
        from rapidocr import RapidOCR
    except ImportError:
        print(json.dumps({"error": "RapidOCR not installed"}))
        sys.exit(1)

    # Initialize engine ONCE - models stay loaded in memory
    engine = RapidOCR()

    # Check for one-shot mode (image path as argument)
    if len(sys.argv) > 1:
        image_path = sys.argv[1]
        if not os.path.exists(image_path):
            print(json.dumps({"error": f"File not found: {image_path}"}))
            sys.exit(1)
        output = engine(image_path)
        texts = list(output.txts) if output.txts else []
        combined = ''.join(texts).replace(' ', '').replace('·', '').replace('.', '')
        # Filter to only valid plate characters
        cleaned = []
        for ch in combined:
            cp = ord(ch)
            if (0x4E00 <= cp <= 0x9FFF) or \
               (ord('A') <= cp <= ord('Z')) or \
               (ord('a') <= cp <= ord('z')) or \
               (ord('0') <= cp <= ord('9')):
                cleaned.append(ch)
        combined = ''.join(cleaned).upper()
        print(json.dumps({
            "text": combined,
            "lines": texts,
            "scores": list(output.scores) if output.scores else [],
            "elapsed_ms": round(output.elapse * 1000, 1)
        }, ensure_ascii=False))
        sys.exit(0)

    # Pipe mode: read lines from stdin, write JSON to stdout
    print(json.dumps({"status": "ready"}), flush=True)

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        image_path = line
        if not os.path.exists(image_path):
            print(json.dumps({"error": f"File not found: {image_path}"}), flush=True)
            continue

        try:
            output = engine(image_path)

            texts = list(output.txts) if output.txts else []
            combined = ''.join(texts)
            # Remove separator dots and spaces
            combined = combined.replace(' ', '')
            combined = combined.replace('·', '')
            combined = combined.replace('.', '')

            # Filter to only valid plate characters: CJK Chinese + A-Z + 0-9
            cleaned = []
            for ch in combined:
                cp = ord(ch)
                if (0x4E00 <= cp <= 0x9FFF) or \
                   (ord('A') <= cp <= ord('Z')) or \
                   (ord('a') <= cp <= ord('z')) or \
                   (ord('0') <= cp <= ord('9')):
                    cleaned.append(ch)
            result_text = ''.join(cleaned).upper()

            print(json.dumps({
                "text": result_text,
                "lines": texts,
                "scores": list(output.scores) if output.scores else [],
                "elapsed_ms": round(output.elapse * 1000, 1)
            }, ensure_ascii=False), flush=True)

        except Exception as e:
            print(json.dumps({"error": str(e)}), flush=True)

if __name__ == '__main__':
    main()

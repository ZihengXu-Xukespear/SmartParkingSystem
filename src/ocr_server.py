"""
Persistent OCR HTTP server for SmartParking.
Loads RapidOCR ONNX models ONCE, serves OCR requests via HTTP.

Start:    python ocr_server.py [--port 8765]
Test:     curl -X POST http://localhost:8765/ocr -d '{"path":"/tmp/plate.png"}'
"""
import sys
import json
import os
import logging

# Suppress logging noise
for name in ['RapidOCR', 'rapidocr', 'onnxruntime', '']:
    logging.getLogger(name).setLevel(logging.ERROR)

try:
    from rapidocr import RapidOCR
except ImportError:
    print(json.dumps({"error": "RapidOCR not installed. Run: pip install rapidocr onnxruntime"}))
    sys.exit(1)

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765

# Load models ONCE at startup
print(f"Loading OCR models...", file=sys.stderr, flush=True)
engine = RapidOCR()
print(f"OCR server ready on port {PORT}", file=sys.stderr, flush=True)

from http.server import HTTPServer, BaseHTTPRequestHandler

class OCRHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/ocr':
            self.send_response(404)
            self.end_headers()
            return

        try:
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length).decode('utf-8')
            data = json.loads(body)
            image_path = data.get('path', '')

            if not image_path or not os.path.exists(image_path):
                self.send_json({"error": f"File not found: {image_path}"})
                return

            # Run OCR
            output = engine(image_path)
            texts = list(output.txts) if output.txts else []
            combined = ''.join(texts).replace(' ', '').replace('·', '').replace('.', '')

            # Filter to valid plate characters only (CJK + A-Z + 0-9)
            cleaned = []
            for ch in combined:
                cp = ord(ch)
                if (0x4E00 <= cp <= 0x9FFF) or \
                   (ord('A') <= cp <= ord('Z')) or \
                   (ord('a') <= cp <= ord('z')) or \
                   (ord('0') <= cp <= ord('9')):
                    cleaned.append(ch)
            text = ''.join(cleaned).upper()

            self.send_json({
                "text": text,
                "lines": texts,
                "scores": list(output.scores) if output.scores else [],
                "elapsed_ms": round(output.elapse * 1000, 1)
            })

        except Exception as e:
            self.send_json({"error": str(e)})

    def send_json(self, data, status=200):
        body = json.dumps(data, ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        pass  # Suppress HTTP access logs

# Start server
server = HTTPServer(('127.0.0.1', PORT), OCRHandler)
try:
    server.serve_forever()
except KeyboardInterrupt:
    pass
finally:
    server.server_close()

#include "plate_recognizer.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>

#ifdef ENABLE_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

PlateRecognizer& PlateRecognizer::instance() {
    static PlateRecognizer inst;
    return inst;
}

// ========== Base64 Decode ==========
static const std::string b64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::vector<uchar> PlateRecognizer::base64Decode(const std::string& data) {
    std::vector<uchar> out;
    std::string clean;
    for (char c : data)
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') clean += c;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) T[b64_chars[i]] = i;
    int val = 0, bits = -8;
    for (char c : clean) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uchar>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ========== Python discovery (works with or without OpenCV) ==========
// Collect every python on PATH instead of only the first one, so we can pick
// the interpreter that actually has the OCR dependencies installed.
static std::vector<std::string> pythonCandidates() {
    std::vector<std::string> out;
#ifdef _WIN32
    FILE* fp = _popen("where python 2>nul", "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            std::string s(line);
            s.erase(s.find_last_not_of(" \r\n") + 1);
            if (!s.empty()) out.push_back(s);
        }
        _pclose(fp);
    }
    out.push_back("python");
#else
    out.push_back("python3");
    out.push_back("python");
#endif
    return out;
}

// Returns true if the interpreter can import the OCR deps the bridge needs.
static bool pythonHasOcrDeps(const std::string& python) {
    std::string cmd = "\"" + python + "\" -c \"import hyperlpr3;import cv2\"";
#ifdef _WIN32
    cmd += " 2>nul";
    FILE* fp = _popen(cmd.c_str(), "r");
#else
    cmd += " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
#endif
    if (!fp) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) { /* drain */ }
#ifdef _WIN32
    int status = _pclose(fp);
    return status == 0;
#else
    int status = pclose(fp);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static std::string findPython() {
    static std::string resolved;
    static bool done = false;
    if (done) return resolved;

    for (const auto& cand : pythonCandidates()) {
        if (pythonHasOcrDeps(cand)) { resolved = cand; done = true; return resolved; }
    }
    // Nothing could import the deps — fall back to the first candidate so the
    // bridge itself can surface a clear "pip install hyperlpr3" error.
    auto cands = pythonCandidates();
    resolved = cands.empty() ? "python" : cands.front();
    done = true;
    return resolved;
}

static std::string findHyperLprBridge() {
    static const char* cand[] = {
        "hyperlpr_bridge.py", "../hyperlpr_bridge.py",
        "src/hyperlpr_bridge.py", "../src/hyperlpr_bridge.py",
    };
    for (auto p : cand) {
        std::ifstream f(p);
        if (f.good()) return p;
    }
    return "hyperlpr_bridge.py";
}

// Parse JSON field value (simple parser, avoids dependency)
static std::string jsonField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    auto colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return "";

    auto vs = colon + 1;
    while (vs < json.size() && (json[vs] == ' ' || json[vs] == '\t')) vs++;
    if (vs >= json.size()) return "";

    if (json[vs] == '"') {
        vs++;
        auto ve = json.find('"', vs);
        if (ve == std::string::npos) return "";
        return json.substr(vs, ve - vs);
    }
    auto ve = json.find_first_of(",}\n\r \t", vs);
    if (ve == std::string::npos) return json.substr(vs);
    return json.substr(vs, ve - vs);
}

// ========== Get temp directory ==========
static std::string getTempDir() {
#ifdef _WIN32
    char tmpbuf[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpbuf);
    return tmpbuf;
#else
    return "/tmp/";
#endif
}

// ========== Call Python bridge with saved image ==========
static PlateRecognizer::RecognitionResult callPythonBridge(const std::string& imgPath) {
    PlateRecognizer::RecognitionResult result;

    std::string python = findPython();
    std::string script = findHyperLprBridge();
    std::string cmd = python + " -u \"" + script + "\" \"" + imgPath + "\"";

    std::string resp;
#ifdef _WIN32
    FILE* fp = _popen(cmd.c_str(), "r");
#else
    FILE* fp = popen(cmd.c_str(), "r");
#endif
    if (fp) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp)) resp += buf;
#ifdef _WIN32
        _pclose(fp);
#else
        pclose(fp);
#endif
    }

    if (resp.empty()) {
        result.message = "OCR 服务调用失败，请检查 Python 环境";
        return result;
    }

    result.plate_number = jsonField(resp, "plate_number");
    result.plate_color = jsonField(resp, "color");
    std::string conf_str = jsonField(resp, "confidence");
    result.confidence = conf_str.empty() ? 0.0 : std::stod(conf_str);
    result.message = jsonField(resp, "message");
    if (result.message.empty() && !result.plate_number.empty())
        result.message = "识别成功";
    // Surface bridge errors (e.g. {"error":"Import error: ..."}) instead of a blank message
    if (result.message.empty())
        result.message = jsonField(resp, "error");

    return result;
}

// ========== Main API: recognize from base64 image (always available) ==========
PlateRecognizer::RecognitionResult PlateRecognizer::recognize(const std::string& image_base64) {
    RecognitionResult r;

    auto data = base64Decode(image_base64);
    if (data.empty()) { r.message = "图片解码失败"; return r; }

    // Save decoded bytes to a temp file
    std::string imgpath = getTempDir() + "sp_camera_capture.jpg";
    {
        std::ofstream f(imgpath, std::ios::binary);
        if (!f) { r.message = "保存图片失败"; return r; }
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

#ifdef ENABLE_OPENCV
    // With OpenCV: load image, resize if needed, re-save
    cv::Mat img = cv::imread(imgpath, cv::IMREAD_COLOR);
    if (!img.empty()) {
        if (img.cols > 1280) {
            double s = 1280.0 / img.cols;
            cv::resize(img, img, cv::Size(1280, (int)(img.rows * s)));
        }
        cv::imwrite(imgpath, img);
    }
#endif

    r = callPythonBridge(imgpath);
    std::remove(imgpath.c_str());
    return r;
}

#ifdef ENABLE_OPENCV
// ========== OpenCV-specific: recognize from cv::Mat frame ==========
static PlateRecognizer::RecognitionResult ocrWithHyperLPR(const cv::Mat& img) {
    PlateRecognizer::RecognitionResult result;

    std::string imgpath = getTempDir() + "sp_hyperlpr.png";
    if (!cv::imwrite(imgpath, img)) {
        result.message = "保存图片失败";
        return result;
    }

    result = callPythonBridge(imgpath);
    std::remove(imgpath.c_str());
    return result;
}

PlateRecognizer::RecognitionResult PlateRecognizer::recognize(const cv::Mat& frame) {
    cv::Mat working = frame;
    if (frame.cols > 1280) {
        double s = 1280.0 / frame.cols;
        cv::resize(frame, working, cv::Size(1280, (int)(frame.rows * s)));
    }
    return ocrWithHyperLPR(working);
}

// Stub implementations (HyperLPR3 handles detection/color internally)
cv::Mat PlateRecognizer::preprocess(const cv::Mat&) { return cv::Mat(); }
std::vector<cv::Rect> PlateRecognizer::detectPlateCandidates(const cv::Mat&) { return {}; }
std::string PlateRecognizer::analyzePlateColor(const cv::Mat&) { return "unknown"; }
std::string PlateRecognizer::recognizeCharacters(const cv::Mat&, double&) { return ""; }
#endif // ENABLE_OPENCV

#pragma once
#include <string>
#include <vector>

#ifdef ENABLE_OPENCV
#include <opencv2/core.hpp>
#else
typedef unsigned char uchar;
#endif

class PlateRecognizer {
public:
    static PlateRecognizer& instance();

    struct RecognitionResult {
        std::string plate_number;
        double confidence = 0.0;
        std::string plate_color;
        std::string message;
    };

    RecognitionResult recognize(const std::string& image_base64);

#ifdef ENABLE_OPENCV
    RecognitionResult recognize(const cv::Mat& frame);
#endif

private:
    PlateRecognizer() = default;

#ifdef ENABLE_OPENCV
    // Image preprocessing
    cv::Mat preprocess(const cv::Mat& src);

    // Plate region detection
    std::vector<cv::Rect> detectPlateCandidates(const cv::Mat& src);

    // Plate color analysis
    std::string analyzePlateColor(const cv::Mat& plate_region);

    // OCR-based character recognition (delegates to RapidOCR via Python bridge)
    std::string recognizeCharacters(const cv::Mat& plate_img, double& confidence);
#endif

    // Base64 helper
    static std::vector<uchar> base64Decode(const std::string& data);
};

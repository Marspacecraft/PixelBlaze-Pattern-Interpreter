#include "pixelblaze.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdarg>
#include <getopt.h>

namespace pixelblaze_cpp {

void log_print(uint8_t level, const char *fmt, ...) {
    const char* tag;
    switch (level) {
        case LOG_LEVEL_DEBUG: tag = "[DBG]"; break;
        case LOG_LEVEL_INFO:  tag = "[INF]"; break;
        case LOG_LEVEL_WARN:  tag = "[WRN]"; break;
        case LOG_LEVEL_ERROR: tag = "[ERR]"; break;
        case LOG_LEVEL_SCRIPT: tag = "[SCR]"; break;
        default: tag = "[UNK]"; break;
    }
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "%s ", tag);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}

} // namespace pixelblaze_cpp

class TestPixelblaze : public pixelblaze_cpp::Pixelblaze {
public:
    explicit TestPixelblaze(std::size_t pixel_count = 16)
        : Pixelblaze(pixel_count), colors_(pixel_count) {}

    void setColor(uint16_t index, const pixelblaze_cpp::WS2812Color& color) override {
        if (index < colors_.size()) {
            colors_[index] = color;
        }
    }

    pixelblaze_cpp::WS2812Color& getColor(uint16_t index) const override {
        return const_cast<std::vector<pixelblaze_cpp::WS2812Color>&>(colors_)[index];
    }

    const std::vector<pixelblaze_cpp::WS2812Color>& getColors() const {
        return colors_;
    }

private:
    mutable std::vector<pixelblaze_cpp::WS2812Color> colors_;
};

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string getBaseName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string getFileNameWithoutExt(const std::string& path) {
    std::string base = getBaseName(path);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) {
        return base.substr(0, dot);
    }
    return base;
}

std::string colorToJson(const pixelblaze_cpp::WS2812Color& c) {
    return "[" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "]";
}

int main(int argc, char** argv) {
    std::string scriptPath;
    std::string outputFile;
    int pixelCount = 16;
    int frameCount = 1;

    int opt;
    while ((opt = getopt(argc, argv, "s:p:f:o:")) != -1) {
        switch (opt) {
            case 's': scriptPath = optarg; break;
            case 'p': pixelCount = std::stoi(optarg); break;
            case 'f': frameCount = std::stoi(optarg); break;
            case 'o': outputFile = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " -s <script> -p <pixel_count> -f <frames> [-o <output_file>]\n";
                return 1;
        }
    }

    if (scriptPath.empty()) {
        std::cerr << "Error: script path is required (-s).\n";
        return 1;
    }

    try {
        std::string source = readFile(scriptPath);
        pixelblaze_cpp::PixelblazeCompiler compiler;

        pixelblaze_cpp::Program program = compiler.compile(source);
        if (!compiler.parse_ok()) {
            std::cerr << "Failed to compile script.\n";
            return 1;
        }

        TestPixelblaze vm(pixelCount);
        vm.begin(program);
        
        if (outputFile.empty()) {
            std::string scriptBase = getFileNameWithoutExt(scriptPath);
            outputFile = scriptBase + "_pixelCount" + std::to_string(pixelCount) +
                         "_frameCount" + std::to_string(frameCount) + ".json";
        }

        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot create output file: " << outputFile << "\n";
            return 1;
        }

        out << "{\n";
        out << "  \"frameCount\": " << frameCount << ",\n";
        out << "  \"pixelCount\": " << pixelCount << ",\n";
        out << "  \"frames\": [\n";

        for (int frame = 0; frame < frameCount; ++frame) {
            vm.beforeRender(1000.0f/120.0f);
            vm.renderFrame();

            const auto& colors = vm.getColors();
            out << (frame == 0 ? "" : ",\n") << "    {\n";
            out << "      \"frame\": " << frame << ",\n";
            out << "      \"pixels\": [";
            for (std::size_t i = 0; i < colors.size(); ++i) {
                if (i > 0) out << ",";
                out << colorToJson(colors[i]);
            }
            out << "]\n    }";
        }

        out << "\n  ]\n}\n";
        out.close();

        std::cout << "Output written to: " << outputFile << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
#include <pch.h>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace WSS {

inline double GetScreenWidth(int monitorId) {
    GdkDisplay* display = gdk_display_get_default();
    GListModel* monitorList = gdk_display_get_monitors(display);
    const auto monitor = GDK_MONITOR(g_list_model_get_item(monitorList, monitorId));
    if (!monitor) {
        WSS_ERROR("Monitor ID '{}' does not exist.", monitorId);
        return -1;
    }

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    return geometry.width;
}

inline double GetScreenHeight(int monitorId) {
    GdkDisplay* display = gdk_display_get_default();
    GListModel* monitorList = gdk_display_get_monitors(display);
    const auto monitor = GDK_MONITOR(g_list_model_get_item(monitorList, monitorId));
    if (!monitor) {
        WSS_ERROR("Monitor ID '{}' does not exist.", monitorId);
        return -1;
    }

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    return geometry.height;
}

class DimensionParser {
  public:
    enum class DimensionType { WIDTH, HEIGHT };
    static int Parse(const DimensionType type, const std::string& size, const int monitorId) {
        std::string expression = size;
        std::smatch match;
        const std::regex percentPattern(R"((\d+)%(\s*))");
        while (std::regex_search(expression, match, percentPattern)) {
            std::string percentStr = match[1].str();
            const double percentValue = std::stod(percentStr);
            const double screenDimension = (type == DimensionType::WIDTH) ? GetScreenWidth(monitorId) : GetScreenHeight(monitorId);
            const double pixelValue = (percentValue / 100.0) * screenDimension;
            expression.replace(match.position(), match.length(), std::to_string(static_cast<int>(pixelValue)));
        }
        const std::regex fractionPattern(R"((\d+)/(\d+)(\s*))");
        while (std::regex_search(expression, match, fractionPattern)) {
            const std::string numeratorStr = match[1].str();
            const std::string denominatorStr = match[2].str();

            const double numerator = std::stod(numeratorStr);
            const double denominator = std::stod(denominatorStr);

            const double fractionValue = numerator / denominator;
            const double screenDimension = (type == DimensionType::WIDTH) ? GetScreenWidth(monitorId) : GetScreenHeight(monitorId);
            const double pixelValue = fractionValue * screenDimension;

            expression.replace(match.position(), match.length(), std::to_string(static_cast<int>(pixelValue)));
        }

        return EvaluateExpression(expression);
    }

  private:
    static int EvaluateExpression(std::string expr) {
        expr.erase(std::remove_if(expr.begin(), expr.end(), ::isspace), expr.end());
        std::stringstream ss(expr);
        double result = 0.0;
        char op = '+';
        double value;

        while (ss >> value) {
            switch (op) {
            case '+':
                result += value;
                break;
            case '-':
                result -= value;
                break;
            case '*':
                result *= value;
                break;
            case '/':
                if (value == 0) {
                    throw std::invalid_argument("Division by zero in dimension expression.");
                }
                result /= value;
                break;
            }
            ss >> op;
        }

        return static_cast<int>(result);
    }

    static double ParseValue(const std::string& value) {
        try {
            return std::stod(value);
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid dimension format: " + value);
        }
    }
};

} // namespace WSS

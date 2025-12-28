#include "tui/BaseScreen.hpp"

void BaseScreen::ClearAndCenterLines(ncpp::Plane& plane, const std::vector<std::string>& lines) {
    plane.erase();
    unsigned rows = 0;
    unsigned cols = 0;
    plane.get_dim(rows, cols);

    const int total_lines = static_cast<int>(lines.size());
    const int mid_row = static_cast<int>(rows) / 2;

    for (int index = 0; index < total_lines; ++index) {
        const int row = mid_row - total_lines / 2 + index;
        plane.putstr(row, ncpp::NCAlign::Center, lines[static_cast<std::size_t>(index)].c_str());
    }
}

void BaseScreen::PutCentered(ncpp::Plane& plane, int row, const std::string& text) {
    plane.putstr(row, ncpp::NCAlign::Center, text.c_str());
}

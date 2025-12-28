#include "tui/Subframe.hpp"

Subframe::Subframe() : plane_(nullptr), cached_rows_(0), cached_cols_(0) {}

Subframe::~Subframe() = default;

void Subframe::Resize(ncpp::Plane& parent, unsigned parent_rows, unsigned parent_cols) {
    int y = 0;
    int x = 0;
    int rows = 0;
    int cols = 0;
    ComputeGeometry(parent_rows, parent_cols, y, x, rows, cols);

    if (rows <= 0 || cols <= 0) {
        plane_.reset();
        cached_rows_ = cached_cols_ = 0;
        return;
    }

    if (plane_ == nullptr || cached_rows_ != static_cast<unsigned>(rows) || cached_cols_ != static_cast<unsigned>(cols)) {
        plane_ = std::make_unique<ncpp::Plane>(&parent, rows, cols, y, x);
        cached_rows_ = static_cast<unsigned>(rows);
        cached_cols_ = static_cast<unsigned>(cols);
    } else {
        plane_->move(y, x);
    }
}

void Subframe::Draw() {
    if (plane_ == nullptr) {
        return;
    }
    plane_->erase();
    DrawContents();
}

void Subframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)input;
    (void)details;
}

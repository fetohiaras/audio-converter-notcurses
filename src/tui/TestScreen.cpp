#include "tui/TestScreen.hpp"

#include <notcurses/notcurses.h>

#include "tui/StateMachine.hpp"

void TestScreen::Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    stdplane.erase();
    stdplane.perimeter_rounded(0, 0, 0);
    // Center the main title inside the outer frame.
    unsigned rows = 0;
    unsigned cols = 0;
    stdplane.get_dim(rows, cols);
    stdplane.putstr(0, ncpp::NCAlign::Center, "Test Screen");

    subframe_.Resize(stdplane, rows, cols);
    subframe_.Draw();
}

void TestScreen::Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::HandleInput(StateMachine& machine,
                             ncpp::NotCurses& nc,
                             ncpp::Plane& stdplane,
                             uint32_t input,
                             const ncinput& details) {
    (void)nc;
    (void)stdplane;
    (void)details;
    subframe_.HandleInputPublic(input, details);
}

void TestScreen::TestSubframe::ComputeGeometry(unsigned parent_rows,
                                               unsigned parent_cols,
                                               int& y,
                                               int& x,
                                               int& rows,
                                               int& cols) {
    // Target a smaller footprint to test adaptive layout.
    rows = std::max(6, static_cast<int>(parent_rows) / 4);
    cols = std::max(20, static_cast<int>(parent_cols) / 4);
    y = (static_cast<int>(parent_rows) - rows) / 2;
    x = (static_cast<int>(parent_cols) - cols) / 2;
}

void TestScreen::TestSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Subframe 1");
    DrawList();
}

void TestScreen::TestSubframe::DrawList() {
    // Paddings to keep text away from borders.
    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;

    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    const int start_row = area.top;
    const int start_col = area.left;
    const int content_width = area.width;
    const int visible_rows = area.height;
    const int item_count = static_cast<int>(items_.size());

    // Clamp scroll offset to keep selection visible.
    if (selected_index_ < scroll_offset_) {
        scroll_offset_ = selected_index_;
    } else if (selected_index_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = selected_index_ - visible_rows + 1;
    }

    for (int i = 0; i < visible_rows && (scroll_offset_ + i) < item_count; ++i) {
        const int item_index = scroll_offset_ + i;
        const bool is_selected = (item_index == selected_index_);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
            for (int col = start_col - 1; col < start_col + content_width; ++col) {
                plane_->putstr(start_row + i, col, " ");
            }
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        plane_->putstr(start_row + i, start_col, items_[static_cast<std::size_t>(item_index)].c_str());
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::TestSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    const int count = static_cast<int>(items_.size());
    if (count == 0) {
        return;
    }

    if (input == NCKEY_UP) {
        selected_index_ = (selected_index_ - 1 + count) % count;
    } else if (input == NCKEY_DOWN) {
        selected_index_ = (selected_index_ + 1) % count;
    }
}

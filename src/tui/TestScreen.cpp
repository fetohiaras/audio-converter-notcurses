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
    rows = static_cast<int>(parent_rows) / 2;
    cols = static_cast<int>(parent_cols) / 2;
    y = static_cast<int>(parent_rows) / 4;
    x = static_cast<int>(parent_cols) / 4;
}

void TestScreen::TestSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Subframe 1");
    DrawList();
}

void TestScreen::TestSubframe::DrawList() {
    const int start_row = 1;
    const int start_col = 2;
    const int max_rows = plane_->get_dim_y();
    const int content_width = plane_->get_dim_x() - start_col - 2; // leave right padding

    for (int i = 0; i < static_cast<int>(items_.size()) && (start_row + i) < max_rows; ++i) {
        const bool is_selected = (i == selected_index_);
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
        plane_->putstr(start_row + i, start_col, items_[static_cast<std::size_t>(i)].c_str());
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

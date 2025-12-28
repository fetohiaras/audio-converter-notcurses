#include <utility>

#include "tui/TestScreen.hpp"

#include <notcurses/notcurses.h>

#include "tui/StateMachine.hpp"

TestScreen::TestScreen()
    : jobs_(),
      focus_(Focus::Files),
      file_subframe_(true),
      job_subframe_(false, jobs_) {}

TestScreen::FileSubframe::FileSubframe(bool is_left) : is_left_(is_left) {}

TestScreen::JobSubframe::JobSubframe(bool is_left, std::vector<std::string>& jobs)
    : jobs_(&jobs), is_left_(is_left) {}

void TestScreen::Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
    file_subframe_.RefreshListing();
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

    file_subframe_.Resize(stdplane, rows, cols);
    job_subframe_.Resize(stdplane, rows, cols);
    file_subframe_.Draw();
    job_subframe_.Draw();
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
    if (input == '\t') {
        focus_ = (focus_ == Focus::Files) ? Focus::Jobs : Focus::Files;
        return;
    }

    if (focus_ == Focus::Files && input == 's') {
        const FileBrowser::Entry* entry = file_subframe_.CurrentEntry();
        if (entry != nullptr) {
            std::filesystem::path full = file_subframe_.CurrentPath() / entry->name;
            if (entry->is_dir && entry->name != "..") {
                full /= "";
            }
            jobs_.push_back(full.string());
        }
        return;
    }

    if (focus_ == Focus::Jobs && input == 's') {
        std::string removed = job_subframe_.RemoveSelected();
        (void)removed;
        return;
    }

    if (focus_ == Focus::Files) {
        file_subframe_.HandleInputPublic(input, details);
    } else {
        job_subframe_.HandleInputPublic(input, details);
    }
}

void TestScreen::FileSubframe::ComputeGeometry(unsigned parent_rows,
                                               unsigned parent_cols,
                                               int& y,
                                               int& x,
                                               int& rows,
                                               int& cols) {
    const int margin = 1;
    const int gap = 2;
    rows = std::max(6, static_cast<int>(parent_rows) / 2);
    int available_width = static_cast<int>(parent_cols) - (margin * 2) - gap;
    cols = std::max(20, available_width / 2);
    y = margin;
    x = is_left_ ? margin : margin + cols + gap;
}

void TestScreen::FileSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "File Selection");
    DrawList();
}

void TestScreen::FileSubframe::DrawList() {
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
    const std::vector<FileBrowser::Entry>& entries = browser_.Entries();
    const int item_count = static_cast<int>(entries.size());
    const int selected_index = static_cast<int>(browser_.SelectedIndex());

    if (selected_index != static_cast<int>(last_selected_index_)) {
        horizontal_offset_ = 0;
        last_selected_index_ = static_cast<std::size_t>(selected_index);
    }

    // Clamp scroll offset to keep selection visible.
    if (selected_index < scroll_offset_) {
        scroll_offset_ = selected_index;
    } else if (selected_index >= scroll_offset_ + visible_rows) {
        scroll_offset_ = selected_index - visible_rows + 1;
    }

    for (int i = 0; i < visible_rows && (scroll_offset_ + i) < item_count; ++i) {
        const int item_index = scroll_offset_ + i;
        const bool is_selected = (item_index == selected_index);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
            for (int col = start_col - 1; col < start_col + content_width - 1; ++col) {
                plane_->putstr(start_row + i, col, " ");
            }
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        std::string label = entries[static_cast<std::size_t>(item_index)].name;
        if (entries[static_cast<std::size_t>(item_index)].is_dir && label != "..") {
            label.append("/");
        }
        const int line_width = std::max(0, content_width - 1);
        int offset = horizontal_offset_;
        int max_offset = static_cast<int>(label.size()) - line_width;
        if (max_offset < 0) {
            max_offset = 0;
        }
        if (offset > max_offset) {
            offset = max_offset;
        }
        std::string view = (line_width > 0 && offset < static_cast<int>(label.size()))
            ? label.substr(static_cast<std::size_t>(offset), static_cast<std::size_t>(line_width))
            : label;
        plane_->putstr(start_row + i, start_col, view.c_str());
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::FileSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    if (input == NCKEY_UP) {
        browser_.MoveSelectionUp();
        horizontal_offset_ = 0;
    } else if (input == NCKEY_DOWN) {
        browser_.MoveSelectionDown();
        horizontal_offset_ = 0;
    } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
        browser_.ActivateSelection();
        scroll_offset_ = 0;
        horizontal_offset_ = 0;
    } else if (input == NCKEY_RIGHT) {
        const FileBrowser::Entry* entry = CurrentEntry();
        if (entry != nullptr) {
            int line_width = 0;
            const int pad_left = 2;
            const int pad_right = 2;
            const ContentArea area = ContentBox(1, pad_left, 1, pad_right, 0, 0);
            line_width = std::max(0, area.width - 1);
            std::string label = entry->name;
            if (entry->is_dir && label != "..") {
                label.append("/");
            }
            int max_offset = static_cast<int>(label.size()) - line_width;
            if (max_offset < 0) {
                max_offset = 0;
            }
            if (horizontal_offset_ < max_offset) {
                ++horizontal_offset_;
            }
        }
    } else if (input == NCKEY_LEFT) {
        if (horizontal_offset_ > 0) {
            --horizontal_offset_;
        }
    }
}

const FileBrowser::Entry* TestScreen::FileSubframe::CurrentEntry() const {
    const std::vector<FileBrowser::Entry>& entries = browser_.Entries();
    if (entries.empty()) {
        return nullptr;
    }
    std::size_t idx = browser_.SelectedIndex();
    if (idx >= entries.size()) {
        return nullptr;
    }
    return &entries[idx];
}

void TestScreen::JobSubframe::ComputeGeometry(unsigned parent_rows,
                                              unsigned parent_cols,
                                              int& y,
                                              int& x,
                                              int& rows,
                                              int& cols) {
    const int margin = 1;
    const int gap = 2;
    rows = std::max(6, static_cast<int>(parent_rows) / 2);
    int available_width = static_cast<int>(parent_cols) - (margin * 2) - gap;
    cols = std::max(20, available_width / 2);
    y = margin;
    x = is_left_ ? margin : margin + cols + gap;
}

void TestScreen::JobSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Job List");
    DrawList();
}

void TestScreen::JobSubframe::DrawList() {
    if (jobs_ == nullptr) {
        return;
    }

    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;

    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    const int start_row = area.top;
    const int start_col = area.left;
    const int content_width = area.width;
    const int visible_rows = area.height;
    const int item_count = static_cast<int>(jobs_->size());

    if (selected_index_ < 0) selected_index_ = 0;
    if (item_count == 0) {
        scroll_offset_ = 0;
        return;
    }
    if (selected_index_ >= item_count) selected_index_ = item_count - 1;

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
            for (int col = start_col - 1; col < start_col + content_width - 1; ++col) {
                plane_->putstr(start_row + i, col, " ");
            }
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        const std::string& label = jobs_->at(static_cast<std::size_t>(item_index));
        const int line_width = std::max(0, content_width - 1);
        int offset = horizontal_offset_;
        int max_offset = static_cast<int>(label.size()) - line_width;
        if (max_offset < 0) {
            max_offset = 0;
        }
        if (offset > max_offset) {
            offset = max_offset;
        }
        std::string view = (line_width > 0 && offset < static_cast<int>(label.size()))
            ? label.substr(static_cast<std::size_t>(offset), static_cast<std::size_t>(line_width))
            : label;
        plane_->putstr(start_row + i, start_col, view.c_str());
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::JobSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    if (jobs_ == nullptr || jobs_->empty()) {
        return;
    }

    const int count = static_cast<int>(jobs_->size());

    if (input == NCKEY_UP) {
        selected_index_ = (selected_index_ - 1 + count) % count;
        horizontal_offset_ = 0;
    } else if (input == NCKEY_DOWN) {
        selected_index_ = (selected_index_ + 1) % count;
        horizontal_offset_ = 0;
    } else if (input == NCKEY_RIGHT) {
        const std::string& label = jobs_->at(static_cast<std::size_t>(selected_index_));
        const int pad_left = 2;
        const int pad_right = 2;
        const ContentArea area = ContentBox(1, pad_left, 1, pad_right, 0, 0);
        const int line_width = std::max(0, area.width - 1);
        int max_offset = static_cast<int>(label.size()) - line_width;
        if (max_offset < 0) {
            max_offset = 0;
        }
        if (horizontal_offset_ < max_offset) {
            ++horizontal_offset_;
        }
    } else if (input == NCKEY_LEFT) {
        if (horizontal_offset_ > 0) {
            --horizontal_offset_;
        }
    }
}

std::string TestScreen::JobSubframe::RemoveSelected() {
    if (jobs_ == nullptr || jobs_->empty()) {
        return {};
    }
    if (selected_index_ < 0 || selected_index_ >= static_cast<int>(jobs_->size())) {
        return {};
    }
    std::string removed = jobs_->at(static_cast<std::size_t>(selected_index_));
    jobs_->erase(jobs_->begin() + selected_index_);
    if (selected_index_ >= static_cast<int>(jobs_->size())) {
        selected_index_ = static_cast<int>(jobs_->size()) - 1;
    }
    if (selected_index_ < 0) {
        selected_index_ = 0;
    }
    return removed;
}

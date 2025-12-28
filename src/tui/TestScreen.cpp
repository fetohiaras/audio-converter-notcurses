#include <utility>

#include "tui/TestScreen.hpp"

#include <notcurses/notcurses.h>
#include <cmath>
#include <utility>

#include "tui/StateMachine.hpp"

namespace {
constexpr int kMargin = 1;
constexpr int kGap = 2;
constexpr int kFooterRows = 4;

void ComputeLayout(unsigned parent_rows, int& top_rows, int& mid_rows, int& footer_y) {
    int avail = static_cast<int>(parent_rows) - (kMargin * 2) - (kGap * 2) - kFooterRows;
    if (avail < 12) {
        avail = 12;
    }
    top_rows = std::max(6, avail / 2);
    mid_rows = std::max(6, avail - top_rows);
    footer_y = kMargin + top_rows + kGap + mid_rows + kGap;
}
}
TestScreen::TestScreen(ConverterConfig& config, bool& config_changed)
    : jobs_(),
      focus_(Focus::Commands),
      config_(config),
      config_changed_(config_changed),
      file_subframe_(true),
      job_subframe_(false, jobs_),
      config_subframe_(true, config_, config_changed_),
      status_subframe_(false),
      command_subframe_() {}

TestScreen::FileSubframe::FileSubframe(bool is_left) : is_left_(is_left) {}

TestScreen::JobSubframe::JobSubframe(bool is_left, std::vector<std::string>& jobs)
    : jobs_(&jobs), is_left_(is_left) {}

TestScreen::ConfigSubframe::ConfigSubframe(bool is_left, ConverterConfig& config, bool& config_changed)
    : config_(config), config_changed_(config_changed) {
    (void)is_left;
}

TestScreen::SystemStatusSubframe::SystemStatusSubframe(bool is_left) {
    (void)is_left;
}

TestScreen::CommandSubframe::CommandSubframe() = default;

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
    config_subframe_.Resize(stdplane, rows, cols);
    status_subframe_.Resize(stdplane, rows, cols);
    command_subframe_.Resize(stdplane, rows, cols);
    file_subframe_.Draw();
    job_subframe_.Draw();
    config_subframe_.Draw();
    status_subframe_.Draw();
    command_subframe_.Draw();
}

void TestScreen::Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
    status_subframe_.Tick();
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
        if (focus_ == Focus::Commands) {
            focus_ = Focus::Files;
        } else if (focus_ == Focus::Files) {
            focus_ = Focus::Jobs;
        } else if (focus_ == Focus::Jobs) {
            focus_ = Focus::Config;
        } else if (focus_ == Focus::Config) {
            focus_ = Focus::Status;
        } else {
            focus_ = Focus::Commands;
        }
        return;
    }

    if (focus_ == Focus::Commands) {
        command_subframe_.HandleInputPublic(input, details);
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
    } else if (focus_ == Focus::Jobs) {
        job_subframe_.HandleInputPublic(input, details);
    } else {
        config_subframe_.HandleInputPublic(input, details);
    }
}

void TestScreen::FileSubframe::ComputeGeometry(unsigned parent_rows,
                                               unsigned parent_cols,
                                               int& y,
                                               int& x,
                                               int& rows,
                                               int& cols) {
    int top_rows = 0;
    int mid_rows = 0;
    int footer_y = 0;
    ComputeLayout(parent_rows, top_rows, mid_rows, footer_y);
    rows = top_rows;
    int available_width = static_cast<int>(parent_cols) - (kMargin * 2) - kGap;
    cols = std::max(20, available_width / 2);
    y = kMargin;
    x = is_left_ ? kMargin : kMargin + cols + kGap;
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
    int top_rows = 0;
    int mid_rows = 0;
    int footer_y = 0;
    ComputeLayout(parent_rows, top_rows, mid_rows, footer_y);
    rows = top_rows;
    int available_width = static_cast<int>(parent_cols) - (kMargin * 2) - kGap;
    cols = std::max(20, available_width / 2);
    y = kMargin;
    x = is_left_ ? kMargin : kMargin + cols + kGap;
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
void TestScreen::ConfigSubframe::ComputeGeometry(unsigned parent_rows,
                                                  unsigned parent_cols,
                                                  int& y,
                                                  int& x,
                                                  int& rows,
                                                  int& cols) {
    int top_rows = 0;
    int mid_rows = 0;
    int footer_y = 0;
    ComputeLayout(parent_rows, top_rows, mid_rows, footer_y);
    rows = mid_rows;
    int available_width = static_cast<int>(parent_cols) - (kMargin * 2) - kGap;
    cols = std::max(20, available_width / 2);
    y = kMargin + top_rows + kGap;
    x = kMargin;
}

void TestScreen::SystemStatusSubframe::ComputeGeometry(unsigned parent_rows,
                                                       unsigned parent_cols,
                                                       int& y,
                                                       int& x,
                                                       int& rows,
                                                       int& cols) {
    int top_rows = 0;
    int mid_rows = 0;
    int footer_y = 0;
    ComputeLayout(parent_rows, top_rows, mid_rows, footer_y);
    rows = mid_rows;
    int available_width = static_cast<int>(parent_cols) - (kMargin * 2) - kGap;
    cols = std::max(20, available_width / 2);
    y = kMargin + top_rows + kGap;
    x = kMargin + cols + kGap;
}

void TestScreen::ConfigSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Config Options");

    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 2; // leave space for edit line
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);

    switch (mode_) {
    case Mode::Submenus:
        DrawSubmenus(area);
        break;
    case Mode::Options:
    case Mode::EditBool:
    case Mode::EditValue:
        DrawOptions(area);
        DrawEditLine(area);
        break;
    }
}

void TestScreen::ConfigSubframe::DrawSubmenus(const ContentArea& area) {
    const int visible_rows = area.height;
    const int start_row = area.top;
    const int start_col = area.left;

    if (submenu_index_ < scroll_offset_) {
        scroll_offset_ = submenu_index_;
    } else if (submenu_index_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = submenu_index_ - visible_rows + 1;
    }

    for (int i = 0; i < visible_rows && (scroll_offset_ + i) < static_cast<int>(submenu_titles_.size()); ++i) {
        const int idx = scroll_offset_ + i;
        const bool is_selected = (idx == submenu_index_);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        plane_->putstr(start_row + i, start_col, submenu_titles_[static_cast<std::size_t>(idx)].c_str());
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::ConfigSubframe::DrawOptions(const ContentArea& area) {
    const int visible_rows = area.height;
    const int start_row = area.top;
    const int start_col = area.left;

    // ensure "Back" is first
    std::vector<std::string> labels;
    labels.reserve(current_options_.size() + 1);
    labels.push_back("(Back)");
    for (const Option& opt : current_options_) {
        std::string label = opt.label + ": ";
        if (opt.type == Option::Type::Bool) {
            label += config_.GetBool(opt.key, false) ? "true" : "false";
        } else if (opt.type == Option::Type::Int) {
            label += std::to_string(config_.GetInt(opt.key, 0));
        } else {
            label += config_.GetString(opt.key, "");
        }
        labels.push_back(label);
    }

    const int total_items = static_cast<int>(labels.size());
    if (option_index_ < scroll_offset_) {
        scroll_offset_ = option_index_;
    } else if (option_index_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = option_index_ - visible_rows + 1;
    }

    for (int i = 0; i < visible_rows && (scroll_offset_ + i) < total_items; ++i) {
        const int idx = scroll_offset_ + i;
        const bool is_selected = (idx == option_index_);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        plane_->putstr(start_row + i, start_col, labels[static_cast<std::size_t>(idx)].c_str());
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::ConfigSubframe::DrawEditLine(const ContentArea& area) {
    const int row = area.top + area.height; // first bottom padding row
    const int col = area.left;
    std::string line;

    if (mode_ == Mode::EditBool) {
        line = "Select: ";
        line += (bool_choice_ == 0) ? "[true] false" : "true [false]";
    } else if (mode_ == Mode::EditValue) {
        line = "Select: " + edit_buffer_;
    } else {
        return;
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
    plane_->putstr(row, col, line.c_str());
}

void TestScreen::SystemStatusSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "System Status");

    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);

    const int bar_row = area.top;
    const int bar_width = std::max(1, area.width - 1);
    // Advance progress and wrap.
    progress_ += fill_speed_;
    if (progress_ >= static_cast<double>(bar_width)) {
        progress_ = 0.0;
    }
    const int filled = static_cast<int>(progress_);

    // Clear bar area.
    plane_->set_bg_default();
    plane_->set_fg_default();
    for (int col = 0; col < bar_width; ++col) {
        plane_->putstr(bar_row, area.left + col, " ");
    }

    // Draw filled portion.
    plane_->set_bg_rgb8(255, 255, 255);
    plane_->set_fg_rgb8(0, 0, 0);
    for (int col = 0; col < filled; ++col) {
        plane_->putstr(bar_row, area.left + col, " ");
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::SystemStatusSubframe::Tick() {
    // Progress is advanced during Draw when we know the bar width.
}

void TestScreen::CommandSubframe::ComputeGeometry(unsigned parent_rows,
                                                  unsigned parent_cols,
                                                  int& y,
                                                  int& x,
                                                  int& rows,
                                                  int& cols) {
    int top_rows = 0;
    int mid_rows = 0;
    int footer_y = 0;
    ComputeLayout(parent_rows, top_rows, mid_rows, footer_y);
    rows = kFooterRows;
    cols = std::max(20, static_cast<int>(parent_cols) - (kMargin * 2));
    y = footer_y;
    x = kMargin;
}

void TestScreen::CommandSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Commands");

    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);

    DrawOptions(area);
    DrawFeedback(area);
}

void TestScreen::CommandSubframe::DrawOptions(const ContentArea& area) {
    const int row = area.top;
    int col = area.left;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        const bool is_selected = (i == selected_index_);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        plane_->putstr(row, col, options_[static_cast<std::size_t>(i)].c_str());
        col += static_cast<int>(options_[static_cast<std::size_t>(i)].size()) + 4;
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::CommandSubframe::DrawFeedback(const ContentArea& area) {
    const int row = area.top + 1;
    const int col = area.left;
    plane_->set_bg_default();
    plane_->set_fg_default();
    plane_->putstr(row, col, feedback_.c_str());
}

void TestScreen::CommandSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    const int count = static_cast<int>(options_.size());
    if (count == 0) {
        return;
    }
    if (input == NCKEY_LEFT) {
        selected_index_ = (selected_index_ - 1 + count) % count;
    } else if (input == NCKEY_RIGHT) {
        selected_index_ = (selected_index_ + 1) % count;
    } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
        const std::string& opt = options_[static_cast<std::size_t>(selected_index_)];
        if (opt == "Start") {
            feedback_ = "Conversion started";
        } else if (opt == "Stop") {
            feedback_ = "Conversion stopped";
        } else if (opt == "Exit") {
            feedback_ = "Exit requested";
        }
    }
}

void TestScreen::ConfigSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;

    if (mode_ == Mode::Submenus) {
        const int total = static_cast<int>(submenu_titles_.size());
        if (input == NCKEY_UP) {
            submenu_index_ = (submenu_index_ - 1 + total) % total;
        } else if (input == NCKEY_DOWN) {
            submenu_index_ = (submenu_index_ + 1) % total;
        } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            EnterOptions();
        }
        return;
    }

    if (mode_ == Mode::Options) {
        const int total_items = static_cast<int>(current_options_.size()) + 1; // includes Back
        if (input == NCKEY_UP) {
            option_index_ = (option_index_ - 1 + total_items) % total_items;
        } else if (input == NCKEY_DOWN) {
            option_index_ = (option_index_ + 1) % total_items;
        } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            if (option_index_ == 0) {
                EnterSubmenus();
            } else {
                const Option& opt = current_options_[static_cast<std::size_t>(option_index_ - 1)];
                if (opt.type == Option::Type::Bool) {
                    BeginEditBool();
                    bool_choice_ = config_.GetBool(opt.key, false) ? 0 : 1;
                } else if (opt.type == Option::Type::Int) {
                    BeginEditValue();
                    edit_buffer_ = std::to_string(config_.GetInt(opt.key, 0));
                } else {
                    BeginEditValue();
                    edit_buffer_ = config_.GetString(opt.key, "");
                }
            }
        }
        return;
    }

    if (mode_ == Mode::EditBool) {
        if (input == NCKEY_LEFT || input == NCKEY_RIGHT) {
            bool_choice_ = 1 - bool_choice_;
        } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            CommitBool();
        }
        return;
    }

    if (mode_ == Mode::EditValue) {
        const Option* opt = (option_index_ > 0 && option_index_ <= static_cast<int>(current_options_.size()))
                                ? &current_options_[static_cast<std::size_t>(option_index_ - 1)]
                                : nullptr;
        if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            CommitValue();
        } else if (input == NCKEY_BACKSPACE || input == 127) {
            if (!edit_buffer_.empty()) {
                edit_buffer_.pop_back();
            }
        } else if (opt != nullptr) {
            if (opt->type == Option::Type::Int) {
                if (input >= '0' && input <= '9') {
                    edit_buffer_.push_back(static_cast<char>(input));
                }
            } else {
                // Accept printable ASCII for string options.
                if (input >= 32 && input <= 126) {
                    edit_buffer_.push_back(static_cast<char>(input));
                }
            }
        }
        return;
    }
}

void TestScreen::ConfigSubframe::EnterOptions() {
    mode_ = Mode::Options;
    option_index_ = 0;
    scroll_offset_ = 0;
    current_options_.clear();

    if (submenu_index_ == 0) {
        current_options_.push_back(Option{"input_folder", "Input folder", Option::Type::String});
        current_options_.push_back(Option{"output_folder", "Output folder", Option::Type::String});
        current_options_.push_back(Option{"use_vbr", "Use VBR", Option::Type::Bool});
    } else if (submenu_index_ == 1) {
        current_options_.push_back(Option{"mp3_bitrate_kbps", "MP3 bitrate kbps", Option::Type::Int});
        current_options_.push_back(Option{"mp3_use_cbr", "MP3 use CBR", Option::Type::Bool});
    } else {
        current_options_.push_back(Option{"opus_bitrate_kbps", "Opus bitrate kbps", Option::Type::Int});
        current_options_.push_back(Option{"opus_use_vbr", "Opus use VBR", Option::Type::Bool});
        current_options_.push_back(Option{"opus_frame_size", "Opus frame size", Option::Type::Int});
    }
}

void TestScreen::ConfigSubframe::EnterSubmenus() {
    mode_ = Mode::Submenus;
    option_index_ = 0;
    scroll_offset_ = 0;
    ResetEditLine();
}

void TestScreen::ConfigSubframe::BeginEditBool() {
    mode_ = Mode::EditBool;
}

void TestScreen::ConfigSubframe::BeginEditValue() {
    mode_ = Mode::EditValue;
}

void TestScreen::ConfigSubframe::CommitBool() {
    if (option_index_ == 0 || option_index_ > static_cast<int>(current_options_.size())) {
        return;
    }
    const Option& opt = current_options_[static_cast<std::size_t>(option_index_ - 1)];
    config_.SetBool(opt.key, bool_choice_ == 0);
    config_changed_ = true;
    ResetEditLine();
    mode_ = Mode::Options;
}

void TestScreen::ConfigSubframe::CommitValue() {
    if (option_index_ == 0 || option_index_ > static_cast<int>(current_options_.size())) {
        return;
    }
    if (!edit_buffer_.empty()) {
        const Option& opt = current_options_[static_cast<std::size_t>(option_index_ - 1)];
        if (opt.type == Option::Type::Int) {
            config_.SetInt(opt.key, std::stoi(edit_buffer_));
        } else {
            config_.SetString(opt.key, edit_buffer_);
        }
        config_changed_ = true;
    }
    ResetEditLine();
    mode_ = Mode::Options;
}

void TestScreen::ConfigSubframe::ResetEditLine() {
    bool_choice_ = 0;
    edit_buffer_.clear();
}

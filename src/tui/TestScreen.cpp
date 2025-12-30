#include <utility>
#include <functional>

#include "tui/TestScreen.hpp"

#include <notcurses/notcurses.h>
#include <cmath>
#include <utility>
#include <filesystem>

#include "tui/StateMachine.hpp"

namespace {
constexpr int kMargin = 1;
constexpr int kGap = 2;
constexpr int kFooterRows = 6;

std::string NormalizePath(const std::string& raw) {
    std::string trimmed = raw;
    while (!trimmed.empty() && (trimmed.front() == '\"' || trimmed.front() == '\'')) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && (trimmed.back() == '\"' || trimmed.back() == '\'')) {
        trimmed.pop_back();
    }
    return trimmed;
}

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
      job_subframe_(false, jobs_, jobs_mutex_),
      config_subframe_(true, config_, config_changed_),
      job_config_subframe_(false, config_),
      command_subframe_() {}

TestScreen::~TestScreen() {
    stop_flag_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) {
        worker_.join();
    }
}

TestScreen::FileSubframe::FileSubframe(bool is_left) : is_left_(is_left) {}

TestScreen::JobSubframe::JobSubframe(bool is_left, std::vector<std::string>& jobs, std::mutex& jobs_mutex)
    : jobs_(&jobs), is_left_(is_left), jobs_mutex_(&jobs_mutex) {}

TestScreen::ConfigSubframe::ConfigSubframe(bool is_left, ConverterConfig& config, bool& config_changed)
    : config_(config), config_changed_(config_changed) {
    (void)is_left;
}

TestScreen::JobConfigSubframe::JobConfigSubframe(bool is_left, ConverterConfig& config)
    : config_(config) {
    (void)is_left;
    options_.push_back(Option{
        "output_folder",
        "Output folder",
        {
            std::string("Standard folder (") + config_.GetString("output_folder", "out") + ")",
            "Same folder as input"
        }
    });
    options_.push_back(Option{"use_vbr", "Use VBR", {"Yes", "No"}});
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

    file_subframe_.SetFocused(focus_ == Focus::Files);
    job_subframe_.SetFocused(focus_ == Focus::Jobs);
    config_subframe_.SetFocused(focus_ == Focus::Config);
    job_config_subframe_.SetFocused(focus_ == Focus::JobConfig);
    command_subframe_.SetFocused(focus_ == Focus::Commands);

    file_subframe_.Resize(stdplane, rows, cols);
    job_subframe_.Resize(stdplane, rows, cols);
    config_subframe_.Resize(stdplane, rows, cols);
    job_config_subframe_.Resize(stdplane, rows, cols);
    command_subframe_.Resize(stdplane, rows, cols);
    file_subframe_.Draw();
    job_subframe_.Draw();
    config_subframe_.Draw();
    job_config_subframe_.Draw();
    command_subframe_.Draw();
}

void TestScreen::Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
    job_subframe_.Tick();
}

// Resolve a user-supplied path against a base, ensuring it stays within base and is not a symlink.
namespace {
std::filesystem::path SafeOutputPath(const std::filesystem::path& raw,
                                     const std::filesystem::path& base,
                                     const std::function<void(const std::string&)>& feedback) {
    std::filesystem::path normalized = NormalizePath(raw.string());
    if (normalized.empty()) {
        feedback("Invalid output path; using default.");
        return base;
    }
    std::error_code ec;
    std::filesystem::path abs_base = std::filesystem::weakly_canonical(base, ec);
    if (ec) {
        abs_base = base;
    }
    std::filesystem::path weak = std::filesystem::weakly_canonical(normalized, ec);
    if (ec) {
        feedback("Output path error; using default.");
        return abs_base;
    }
    const auto base_str = abs_base.string();
    const auto weak_str = weak.string();
    if (weak_str.rfind(base_str, 0) != 0) {
        feedback("Output outside allowed folder; using default.");
        return abs_base;
    }
    // Reject symlinks in the resolved path.
    std::filesystem::path current;
    for (const auto& part : weak) {
        current /= part;
        if (std::filesystem::is_symlink(current, ec)) {
            feedback("Symlink in output path blocked; using default.");
            return abs_base;
        }
    }
    return weak;
}
} // namespace

void TestScreen::StartConversions() {
    if (converting_.load(std::memory_order_relaxed)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    if (jobs_.empty()) {
        command_subframe_.SetFeedback("No jobs to convert");
        return;
    }
    stop_flag_.store(false, std::memory_order_relaxed);
    converting_.store(true, std::memory_order_relaxed);
    worker_ = std::thread([this]() {
        while (!stop_flag_.load(std::memory_order_relaxed)) {
                std::string job_path;
                {
                    std::lock_guard<std::mutex> guard(jobs_mutex_);
                    if (jobs_.empty()) {
                        break;
                    }
                    job_path = jobs_.front();
                    jobs_.erase(jobs_.begin());
                }

                try {
                    const int bitrate_kbps = config_.GetInt("opus_bitrate_kbps", 128);
                    MP3ToOpusConverter converter(bitrate_kbps * 1000);
                    std::filesystem::path input(job_path);
                    std::filesystem::path raw_output = config_.GetString("output_folder", "out");
                    auto fb = [this](const std::string& msg) { command_subframe_.SetFeedback(msg); };
                    std::filesystem::path output_root = SafeOutputPath(raw_output, std::filesystem::absolute("out"), fb);
                    job_subframe_.BeginConversionDisplay(input.filename().string());
                    if (!std::filesystem::exists(output_root)) {
                        std::filesystem::create_directories(output_root);
                        // Restrict permissions (best-effort, POSIX).
                        std::filesystem::permissions(output_root,
                                                     std::filesystem::perms::owner_all,
                                                     std::filesystem::perm_options::replace);
                    }
                    if (std::filesystem::is_directory(input)) {
                    converter.ConvertDirectory(input.string(), output_root.string());
                } else {
                    std::filesystem::path out_file = output_root / input.filename();
                    out_file.replace_extension(".opus");
                    converter.SetProgressCallback([this](double p) {
                        job_subframe_.UpdateProgress(p);
                    });
                    converter.ConvertFile(input.string(), out_file.string());
                    command_subframe_.SetFeedback(std::string("Converted ") + input.filename().string() + ".");
                }
                    job_subframe_.EndConversionDisplay();
                } catch (const std::exception& e) {
                    command_subframe_.SetFeedback(std::string("Error: ") + e.what());
                    job_subframe_.EndConversionDisplay();
                    break;
                }
            }

        if (!stop_flag_.load(std::memory_order_relaxed)) {
            command_subframe_.SetFeedback("All jobs finished.");
        } else {
            command_subframe_.SetFeedback("Conversion stopped");
        }
        converting_.store(false, std::memory_order_relaxed);
    });
}

void TestScreen::StopConversions() {
    stop_flag_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) {
        worker_.join();
    }
    converting_.store(false, std::memory_order_relaxed);
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
            focus_ = Focus::JobConfig;
        } else {
            focus_ = Focus::Commands;
        }
        return;
    }

    if (focus_ == Focus::Commands) {
        if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            const std::string& opt = command_subframe_.SelectedOption();
            if (opt == "Start") {
                StartConversions();
                command_subframe_.SetFeedback("Conversion started");
            } else if (opt == "Stop") {
                StopConversions();
                command_subframe_.SetFeedback("Conversion stopped");
            } else if (opt == "Exit") {
                machine.SetRunning(false);
                command_subframe_.SetFeedback("Exit requested");
            }
        } else {
            command_subframe_.HandleInputPublic(input, details);
        }
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
    } else if (focus_ == Focus::Config) {
        config_subframe_.HandleInputPublic(input, details);
    } else if (focus_ == Focus::JobConfig) {
        job_config_subframe_.HandleInputPublic(input, details);
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
    uint64_t channels = 0;
    if (focused_) {
        ncchannels_set_fg_rgb8(&channels, 150, 200, 255);
        ncchannels_set_bg_default(&channels);
    }
    plane_->perimeter_rounded(0, channels, 0);
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
    const int bar_col = start_col + content_width - 1;
    const int text_width = std::max(0, content_width - 1);
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
        const int line_width = text_width;
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
        plane_->putstr(start_row + i, bar_col, " ");
    }

    // Draw scrollbar
    if (item_count > visible_rows && visible_rows > 0) {
        const int bar_height = visible_rows;
        const int thumb_height = std::max(1, (visible_rows * bar_height) / item_count);
        const int max_thumb_start = bar_height - thumb_height;
        const int thumb_start = (item_count - visible_rows == 0) ? 0
                                : (max_thumb_start * scroll_offset_) / (item_count - visible_rows);
        plane_->set_bg_rgb8(200, 200, 200);
        plane_->set_fg_rgb8(0, 0, 0);
        for (int i = 0; i < thumb_height; ++i) {
            const int row = start_row + thumb_start + i;
            plane_->putstr(row, bar_col, " ");
        }
        plane_->set_bg_default();
        plane_->set_fg_default();
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
    uint64_t channels = 0;
    if (focused_) {
        ncchannels_set_fg_rgb8(&channels, 150, 200, 255);
        ncchannels_set_bg_default(&channels);
    }
    plane_->perimeter_rounded(0, channels, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Job List");
    if (converting_display_) {
        DrawProgressBar();
        const int pad_top = 2;
        const int pad_left = 2;
        const int pad_bottom = 1;
        const int pad_right = 2;
        const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
        plane_->putstr(area.top, area.left, "Converting:");
        plane_->putstr(area.top + 1, area.left, converting_file_.c_str());
    } else {
        DrawProgressBar();
        DrawList();
    }
}

void TestScreen::JobSubframe::DrawList() {
    std::lock_guard<std::mutex> lock(*jobs_mutex_);
    if (jobs_ == nullptr || jobs_->empty()) {
        return;
    }

    const int pad_top = 2; // leave room for progress bar
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;

    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    const int start_row = area.top;
    const int start_col = area.left;
    const int content_width = area.width;
    const int visible_rows = area.height;
    const int bar_col = start_col + content_width - 1;
    const int text_width = std::max(0, content_width - 1);
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
        const int line_width = text_width;
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
        plane_->putstr(start_row + i, bar_col, " ");
    }

    // Draw scrollbar
    if (item_count > visible_rows && visible_rows > 0) {
        const int bar_height = visible_rows;
        const int thumb_height = std::max(1, (visible_rows * bar_height) / item_count);
        const int max_thumb_start = bar_height - thumb_height;
        const int thumb_start = (item_count - visible_rows == 0) ? 0
                                : (max_thumb_start * scroll_offset_) / (item_count - visible_rows);
        plane_->set_bg_rgb8(200, 200, 200);
        plane_->set_fg_rgb8(0, 0, 0);
        for (int i = 0; i < thumb_height; ++i) {
            const int row = start_row + thumb_start + i;
            plane_->putstr(row, bar_col, " ");
        }
        plane_->set_bg_default();
        plane_->set_fg_default();
    }

    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::JobSubframe::DrawProgressBar() {
    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    const int bar_row = area.top;
    const int bar_width = std::max(1, area.width - 1);
    plane_->set_bg_default();
    plane_->set_fg_default();
    for (int col = 0; col < bar_width; ++col) {
        plane_->putstr(bar_row, area.left + col, " ");
    }
    int filled = 0;
    bool local_converting = false;
    double local_progress_value = 0.0;
    {
        std::lock_guard<std::mutex> lock(convert_mutex_);
        local_converting = converting_display_;
        local_progress_value = progress_value_;
    }
    if (local_converting) {
        filled = static_cast<int>(local_progress_value * bar_width);
    } else {
        progress_ += fill_speed_;
        if (progress_ >= static_cast<double>(bar_width)) {
            progress_ = 0.0;
        }
        filled = static_cast<int>(progress_);
    }
    if (filled > bar_width) {
        filled = bar_width;
    }
    plane_->set_bg_rgb8(255, 255, 255);
    plane_->set_fg_rgb8(0, 0, 0);
    for (int col = 0; col < filled; ++col) {
        plane_->putstr(bar_row, area.left + col, " ");
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::JobSubframe::Tick() {
    // Progress advances during DrawProgressBar when width is known.
}

void TestScreen::JobSubframe::BeginConversionDisplay(const std::string& file_name) {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    converting_display_ = true;
    converting_file_ = file_name;
    progress_ = 0.0;
    progress_value_ = 0.0;
}

void TestScreen::JobSubframe::EndConversionDisplay() {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    converting_display_ = false;
    converting_file_.clear();
    progress_ = 0.0;
    progress_value_ = 0.0;
}

void TestScreen::JobSubframe::UpdateProgress(double value) {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    progress_value_ = value;
}

void TestScreen::JobSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    std::lock_guard<std::mutex> lock(*jobs_mutex_);
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
    std::lock_guard<std::mutex> lock(*jobs_mutex_);
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
    int y_base = kMargin + top_rows + kGap - 2;
    if (y_base < kMargin) {
        y_base = kMargin;
    }
    y = y_base;
    x = kMargin;
}

void TestScreen::JobConfigSubframe::ComputeGeometry(unsigned parent_rows,
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
    int y_base = kMargin + top_rows + kGap - 2;
    if (y_base < kMargin) {
        y_base = kMargin;
    }
    y = y_base;
    x = kMargin + cols + kGap;
}

void TestScreen::ConfigSubframe::DrawContents() {
    uint64_t channels = 0;
    if (focused_) {
        ncchannels_set_fg_rgb8(&channels, 150, 200, 255);
        ncchannels_set_bg_default(&channels);
    }
    plane_->perimeter_rounded(0, channels, 0);
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

void TestScreen::JobConfigSubframe::DrawContents() {
    uint64_t channels = 0;
    if (focused_) {
        ncchannels_set_fg_rgb8(&channels, 150, 200, 255);
        ncchannels_set_bg_default(&channels);
    }
    plane_->perimeter_rounded(0, channels, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Job Configuration");

    if (mode_ == Mode::List) {
        DrawOptions();
    } else {
        DrawChoice();
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::JobConfigSubframe::DrawOptions() {
    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    const int visible_rows = area.height;
    const int start_row = area.top;
    const int start_col = area.left;
    const int content_width = area.width;
    const int bar_col = start_col + content_width - 1;
    const int text_width = std::max(0, content_width - 1);
    const int item_count = static_cast<int>(options_.size());

    if (selected_index_ < scroll_offset_) {
        scroll_offset_ = selected_index_;
    } else if (selected_index_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = selected_index_ - visible_rows + 1;
    }

    for (int i = 0; i < visible_rows; ++i) {
        const int row = start_row + i;
        plane_->putstr(row, start_col, " ");
        plane_->putstr(row, bar_col, " ");
    }

    for (int i = 0; i < visible_rows && (scroll_offset_ + i) < item_count; ++i) {
        const int item_index = scroll_offset_ + i;
        const bool is_selected = (item_index == selected_index_);
        if (is_selected) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        const std::string& label = options_[static_cast<std::size_t>(item_index)].label;
        const std::string view = (text_width > 0)
            ? label.substr(0, static_cast<std::size_t>(text_width))
            : label;
        plane_->putstr(start_row + i, start_col, view.c_str());
    }
    plane_->set_bg_default();
    plane_->set_fg_default();

    if (item_count > visible_rows && visible_rows > 0) {
        const int bar_height = visible_rows;
        const int thumb_height = std::max(1, (visible_rows * bar_height) / item_count);
        const int max_thumb_start = bar_height - thumb_height;
        const int thumb_start = (item_count - visible_rows == 0) ? 0
                                : (max_thumb_start * scroll_offset_) / (item_count - visible_rows);
        plane_->set_bg_rgb8(200, 200, 200);
        plane_->set_fg_rgb8(0, 0, 0);
        for (int i = 0; i < thumb_height; ++i) {
            const int row = start_row + thumb_start + i;
            plane_->putstr(row, bar_col, " ");
        }
        plane_->set_bg_default();
        plane_->set_fg_default();
    }
}

void TestScreen::JobConfigSubframe::DrawChoice() {
    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);
    int row = area.top;
    const int col = area.left;

    const Option& opt = options_[static_cast<std::size_t>(selected_index_)];

    // Option title first.
    plane_->putstr(row++, col, (opt.label + ":").c_str());

    // Back entry
    if (choice_index_ == 0) {
        plane_->set_bg_rgb8(255, 255, 255);
        plane_->set_fg_rgb8(0, 0, 0);
    } else {
        plane_->set_bg_default();
        plane_->set_fg_default();
    }
    plane_->putstr(row++, col, "(Back)");
    plane_->set_bg_default();
    plane_->set_fg_default();

    for (int i = 0; i < static_cast<int>(opt.choices.size()); ++i) {
        if (choice_index_ == i + 1) {
            plane_->set_bg_rgb8(255, 255, 255);
            plane_->set_fg_rgb8(0, 0, 0);
        } else {
            plane_->set_bg_default();
            plane_->set_fg_default();
        }
        plane_->putstr(row++, col, opt.choices[static_cast<std::size_t>(i)].c_str());
    }
    plane_->set_bg_default();
    plane_->set_fg_default();
}

void TestScreen::JobConfigSubframe::HandleInput(uint32_t input, const ncinput& details) {
    (void)details;
    if (mode_ == Mode::List) {
        const int item_count = static_cast<int>(options_.size());
        if (item_count == 0) {
            return;
        }
        if (input == NCKEY_UP || input == NCKEY_BUTTON4) {
            selected_index_ = (selected_index_ - 1 + item_count) % item_count;
            if (selected_index_ < scroll_offset_) {
                scroll_offset_ = selected_index_;
            }
        } else if (input == NCKEY_DOWN || input == NCKEY_BUTTON5) {
            selected_index_ = (selected_index_ + 1) % item_count;
            if (selected_index_ >= scroll_offset_ + ContentBox(1, 2, 1, 2, 0, 0).height) {
                ++scroll_offset_;
            }
        } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
            mode_ = Mode::Choice;
            choice_index_ = 0;
        }
        return;
    }

    const Option& opt = options_[static_cast<std::size_t>(selected_index_)];
    const int choice_count = static_cast<int>(opt.choices.size()) + 1; // include Back
    if (input == NCKEY_UP || input == NCKEY_BUTTON4) {
        choice_index_ = (choice_index_ - 1 + choice_count) % choice_count;
    } else if (input == NCKEY_DOWN || input == NCKEY_BUTTON5) {
        choice_index_ = (choice_index_ + 1) % choice_count;
    } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
        if (choice_index_ == 0) {
            mode_ = Mode::List;
            return;
        }
        selection_map_[opt.key] = choice_index_ - 1;
        mode_ = Mode::List;
    }
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
    y = footer_y - 2; // lift footer by 2 rows to accommodate taller area
    x = kMargin;
}

void TestScreen::CommandSubframe::DrawContents() {
    uint64_t channels = 0;
    if (focused_) {
        ncchannels_set_fg_rgb8(&channels, 150, 200, 255);
        ncchannels_set_bg_default(&channels);
    }
    plane_->perimeter_rounded(0, channels, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Commands");

    const int pad_top = 1;
    const int pad_left = 2;
    const int pad_bottom = 1;
    const int pad_right = 2;
    const ContentArea area = ContentBox(pad_top, pad_left, pad_bottom, pad_right, 0, 0);

    // Show feedback first (above), commands on bottom line.
    DrawFeedback(area);
    DrawOptions(area);
}

void TestScreen::CommandSubframe::DrawOptions(const ContentArea& area) {
    const int row = area.top + area.height - 1;
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
    std::lock_guard<std::mutex> lock(log_mutex_);
    const int available_rows = std::max(0, area.height - 1);
    const int total = static_cast<int>(log_.size());
    const int lines_to_show = std::min(available_rows, total);
    int max_offset = std::max(0, total - lines_to_show);
    if (log_offset_ > max_offset) {
        log_offset_ = max_offset;
    }
    const int start_index = std::max(0, total - lines_to_show - log_offset_);

    plane_->set_bg_default();
    plane_->set_fg_default();
    const int bar_col = area.left + area.width - 1;
    const int text_width = std::max(0, area.width - 1);
    for (int i = 0; i < available_rows; ++i) {
        const int row = area.top + i;
        if (i < lines_to_show && text_width > 0) {
            const std::string& line = log_[static_cast<std::size_t>(start_index + i)];
            plane_->putstr(row, area.left, line.substr(0, static_cast<std::size_t>(text_width)).c_str());
        } else {
            plane_->putstr(row, area.left, " ");
        }
        // Clear scrollbar column; thumb is drawn below.
        plane_->putstr(row, bar_col, " ");
    }

    // Draw a simple scrollbar thumb if there is more content than fits.
    if (total > lines_to_show && available_rows > 0) {
        const int bar_height = available_rows;
        const int thumb_height = std::max(1, (lines_to_show * bar_height) / total);
        const int max_thumb_start = bar_height - thumb_height;
        int thumb_start = 0;
        if (max_offset > 0) {
            // Invert so scrolling up moves thumb up (toward top).
            thumb_start = max_thumb_start - ((max_thumb_start * log_offset_) / max_offset);
        }
        plane_->set_bg_rgb8(200, 200, 200);
        plane_->set_fg_rgb8(0, 0, 0);
        for (int i = 0; i < thumb_height; ++i) {
            const int row = area.top + thumb_start + i;
            plane_->putstr(row, bar_col, " ");
        }
        plane_->set_bg_default();
        plane_->set_fg_default();
    }
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
    } else if (input == NCKEY_UP || input == NCKEY_BUTTON4) {
        ++log_offset_;
    } else if (input == NCKEY_ENTER || input == '\n' || input == '\r') {
        const std::string& opt = options_[static_cast<std::size_t>(selected_index_)];
        if (opt == "Start") {
            SetFeedback("Conversion started");
        } else if (opt == "Stop") {
            SetFeedback("Conversion stopped");
        } else if (opt == "Exit") {
            SetFeedback("Exit requested");
        }
    } else if (input == NCKEY_DOWN || input == NCKEY_BUTTON5) {
        if (log_offset_ > 0) {
            --log_offset_;
        }
    }
}

void TestScreen::CommandSubframe::SetFeedback(const std::string& text) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    feedback_ = text;
    log_.push_back(text);
    if (log_.size() > 100) {
        log_.erase(log_.begin(), log_.begin() + static_cast<std::ptrdiff_t>(log_.size() - 100));
    }
    log_offset_ = 0;
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

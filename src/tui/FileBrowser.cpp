#include "tui/FileBrowser.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

FileBrowser::FileBrowser()
    : current_path_(std::filesystem::current_path()),
      selected_index_(0) {
    Refresh();
}

bool FileBrowser::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
        return false;
    }
    current_path_ = path;
    Refresh();
    return true;
}

void FileBrowser::MoveSelectionUp() {
    if (entries_.empty()) {
        return;
    }
    if (selected_index_ == 0) {
        selected_index_ = entries_.size() - 1;
    } else {
        --selected_index_;
    }
}

void FileBrowser::MoveSelectionDown() {
    if (entries_.empty()) {
        return;
    }
    selected_index_ = (selected_index_ + 1) % entries_.size();
}

void FileBrowser::ActivateSelection() {
    if (entries_.empty()) {
        return;
    }

    const Entry& entry = entries_[selected_index_];
    if (entry.name == "..") {
        if (current_path_.has_parent_path()) {
            current_path_ = current_path_.parent_path();
            Refresh();
        }
        return;
    }

    if (!entry.is_dir) {
        return;
    }

    current_path_ /= entry.name;
    Refresh();
}

void FileBrowser::Refresh() {
    entries_.clear();

    if (current_path_.has_parent_path()) {
        entries_.push_back(Entry{"..", true});
    }

    std::vector<Entry> dirs;
    std::vector<Entry> files;

    for (const std::filesystem::directory_entry& dirent : std::filesystem::directory_iterator(current_path_)) {
        const std::string name = dirent.path().filename().string();
        if (dirent.is_directory()) {
            dirs.push_back(Entry{name, true});
        } else {
            files.push_back(Entry{name, false});
        }
    }

    auto sorter = [](const Entry& a, const Entry& b) {
        return a.name < b.name;
    };
    std::sort(dirs.begin(), dirs.end(), sorter);
    std::sort(files.begin(), files.end(), sorter);

    entries_.insert(entries_.end(), dirs.begin(), dirs.end());
    entries_.insert(entries_.end(), files.begin(), files.end());

    selected_index_ = 0;
}

#ifndef TUI_FILEBROWSER_HPP
#define TUI_FILEBROWSER_HPP

#include <filesystem>
#include <string>
#include <vector>

// Simple filesystem helper to list entries, navigate into directories, and keep a selection.
class FileBrowser {
public:
    struct Entry {
        std::string name;
        bool is_dir;
    };

    FileBrowser();

    // Load a new directory; returns false if it could not be read.
    bool Load(const std::filesystem::path& path);

    // Move selection up/down with wrap-around.
    void MoveSelectionUp();
    void MoveSelectionDown();

    // Enter the selected directory (".." goes to parent). No-op on files.
    void ActivateSelection();

    const std::vector<Entry>& Entries() const { return entries_; }
    std::size_t SelectedIndex() const { return selected_index_; }
    std::filesystem::path CurrentPath() const { return current_path_; }

private:
    void Refresh();

    std::filesystem::path current_path_;
    std::vector<Entry> entries_;
    std::size_t selected_index_;
};

#endif // TUI_FILEBROWSER_HPP

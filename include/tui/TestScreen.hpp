#ifndef TUI_TESTSCREEN_HPP
#define TUI_TESTSCREEN_HPP

#include <memory>
#include <vector>

#include "tui/BaseScreen.hpp"
#include "tui/Subframe.hpp"
#include "tui/FileBrowser.hpp"

// Minimal test screen: just a framed title for layout experiments.
class TestScreen : public BaseScreen {
public:
    TestScreen();
    ~TestScreen() override = default;

    void Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void HandleInput(StateMachine& machine,
                     ncpp::NotCurses& nc,
                     ncpp::Plane& stdplane,
                     uint32_t input,
                     const ncinput& details) override;

private:
    enum class Focus {
        Files,
        Jobs
    };

    class FileSubframe : public Subframe {
    public:
        explicit FileSubframe(bool is_left);

        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        void RefreshListing() { browser_.Load(browser_.CurrentPath()); }
        const FileBrowser::Entry* CurrentEntry() const;
        std::filesystem::path CurrentPath() const { return browser_.CurrentPath(); }

    protected:
        void ComputeGeometry(unsigned parent_rows,
                             unsigned parent_cols,
                             int& y,
                             int& x,
                             int& rows,
                             int& cols) override;
        void DrawContents() override;
        void HandleInput(uint32_t input, const ncinput& details) override;

    private:
        void DrawList();

        FileBrowser browser_;
        int scroll_offset_ = 0;
        bool is_left_;
        std::size_t last_selected_index_ = 0;
        int horizontal_offset_ = 0;
    };

    class JobSubframe : public Subframe {
    public:
        JobSubframe(bool is_left, std::vector<std::string>& jobs);

        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        std::string RemoveSelected();

    protected:
        void ComputeGeometry(unsigned parent_rows,
                             unsigned parent_cols,
                             int& y,
                             int& x,
                             int& rows,
                             int& cols) override;
        void DrawContents() override;
        void HandleInput(uint32_t input, const ncinput& details) override;

    private:
        void DrawList();

        std::vector<std::string>* jobs_;
        int selected_index_ = 0;
        int scroll_offset_ = 0;
        bool is_left_;
        int horizontal_offset_ = 0;
    };

    std::vector<std::string> jobs_;
    Focus focus_ = Focus::Files;
    FileSubframe file_subframe_;
    JobSubframe job_subframe_;
};

#endif // TUI_TESTSCREEN_HPP

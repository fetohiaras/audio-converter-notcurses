#ifndef TUI_TESTSCREEN_HPP
#define TUI_TESTSCREEN_HPP

#include <memory>
#include <vector>

#include "tui/BaseScreen.hpp"
#include "tui/Subframe.hpp"
#include "tui/FileBrowser.hpp"
#include "tui/Config.hpp"

// Minimal test screen: just a framed title for layout experiments.
class TestScreen : public BaseScreen {
public:
    explicit TestScreen(ConverterConfig& config, bool& config_changed);
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
        Commands,
        Files,
        Jobs,
        Config,
        Status
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

    class ConfigSubframe : public Subframe {
    public:
        ConfigSubframe(bool is_left, ConverterConfig& config, bool& config_changed);
        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }

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
        enum class Mode { Submenus, Options, EditBool, EditValue };

        struct Option {
            std::string key;
            std::string label;
            enum class Type { Bool, Int, String } type;
        };

        void DrawSubmenus(const ContentArea& area);
        void DrawOptions(const ContentArea& area);
        void DrawEditLine(const ContentArea& area);
        void EnterOptions();
        void EnterSubmenus();
        void BeginEditBool();
        void BeginEditValue();
        void CommitBool();
        void CommitValue();
        void ResetEditLine();

        ConverterConfig& config_;
        bool& config_changed_;
        Mode mode_ = Mode::Submenus;
        int submenu_index_ = 0;
        int option_index_ = 0;
        int scroll_offset_ = 0;
        int bool_choice_ = 0;
        std::string edit_buffer_;

        std::vector<Option> current_options_;
        const std::vector<std::string> submenu_titles_{"General options", "MP3 converter", "Opus converter"};
    };

    class SystemStatusSubframe : public Subframe {
    public:
        SystemStatusSubframe(bool is_left);
        void Tick();

    protected:
        void ComputeGeometry(unsigned parent_rows,
                             unsigned parent_cols,
                             int& y,
                             int& x,
                             int& rows,
                             int& cols) override;
        void DrawContents() override;

    private:
        double progress_ = 0.0;
        double fill_speed_ = 0.003; // columns per frame
    };

    class CommandSubframe : public Subframe {
    public:
        CommandSubframe();
        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }

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
        void DrawOptions(const ContentArea& area);
        void DrawFeedback(const ContentArea& area);

        std::vector<std::string> options_{"Start", "Stop", "Exit"};
        int selected_index_ = 0;
        std::string feedback_ = "Ready";
    };

    std::vector<std::string> jobs_;
    Focus focus_ = Focus::Commands;
    ConverterConfig& config_;
    bool& config_changed_;
    FileSubframe file_subframe_;
    JobSubframe job_subframe_;
    ConfigSubframe config_subframe_;
    SystemStatusSubframe status_subframe_;
    CommandSubframe command_subframe_;
};

#endif // TUI_TESTSCREEN_HPP

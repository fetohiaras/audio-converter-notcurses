#ifndef TUI_TESTSCREEN_HPP
#define TUI_TESTSCREEN_HPP

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

#include "tui/BaseScreen.hpp"
#include "tui/Subframe.hpp"
#include "tui/FileBrowser.hpp"
#include "tui/Config.hpp"
#include "converter/MP3ToOpusConverter.hpp"

// Minimal test screen: just a framed title for layout experiments.
class TestScreen : public BaseScreen {
public:
    explicit TestScreen(ConverterConfig& config, bool& config_changed);
    ~TestScreen() override;

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
        JobConfig
    };

    class FileSubframe : public Subframe {
    public:
        explicit FileSubframe(bool is_left);

        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        void RefreshListing() { browser_.Load(browser_.CurrentPath()); }
        const FileBrowser::Entry* CurrentEntry() const;
        std::filesystem::path CurrentPath() const { return browser_.CurrentPath(); }
        void SetFocused(bool focused) { focused_ = focused; }

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
        bool focused_ = false;
    };

    class JobSubframe : public Subframe {
    public:
        JobSubframe(bool is_left, std::vector<std::string>& jobs);

        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        std::string RemoveSelected();
        void Tick();
        void SetFocused(bool focused) { focused_ = focused; }

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
        void DrawProgressBar();

        std::vector<std::string>* jobs_;
        int selected_index_ = 0;
        int scroll_offset_ = 0;
        bool is_left_;
        int horizontal_offset_ = 0;
        bool focused_ = false;
        double progress_ = 0.0;
        double fill_speed_ = 0.003; // columns per frame
    };

    class ConfigSubframe : public Subframe {
    public:
        ConfigSubframe(bool is_left, ConverterConfig& config, bool& config_changed);
        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        void SetFocused(bool focused) { focused_ = focused; }

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
        bool focused_ = false;
    };

    class JobConfigSubframe : public Subframe {
    public:
        JobConfigSubframe(bool is_left);
        void SetFocused(bool focused) { focused_ = focused; }

    protected:
        void ComputeGeometry(unsigned parent_rows,
                             unsigned parent_cols,
                             int& y,
                             int& x,
                             int& rows,
                             int& cols) override;
        void DrawContents() override;

    private:
        bool focused_ = false;
    };

    class CommandSubframe : public Subframe {
    public:
        CommandSubframe();
        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }
        void SetFocused(bool focused) { focused_ = focused; }
        const std::string& SelectedOption() const { return options_[static_cast<std::size_t>(selected_index_)]; }
        void SetFeedback(const std::string& text);

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
        std::vector<std::string> log_;
        int log_offset_ = 0;
        bool focused_ = false;
    };

    std::vector<std::string> jobs_;
    Focus focus_ = Focus::Commands;
    ConverterConfig& config_;
    bool& config_changed_;
    FileSubframe file_subframe_;
    JobSubframe job_subframe_;
    ConfigSubframe config_subframe_;
    JobConfigSubframe job_config_subframe_;
    CommandSubframe command_subframe_;

    // Conversion worker
    std::thread worker_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> converting_{false};
    std::mutex jobs_mutex_;

    void StartConversions();
    void StopConversions();
};

#endif // TUI_TESTSCREEN_HPP

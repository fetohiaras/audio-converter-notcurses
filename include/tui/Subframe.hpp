#ifndef TUI_SUBFRAME_HPP
#define TUI_SUBFRAME_HPP

#include <memory>

#include <ncpp/Plane.hh>
#include <notcurses/notcurses.h>

// Reusable subframe that owns its own ncplane anchored to a parent.
class Subframe {
public:
    Subframe();
    virtual ~Subframe();

    // Resize/recreate the subframe plane based on the parent's current dimensions.
    void Resize(ncpp::Plane& parent, unsigned parent_rows, unsigned parent_cols);

    // Draw the frame contents. Assumes Resize was called this frame.
    void Draw();

    // Optional input handler for focused subframes.
    virtual void HandleInput(uint32_t input, const ncinput& details);

protected:
    // Derived classes describe placement relative to the parent.
    virtual void ComputeGeometry(unsigned parent_rows,
                                 unsigned parent_cols,
                                 int& y,
                                 int& x,
                                 int& rows,
                                 int& cols) = 0;

    // Derived classes render into plane_; Resize ensures plane_ is valid.
    virtual void DrawContents() = 0;

    std::unique_ptr<ncpp::Plane> plane_;
    unsigned cached_rows_;
    unsigned cached_cols_;

    struct ContentArea {
        int top;
        int left;
        int height;
        int width;
    };

    // Compute an inner content area after applying padding and ensuring min size.
    ContentArea ContentBox(int pad_top, int pad_left, int pad_bottom, int pad_right, int min_height = 0, int min_width = 0) const;
};

#endif // TUI_SUBFRAME_HPP
#include <algorithm>

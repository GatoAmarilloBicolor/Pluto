#ifndef MOCK_SCROLL_BAR_H
#define MOCK_SCROLL_BAR_H

#include "Types.h"
#include "View.h"
#include "Rect.h"

class BScrollBar : public BView {
public:
    BScrollBar(BRect frame, const char* name, BView* target,
               float min, float max, int orient = B_HORIZONTAL,
               uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP,
               uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS)
        : BView(frame, name, resizingMode, flags) {
        (void)target; (void)min; (void)max; (void)orient;
    }
    virtual ~BScrollBar() = default;
    
    virtual void SetRange(float min, float max) { (void)min; (void)max; }
    virtual void GetRange(float* min, float* max) const { if (min) *min = 0; if (max) *max = 100; }
    virtual void SetValue(float value) { (void)value; }
    virtual float Value() const { return 0; }
    virtual void SetSteps(float smallStep, float bigStep) { (void)smallStep; (void)bigStep; }
    virtual void GetSteps(float* smallStep, float* bigStep) const { if (smallStep) *smallStep = 1; if (bigStep) *bigStep = 10; }
    virtual void SetProportion(float proportion) { (void)proportion; }
    virtual float Proportion() const { return 1.0f; }
};

#endif
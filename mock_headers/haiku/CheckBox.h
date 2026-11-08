// Minimal CheckBox mock for cross-compilation
#ifndef _CHECK_BOX_H
#define _CHECK_BOX_H

#include "Control.h"

class BCheckBox : public BControl {
public:
    BCheckBox(const char* name, const char* label, BMessage* message = nullptr);
    virtual ~BCheckBox();
    virtual void SetValue(int32 value);
    virtual int32 Value() const;
};

#endif // _CHECK_BOX_H
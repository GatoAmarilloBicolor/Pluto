#ifndef MOCK_MESSAGE_RUNNER_H
#define MOCK_MESSAGE_RUNNER_H

#include "Handler.h"
#include "Message.h"

class BMessageRunner {
public:
    BMessageRunner(BHandler* target, BMessage* message, bigtime_t interval, int32 count = 1);
    BMessageRunner(BMessenger target, BMessage* message, bigtime_t interval, int32 count = 1);
    virtual ~BMessageRunner();

    void SetInterval(bigtime_t interval);
    bigtime_t Interval() const;
    void SetCount(int32 count);
    int32 Count() const;
    status_t InitCheck() const;

private:
    BMessageRunner(const BMessageRunner&);
    BMessageRunner& operator=(const BMessageRunner&);
};

#endif
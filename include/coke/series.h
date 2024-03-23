#ifndef COKE_SERIES_H
#define COKE_SERIES_H

#include "coke/detail/awaiter_base.h"
#include "workflow/Workflow.h"

namespace coke {

class SeriesAwaiter : public BasicAwaiter<SeriesWork *> {
private:
    SeriesAwaiter();

    friend SeriesAwaiter current_series();
    friend SeriesAwaiter empty();
};

using EmptyAwaiter = SeriesAwaiter;

[[nodiscard]]
inline SeriesAwaiter current_series() {
    return SeriesAwaiter();
}

[[nodiscard]]
inline EmptyAwaiter empty() {
    return EmptyAwaiter();
}

} // namespace coke

#endif // COKE_SERIES_H

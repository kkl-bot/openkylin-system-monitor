#include "domain/process_types.h"

namespace monitor {

bool operator==(const ProcessIdentity &left, const ProcessIdentity &right)
{
    return left.pid == right.pid && left.startTimeTicks == right.startTimeTicks && left.uid == right.uid;
}

bool operator!=(const ProcessIdentity &left, const ProcessIdentity &right)
{
    return !(left == right);
}

} // namespace monitor

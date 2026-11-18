#pragma once

#include "domain/process_types.h"

namespace monitor {

class ProcessControl final {
public:
    // Empty means the signal was delivered; exit is confirmed by a later process snapshot.
    static QString sendSignal(const ProcessIdentity &identity, bool force = false);
};

} // namespace monitor

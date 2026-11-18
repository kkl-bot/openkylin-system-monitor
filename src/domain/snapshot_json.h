#pragma once

#include "domain/metrics.h"
#include <QJsonDocument>

namespace monitor {
QJsonDocument snapshotJson(const Snapshot &snapshot, bool paused);
} // namespace monitor

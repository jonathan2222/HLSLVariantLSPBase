#pragma once

#define WriteLocker std::unique_lock<std::shared_mutex>
#define ReadLocker std::shared_lock<std::shared_mutex>

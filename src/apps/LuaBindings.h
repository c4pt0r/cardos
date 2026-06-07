#pragma once
#include <memory>

#include "../lua/Value.h"

namespace cardos {

// Platform-service bindings shared by ScriptApp and LuaReplApp: log,
// millis, fs_read/fs_write/fs_list, http_get, record_start/record_stop,
// level. Drawing and redraw stay in ScriptApp — they only make sense in
// a script's render context.
void installPlatformBindings(const std::shared_ptr<minlua::Table>& tbl);

}  // namespace cardos

# File Explorer — design

2026-06-06. Status: approved.

A built-in app to browse `/flash` and `/sd` and delete files. Minimal by
design: no rename/copy/move, no file preview, no recursive delete.

## Approach

Native C++ app (`src/apps/FileExplorerApp.{h,cpp}`), registered in the
Launcher as **Files**. Reuses existing pieces wholesale:

- `cardos::fs::list / remove / sdAvailable` — all storage access
- `MenuList` — directory listing (same pattern as RecorderApp)
- `ConfirmDialog` — delete confirmation
- `cardos::fs::parentPath` — new pure helper in `FsPath.h` (unit-tested
  natively alongside `splitPath`)

## UI / navigation

- Root screen lists the mounts: `/flash`, and `/sd` (annotated
  "no card" and not enterable when absent).
- Inside a directory each entry is a `MenuItem`: dirs first (`name/`),
  then files with a right-aligned size note (`12K`); empty dirs show
  `(empty)`.
- Keys: Up/Down move, Enter descends into a dir, Esc/Left goes to the
  parent (Esc at root exits the app), Backspace asks to delete.
- The status bar title shows the current path (root shows "Files").

## Delete

- Backspace on a file or dir opens `ConfirmDialog` ("Delete <name>?").
- Files: `fs::remove`. Dirs: only when empty (`fs::list` returns none);
  otherwise a transient "dir not empty" notice. No recursion on purpose.
- After a delete the listing refreshes; selection clamps to the list.

## Errors

- A failed `remove` shows a transient "delete failed" notice.
- If the SD card disappears mid-browse, `list()` comes back empty —
  rendering survives, and Esc walks back to the root as usual.

## Testing

- `parentPath()` unit tests in `test/test_native` (native env).
- UI exercised on-device (browse both mounts, delete file, refuse
  non-empty dir, delete empty dir).

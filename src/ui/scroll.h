#pragma once

// Index of the first visible row of a scrolling list. Keeps the selected
// row centered where possible, clamped to the list bounds. Pure header.
inline int scrollFirstVisible(int selected, int count, int visible) {
  if (count <= visible) return 0;
  int first = selected - visible / 2;
  if (first < 0) first = 0;
  if (first > count - visible) first = count - visible;
  return first;
}

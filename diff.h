#ifndef diff_hpp
#define diff_hpp

#include <stdio.h>
#include <vector>
#include <algorithm>

namespace buffer {

  enum DiffType { INSERT, DELETE, SUBSTITUTE, ALL };
  template <typename T>
  struct Diff {
  public:
    DiffType type;    // Whether this is a substitution, a deletion or an insertion.
    size_t index;     // The index that the diff is targetting.
    T value;          // The new value associated to this operation.
  };

  // computes the Levenshtein distance between the two vectors passed as argument.
  template <typename T>
  std::vector<Diff<T>> diff(const std::vector<T> &x,
                            const std::vector<T> &y,
                            const std::function<bool (T, T)> compare = 0) {

    // creates the memoization table.
    const size_t m = x.size();
    const size_t n = y.size();
    std::vector<std::vector<int>> table(m+1, std::vector<int>(n+1, 0));

    // source prefixes can be transformed into empty string by
    // dropping all characters.
    for (auto i = 1; i < m+1; i++) table[i][0] = i;

    // target prefixes can be reached from empty source prefix
    // by inserting every character.
    for (auto j = 1; j < n+1; j++) table[0][j] = j;

    // populate the table.
    for (auto j = 1; j < n+1; j++) {
      for (auto i = 1; i < m+1; i++) {
        auto x_val = x[i-1], y_val = y[j-1];
        if ((compare && (compare(x_val, y_val))) || (!compare && x[i-1] == y[j-1]))
          table[i][j] = table[i-1][j-1];
        else {
          auto insertion = table[i][j-1];
          auto deletion = table[i-1][j];
          auto substitution = table[i-1][j-1];
          table[i][j] = std::min(deletion, std::min(insertion, substitution)) + 1;
        }
      }
    }

    // backtrack the changes.
    int i = (int)m;
    int j = (int)n;
    std::vector<Diff<T>> list;
    while (i > 0 || j > 0) {

      // sanity check.
      if (i < 0 || j < 0) break;

      // computes the likelihood of the ops.
      auto insertion = j > 0 ? table[i][j-1] : INT_MAX;
      auto deletion  = i > 0 ? table[i-1][j] : INT_MAX;
      auto diagonal  = (i > 0 && j > 0) ? table[i-1][j-1] : INT_MAX;

      // creates the operations.
      if (insertion < diagonal && insertion < deletion) {
        list.push_back(Diff<T>{INSERT, static_cast<size_t>(i), y[j-1]});
        --j;
      } else if (deletion < diagonal && deletion < insertion) {
        list.push_back(Diff<T>{DELETE, static_cast<size_t>(i-1), x[i-1]});
        --i;
      } else if (diagonal == table[i][j]) {
        --i;
        --j;
      } else {
        list.push_back(Diff<T>{SUBSTITUTE, static_cast<size_t>(i-1), y[j-1]});
        --i;
        --j;
      }
    }
    return list;
  }
}

#endif

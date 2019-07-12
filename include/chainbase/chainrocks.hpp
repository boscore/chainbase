// clang++ -g -Wall -Wextra -std=c++17 -o prog rocks2.cpp -lboost_system; ./prog

/// [ ] TODO: finish writing sufficient comments
/// [X] TODO: convert tests to use boost test framework
/// [ ] TODO: why won't `erase` erase the actualy key with the value as well?
/// [ ] TODO: revisit all comments to ensure accuracy.
/// [ ] TODO: revisit all branch to appropriately maximize branch prediction speed.
/// [ ] TODO: revisit tests to keep them logically consistent.
/// [ ] TODO: figure out why the key itself is not being erased and change respective tests.
/// [ ] TODO: integrate `std::optional`.

#pragma once

#include <boost/core/demangle.hpp>
#include <boost/throw_exception.hpp>

#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

/// There are a few things to keep in mind when reviewing the
/// implementation of the undo functionality in chainbase. There
/// are a myriad of terms thrown around that might seem arbitrary,
/// but in fact they are vital to understand. The first of which
/// is the idea of `undo`. This is the core idea around chainbase.

/// |---------------------The Undo Deque--------------------|
///
/// |-------------------------------------------------------|
/// |       |       |       |       |       |       |       |
/// |       |       |       |       |       |       |       |
/// |-------------------------------------------------------|
///
/// TODO: Continue with a more detailed ASCII drawing later.

/// Holds the current undo state of a particular session.
/// For exmple: whenever `start_undo_session` gets called
/// and set to true, a new `undo_state` object is created
/// for that particular session. In turn, whenever the
/// current `_state` is modified, these changes get recorded
/// in the `undo_state` object. Then if a user chooses to undo
/// whatever changes they've made, the information is readily
/// available to revert back safely.
struct undo_state {
   undo_state()
   {
   }

   /// Mapping to hold any modifications made to `_state`.
   std::map<uint64_t, std::string> modified_values{};

   /// Mapping to hold any removed values from `_state`.
   std::map<uint64_t, std::string> removed_values{};

   /// Set representing new keys that have been edded to `_state`.
   std::set<uint64_t> new_keys{};
};

/// Holds multiple `undo_state` objects to keep track of the undo
/// sessions in the given program.
std::deque<undo_state> _stack{};

/// The current state of the system. This is where the real meat
/// of the program lies.
std::map<uint64_t, std::string> _state{};

/// Variable to determine if an undo session is currently active.
bool _enabled{false};

/// Start an undo session. This effectively creates a checkpoint in
/// time where any values that have modified the current system state
/// could be thrown away if `undo` is called during the session or
/// if the session is pushed and then later acted upon by the `undo`
/// function.
void start_undo_session() {
   _stack.emplace_back(undo_state{});
   _enabled = true;
}

// /// Update the mapping `new_keys` to make the current undo session
// /// aware that a new value has been added to the system state.
// void on_create(const uint64_t& key) {
//    if (!_enabled) {
//       return;
//    }
//    auto& head = _stack.back();
//    head.new_keys.insert(key);
// }

/// Stuff.
void on_create(const uint64_t& key) {
   if (!_enabled) {
      return;
   }

   auto& head{_stack.back()};
   head.new_keys.insert(key);
}

/// Add a new value the current system state or modifies a currently
/// existing value.
void create(const uint64_t& key, const std::string& value) {
   on_create(key);
   _state[key] = value;
}

/// Update the mapping `modified_values` to make the current undo session
/// aware that a new value has been modified in the system state.
void on_put(const uint64_t& key) {
   if (!_enabled) {
      return;
   }

   auto& head = _stack.back();

   if (head.new_keys.find(key) != head.new_keys.end()) {
      return;
   }

   if (head.modified_values.find(key) != head.modified_values.end()) {
      return;
   }

   head.modified_values[key] = _state[key];
}

/// Add a new value the current system state or modifies a currently
/// existing value.
void put(const uint64_t& key, const std::string& value) {
   on_put(key);
   _state[key] = value;
}

/// Update the mapping `removed_values` to make the current undo session
/// aware that a value has been removed in the system state.
void on_remove(const uint64_t& key) {
   if (!_enabled) {
      return;
   }
   else {
      auto& head = _stack.back();
      head.removed_values[key] = _state[key];
   }
}

/// Remove a value in the current system state.
void remove(const uint64_t& key) {
   on_remove(key);
   _state.erase(key);
}

/// Look for a key in the current system state. If a value is not found
/// a `nullptr` is returned; note that this function will not throw if it
/// does not find the key that it's looking for.
auto find(const uint64_t& key) -> decltype(&*_state.find(key)) {
   auto itr = _state.find(key);
   if (itr != _state.end()) {
      return &*itr;
   }
   else {
      return nullptr;
   }
}

/// Look for a key in the current system state by calling the function `find`
/// on its behalf. If a value is not found this function will throw an error.
auto get(const uint64_t& key) -> decltype(*find(key)) {
   auto ptr = find(key);
   if(!ptr) {
      std::stringstream ss;
      ss << "Key not found!\n"
         << "(" << boost::core::demangle(typeid(key).name()) << "): " << key;
      BOOST_THROW_EXCEPTION(std::out_of_range(ss.str().c_str()));
   }
   else {
      return *ptr;
   }
}

/// Effectively erase any new key value pair introduced to the system state.
void undo_new_keys(const undo_state& head) {
   for (const auto& key : head.new_keys) {
      _state.erase(key);
   }
}

/// Effectively replace any modified key value pair with its previous value
/// to the system state.
void undo_modified_values(const undo_state& head) {
   for (const auto& modified_value : head.modified_values) {
      _state[modified_value.first] = modified_value.second;
   }
}

/// Effectively reintroduce any removed key value pair from the system state
/// back into the system state.
void undo_removed_values(const undo_state& head) {
   for (const auto& removed_value : head.removed_values) {
      _state[removed_value.first] = removed_value.second;
   }
}

/// Undo any combination thereof: adding key value pairs, modifying key value pairs,
/// and removing key value pairs within a particular undo session.
void undo() {
   if(!_enabled) {
      return;
   }

   /// Note that this is taking the back of the undo deque,
   /// this is because if it were to take the front, we would be
   /// grabbing the oldest `undo_state` object, and this would
   /// defeat the purpose of having an undo history.
   const auto& head{_stack.back()};

   undo_new_keys(head);
   undo_modified_values(head);
   undo_removed_values(head);

   _stack.pop_back();

   if (_stack.empty()) {
      _enabled = false;
   }
}

/// After each `undo_state` is acted upon by a call to `undo`, it shall get popped off of the undo stack.
/// Therefore, a call to `undo_all` will continually undo and pop states off of the stack until it is
/// empty. The analagous function to this is `commit`; which pops states off of the stack, but does not
/// undo the state because the said datum has been committed to the system state.
void undo_all() {
   while (_enabled) {
      undo();
   }
}

/// All possible results from squashing all combinations of two undo sessions given one key-value pair:
/// S = state, U = undo session, K = key, V = value, put = put operation, del = delete operation
/// 1
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={()}, U'={()}                 Undo Sessions: U={()}, U'={()}
/// Squashed Sesh: U={()}                          Squashed Sesh: U={()}
/// 2
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(put,K,V)}, U'={()}          Undo Sessions: U={(put,K',V')}, U'={()}
/// Squashed Sesh: U={(put,K,V)}                   Squashed Sesh: U={(put,K',V')}
/// 3
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={()}, U'={(put,K,V)}          Undo Sessions: U={()}, U'={(put,K',V')}
/// Squashed Sesh: U={(put,K,V)}                   Squashed Sesh: U={(put,K',V')}
/// 4
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(put,K,V)}, U'={(put,K',V')} Undo Sessions: U={(put,K',V')}, U'={(put,K'',V'')}
/// Squashed Sesh: U={(put,K',V')}                 Squashed Sesh: U={(put,K'',V'')}
/// 5
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(del,K)}, U'={()}            Undo Sessions: U={(del,K)}, U'={()}
/// Squashed Sesh: IMPOSSIBLE                      Squashed Sesh: U={(del,K)}
/// 6
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={()}, U'={(del, K)}           Undo Sessions: U={()}, U'={(del, K)}
/// Squashed Sesh: IMPOSSIBLE                      Squashed Sesh: U={(del,K)}
/// 7
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(del,K)}, U'={(del, K')}     Undo Sessions: U={(del,K)}, U'={(del, K')}
/// Squashed Sesh: IMPOSSIBLE                      Squashed Sesh: IMPOSSIBLE
/// 8
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(put,K,V)}, U'={(del, K)}    Undo Sessions: U={(put,K',V')}, U'={(del, K')}
/// Squashed Sesh: U={()}                          Squashed Sesh: U={()}
/// 9
/// Initial State: S={}                            Initial State: S={(K,V)}
/// Undo Sessions: U={(del,K)}, U'={(put,K,V)}     Undo Sessions: U={(del,K)}, U'={(put,K,V)}
/// Squashed Sesh: IMPOSSIBLE                      Squashed Sesh: U={(put,K,V)}



/// Case 1:                                                             /// Case 1':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: ()                                               ///   4.  Operation 2: ()
///   5.  State 2:     S = {}, U = [{}]                                 ///   5.  State 2:     S = {(K,V)}, U = [{}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {}, U = [{},{}]                              ///   7.  State 3:     S = {(K,V)}, U = [{},{}]
///   8.  Operation 4: ()                                               ///   8.  Operation 4: ()
///   9.  State 4:     S = {}, U = [{},{}]                              ///   9.  State 4:     S = {(K,V)}, U = [{},{}]
///   10. Operation 5: (squash)                                         ///   10. Operation 5: (squash)
///   11. State 5:     S = {}, U = [{}]                                 ///   11. State 5:     S = {(K,V)}, U = [{}]
///   12. Operation 6: (undo)                                           ///   12. Operation 6: (undo)
///   13. State 6:     S = {}, U = []                                   ///   13. State 6:     S = {(K,V)}, U = []
///                                                                     ///
/// Case 2:                                                             /// Case 2':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: (put,K,V)                                        ///   4.  Operation 2: (put,K,V')
///   5.  State 2:     S = {(K,V)}, U = [{(new,K)}]                     ///   5.  State 2:     S = {(K,V')}, U = [{(update, K,V)}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {(K,V)}, U = [{(new,K)}, {}]                 ///   7.  State 3:     S = {(K,V')}, U = [{(modified, K,V)}, {}]
///   8.  Operation 4: ()                                               ///   8.  Operation 4: ()
///   9.  State 4:     S = {(K,V)}, U = [{(new,K)}, {}]                 ///   9.  State 4:     S = {(K,V')}, U = [{(modified, K,V)}, {}]
///   10. Operation 5: (squash)                                         ///   10. Operation 5: (squash)
///   11. State 5:     S = {(K,V)}, U = [{(new,K)}]                     ///   11. State 5:     S = {(K,V')}, U = [{(modified, K,V)}]
///   12. Operation 6: (undo)                                           ///   12. Operation 6: (undo)
///   13. State 6:     S = {}, U = []                                   ///   13. State 6:     S = {(K,V)}, U = []
///                                                                     ///
/// Case 3:                                                             /// Case 3':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {K,V}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {K,V}, U = [{}]
///   4.  Operation 2: ()                                               ///   4.  Operation 2: ()
///   5.  State 2:     S = {}, U = [{}]                                 ///   5.  State 2:     S = {}, U = [{}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {}, U = [{}, {}]                             ///   7.  State 3:     S = {}, U = [{}, {}]
///   8.  Operation 4: (put,K,V)                                        ///   8.  Operation 4: (put,K,V')
///   9.  State 4:     S = {(K,V)}, U = [{}, {(new, K)}]                ///   9.  State 4:     S = {(K,V')}, U = [{}, {(modified,K,V)}]
///   10. Operation 5: (squash)                                         ///   10. Operation 5: (squash)
///   11. State 5:     S = {(K,V)}, U = [{(new, K)}]                    ///   11. State 5:     S = {(K,V')}, U = [{(modified,K,V)}]
///   12. Operation 6: (undo)                                           ///   12. Operation 6: (undo)
///   13. State 6:     S = {}, U = []                                   ///   13. State 6:     S = {K,V}, U = []
///                                                                     ///
/// Case 4:                                                             /// Case 4':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {K,V}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {}, U = [{}]
///   4.  Operation 2: (put,K,V)                                        ///   4.  Operation 2: (put,K,V')
///   5.  State 2:     S = {(K,V)}, U = [{(new,K)}]                     ///   5.  State 2:     S = {(K,V')}, U = [{(modified,K,V)}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {(K,V)}, U = [{(new,K)}, {}]                 ///   7.  State 3:     S = {(K,V')}, U = [{(modified,K,V)}, {}]
///   8.  Operation 4: (put,K,V')                                       ///   8.  Operation 4: (put,K,V'')
///   9.  State 4:     S = {(K,V')}, U = [{(new,K)}, {(modified,K,V)}]  ///   9.  State 4:     S = {(K, V'')}, U = [{(modified,K,V)}, {(modified,K,V')}]
///   10. Operation 5: (squash)                                         ///   10. Operation 5: (squash)
///   11. State 5:     S = {(K,V')}, U = [{(new,K)}]                    ///   11. State 5:     S = {(K, V'')}, U = [{(modified,K,V)}]
///   12. Operation 6: (undo)                                           ///   12. Operation 6: (undo)
///   13. State 6:     S = {}, U = []                                   ///   13. State 6:     S = {K,V}, U = []
///                                                                     ///
/// Case 5:                                                             /// Case 5':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: (delete,K)                                       ///   4.  Operation 2: (delete,K)
///   5.  State 2:     IMPOSSIBLE                                       ///   5.  State 2:     S = {}, U = [{(removed,K,V)}]
///   6.  Operation 3: IMPOSSIBLE                                       ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     IMPOSSIBLE                                       ///   7.  State 3:     S = {}, U = [{(removed,K,V)}, {}]
///   8.  Operation 4: IMPOSSIBLE                                       ///   8.  Operation 4: ()
///   9.  State 4:     IMPOSSIBLE                                       ///   9.  State 4:     S = {}, U = [{(removed,K,V)}, {}]
///   10. Operation 5: IMPOSSIBLE                                       ///   10. Operation 5: (squash)
///   11. State 5:     IMPOSSIBLE                                       ///   11. State 5:     S = {}, U = [{(removed,K,V)}]
///   12. Operation 6: IMPOSSIBLE                                       ///   12. Operation 6: (undo)
///   13. State 6:     IMPOSSIBLE                                       ///   13. State 6:     S = {(K,V)}, U = []
///                                                                     ///
/// Case 6:                                                             /// Case 6':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: ()                                               ///   4.  Operation 2: ()
///   5.  State 2:     S = {}, U = [{}]                                 ///   5.  State 2:     S = {(K,V)}, U = [{}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {}, U = [{}, {}]                             ///   7.  State 3:     S = {(K,V)}, U = [{}, {}]
///   8.  Operation 4: (delete,K)                                       ///   8.  Operation 4: (delete,K)
///   9.  State 4:     IMPOSSIBLE                                       ///   9.  State 4:     S = {}, U = [{}, {(removed,K,V)}]
///   10. Operation 5: IMPOSSIBLE                                       ///   10. Operation 5: (squash)
///   11. State 5:     IMPOSSIBLE                                       ///   11. State 5:     S = {}, U = [{(removed,K,V)}]
///   12. Operation 6: IMPOSSIBLE                                       ///   12. Operation 6: (undo)
///   13. State 6:     IMPOSSIBLE                                       ///   13. State 6:     S = {(K,V)}, U = []
///                                                                     ///
///
/// Case 7:                                                             /// Case 7':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: (delete,K)                                       ///   4.  Operation 2: (delete,K)
///   5.  State 2:     IMPOSSIBLE                                       ///   5.  State 2:     S = {}, U = [{(removed,K,V)}]
///   6.  Operation 3: IMPOSSIBLE                                       ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     IMPOSSIBLE                                       ///   7.  State 3:     S = {}, U = [{(removed,K,V)}, {}]
///   8.  Operation 4: IMPOSSIBLE                                       ///   8.  Operation 4: (delete,K)
///   9.  State 4:     IMPOSSIBLE                                       ///   9.  State 4:     IMPOSSIBLE
///   10. Operation 5: IMPOSSIBLE                                       ///   10. Operation 5: IMPOSSIBLE
///   11. State 5:     IMPOSSIBLE                                       ///   11. State 5:     IMPOSSIBLE
///   12. Operation 6: IMPOSSIBLE                                       ///   12. Operation 6: IMPOSSIBLE
///   13. State 6:     IMPOSSIBLE                                       ///   13. State 6:     IMPOSSIBLE
///                                                                     ///
/// Case 8:                                                             /// Case 8:
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {K,V}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {K,V}, U = [{}]
///   4.  Operation 2: (put,K,V)                                        ///   4.  Operation 2: (put,K,V')
///   5.  State 2:     S = {(K,V)}, U = [{(new,K)}]                     ///   5.  State 2:     S = {(K,V')}, U = [{(modified,K,V)}]
///   6.  Operation 3: (start_undo_session)                             ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     S = {(K,V)}, U = [{(new, K)}, {}]                ///   7.  State 3:     S = {(K,V)}, U = [{(modified,K,V)}, {}]
///   8.  Operation 4: (del,K)                                          ///   8.  Operation 4: (del,K)
///   9.  State 4:     S = {}, U = [{(new,K)}, {(removed,K,V)}]         ///   9.  State 4:     S = {}, U = [{(modified,K,V)}, {(removed,K,V')}]
///   10. Operation 5: (squash)                                         ///   10. Operation 5: (squash)
///   11. State 5:     S = {}, U = [{}]                                 ///   11. State 5:     S = {}, U = [{modifed,K,V}]
///   12. Operation 6: (undo)                                           ///   12. Operation 6: (undo)
///   13. State 6:     S = {}, U = []                                   ///   13. State 6:     S = {(K,V)}, U = []
///                                                                     ///
/// Case 9:                                                             /// Case 9':
///   1.  State 0:     S = {}, U = []                                   ///   1.  State 0:     S = {(K,V)}, U = []
///   2.  Operation 1: (start_undo_session)                             ///   2.  Operation 1: (start_undo_session)
///   3.  State 1:     S = {}, U = [{}]                                 ///   3.  State 1:     S = {(K,V)}, U = [{}]
///   4.  Operation 2: (delete,K)                                       ///   4.  Operation 2: (delete,K)
///   5.  State 2:     IMPOSSIBLE                                       ///   5.  State 2:     S = {}, U = [{(removed,K,V)}]
///   6.  Operation 3: IMPOSSIBLE                                       ///   6.  Operation 3: (start_undo_session)
///   7.  State 3:     IMPOSSIBLE                                       ///   7.  State 3:     S = {}, U = [{(removed,K,V)}, {}]
///   8.  Operation 4: IMPOSSIBLE                                       ///   8.  Operation 4: (put,K,V')
///   9.  State 4:     IMPOSSIBLE                                       ///   9.  State 4:     S = {(K,V')}, U = [{(removed,K,V)}, {(new,K)}]
///   10. Operation 5: IMPOSSIBLE                                       ///   10. Operation 5: (squash)
///   11. State 5:     IMPOSSIBLE                                       ///   11. State 5:     S = {(K,V')}, U = [{(modifed,K,V)}]
///   12. Operation 6: IMPOSSIBLE                                       ///   12. Operation 6: (undo)
///   13. State 6:     IMPOSSIBLE                                       ///   13. State 6:     S = {(K,V)}, U = []

/// Effectively squash new keys two individual sessions together.
void squash_new_keys(const undo_state& head, undo_state& head_minus_one) {
   for (const auto& key : head.new_keys) {
      head_minus_one.new_keys.insert(key);
   }
}

/// Effectively squash modified values from two individual sessions together.
void squash_modified_values(const undo_state& head, undo_state& head_minus_one) {
   for (const auto& value : head.modified_values) {
      if (head_minus_one.new_keys.find(value.first) != head_minus_one.new_keys.end()) {
         continue;
      }
      if (head_minus_one.modified_values.find(value.first) != head_minus_one.modified_values.end()) {
         continue;
      }
      assert(head_minus_one.removed_values.find(value.first) == head_minus_one.removed_values.end());
      head_minus_one.modified_values[value.first] = value.second;
   }
}

/// Effectively squash removed values from two individual sessions together.
void squash_removed_values(const undo_state& head, undo_state& head_minus_one) {
   for (const auto& value : head.removed_values) {
      if (head_minus_one.new_keys.find(value.first) != head_minus_one.new_keys.end()) {
         head_minus_one.new_keys.erase(value.first);
         continue;
      }
      auto iter{head_minus_one.modified_values.find(value.first)};
      if (iter != head_minus_one.modified_values.end()) {
         head_minus_one.removed_values[iter->first] = iter->second;
         head_minus_one.modified_values.erase(value.first);
         continue;
      }
      assert(head_minus_one.removed_values.find(value.first) == head_minus_one.removed_values.end());
      head_minus_one.removed_values[value.first] = value.second;
   }
}

/// Sqaushing is the act of taking the first two recent `undo_state`s and combining them into one.
void squash() {
   if(!_enabled) {
      return;
   }

   if(_stack.size() == 1) {
      _stack.pop_front();
      return;
   }

   auto& head = _stack.back();
   auto& head_minus_one = _stack[_stack.size()-2];

   squash_new_keys(head, head_minus_one);
   squash_modified_values(head, head_minus_one);
   squash_removed_values(head, head_minus_one);

   _stack.pop_back();
}

/// Commit all `undo_state`s to the system state by effectively not acting upon them in
/// the context of an undo session.
void commit() {
   while (_stack.size()) {
      _stack.pop_front();
   }
}

/// Helper function for tests.
/// TODO: get rid of later.
void print_state() {
   std::cout << "_state: ";
   for (const auto& value : _state) {
      std::cout << value.first << value.second << ' ';
   }
   std::cout << '\n';
}

/// Helper function for tests.
/// TODO: get rid of later.
void print_keys() {
   for (const auto& undo_state_obj : _stack) {
      std::cout << "new_keys: ";
      for (const auto& key : undo_state_obj.new_keys) {
         std::cout << key << ' ';
      }
      std::cout << '\n';
   }
}

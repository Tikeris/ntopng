/*
 *
 * (C) 2013-19 - ntop.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _GENERIC_HASH_H_
#define _GENERIC_HASH_H_

#include "ntop_includes.h"

class GenericHashEntry;

/** @defgroup MonitoringData Monitoring Data
 * This is the group that contains all classes and datastructures that handle monitoring data.
 */

/** @class GenericHash
 *  @brief Base hash class.
 *  @details Defined the base hash class for ntopng.
 *
 *  @ingroup MonitoringData
 *
 */
class GenericHash {
 protected:
  GenericHashEntry **table; /**< Entry table. It is used for maintain an update history */
  char *name;
  u_int32_t num_hashes; /**< Number of hash */
  u_int32_t current_size; /**< Current size of hash (including idle or ready-to-purge elements) */
  u_int32_t max_hash_size; /**< Max size of hash */
  RwLock **locks;
  NetworkInterface *iface; /**< Pointer of network interface for this generic hash */
  u_int last_purged_hash; /**< Index of last purged hash */
  u_int last_entry_id; /**< An uniue identifier assigned to each entry in the hash table */
  u_int purge_step;
  struct {
    u_int64_t num_idle_transitions;
    u_int64_t num_purged;
  } entry_state_transition_counters;

  vector<GenericHashEntry*> *idle_entries; /**< Vector used by the offline thread in charge of deleting hash table entries */
  vector<GenericHashEntry*> *purge_idle_idle_entries; /**< Vector prepared by the purgeIdle and periodically swapped to idle_entries */
  vector<GenericHashEntry*> *purge_idle_idle_entries_shadow; /**< Vector prepared by the purgeIdle and periodically swapped to purge_idle_idle_entries */

 public:

  /**
   * @brief A Constructor
   * @details Creating a new GenericHash.
   *
   * @param _iface Network interface pointer for the new hash.
   * @param _num_hashes Number of hashes.
   * @param _max_hash_size Max size of new hash.
   * @param _name Hash name (debug)
   * @return A new Instance of GenericHash.
   */
  GenericHash(NetworkInterface *_iface, u_int _num_hashes,
	      u_int _max_hash_size, const char *_name);

  /**
   * @brief A Destructor
   */
  virtual ~GenericHash();

  /**
   * @brief Get number of entries.
   * @details Inline method.
   *
   * @return Current size of hash.
   */
  inline u_int32_t getNumEntries() { return(current_size); };

  /**
   * @brief Tell this GenericHash about a state transition of
   * one of the entry it contains. GenericHash will use this
   * information to count total state transitions.
   *
   * @param s The new state of the transition
   */
  void notify_transition(HashEntryState s);

  /**
   * @brief Add new entry to generic hash.
   * @details If current_size < max_hash_size, this method calculate a new hash key for the new entry, add it and update the current_size value.
   *
   * @param h Pointer of new entry to add.
   * @param h whether the bucket should be locked before addin the entry to the linked list.
   * @return True if the entry has been added successfully,
   *         False otherwise.
   *
   */
  bool add(GenericHashEntry *h, bool do_lock);

  /**
   * @brief generic walker for the hash.
   * @details This method uses the walker function to compare each elements of the hash with the user data.
   *
   * @param begin_slot begin hash slot. Use 0 to walk all slots
   * @param walk_all true = walk all hash, false, walk only one (non NULL) slot
   * @param walker A pointer to the comparison function.
   * @param user_data Value to be compared with the values of hash.
   */
  bool walk(u_int32_t *begin_slot, bool walk_all,
	    bool (*walker)(GenericHashEntry *h, void *user_data, bool *entryMatched), void *user_data);

  /**
   * @brief Hash table walker used only by an offline thread in charge of deleting idle entries
   * @details This method walks the hash table and the idle_entries vector to perform cleanup operations
   *          on idle entries, before calling the destructor on them
   *
   * @param walker A pointer to the comparison function.
   * @param user_data Value to be compared with the values of hash.
   */
  void walkIdle(bool (*walker)(GenericHashEntry *h, void *user_data, bool *entryMatched), void *user_data);

  /**
   * @brief Purge idle hash entries.
   *
   * @param force_idle Forcefully marks all hash_entry_state_active entries to
   * hash_entry_state_idle
   *
   * @return Numbers of purged entry, 0 otherwise.
   */
  u_int purgeIdle(bool force_idle);

  /**
   * @brief Purge all hash entries.
   *
   */
  void cleanup();

  /**
   * @brief Return the network interface instance associated with the hash.
   * @details Inline method.
   *
   * @return Pointer of network interface instance.
   */
  inline NetworkInterface* getInterface() { return(iface); };

  /**
   * @brief Check whether the hash has empty space
   *
   * @return true if there is space left, or false if the hash is full
   */
  inline bool hasEmptyRoom() { return((current_size < max_hash_size) ? true : false); };

  /**
   * @brief Populates a lua table with hash table stats, including
   * the state transitions
   *
   * @param vm A lua VM
   *
   * @return Current size of hash.
   */
  void lua(lua_State* vm);

};

#endif /* _GENERIC_HASH_H_ */

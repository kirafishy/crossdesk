/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _GALLERY_WORKSPACE_H_
#define _GALLERY_WORKSPACE_H_

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "gallery_storage.h"

namespace crossdesk::gallery {
struct Member {
  std::string id;
  std::string group;
};

// Layout contains no credentials and never reconnects devices when loaded.
struct Workspace {
  // Group assignments are shared by history and the wall. Wall enrollment is
  // independent, so assigning a group alone must never add a device to it.
  std::vector<Member> members;
  std::vector<std::string> groups;
  std::set<std::string> selected;
  int page = 0;
  int capacity = 6;

  static std::string NormalizeId(const std::string &input) {
    std::string result;
    for (size_t i = 0; i < input.size();) {
      const auto ch = static_cast<unsigned char>(input[i]);
      if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
        ++i;
        continue;
      }
      if (i + 2 < input.size() && ch == 0xEF &&
          static_cast<unsigned char>(input[i + 1]) == 0xBC &&
          static_cast<unsigned char>(input[i + 2]) >= 0x90 &&
          static_cast<unsigned char>(input[i + 2]) <= 0x99) {
        result += char('0' + static_cast<unsigned char>(input[i + 2]) - 0x90);
        i += 3;
      } else if (input.compare(i, 3, "\xe3\x80\x80") == 0)
        i += 3;
      else
        result += input[i++];
    }
    return result;
  }
  static bool ValidId(const std::string &id) {
    return id.size() == 9 &&
           std::all_of(id.begin(), id.end(),
                       [](unsigned char c) { return c >= '0' && c <= '9'; });
  }
  bool Add(const std::string &id, const std::string &group = {}) {
    if (!ValidId(id))
      return false;
    const bool joined = wall_ids_.insert(id).second;
    // A device grouped earlier in history joins at the end when explicitly
    // added, while retaining its existing group assignments.
    if (joined)
      std::stable_partition(members.begin(), members.end(),
                            [&](const auto &m) { return m.id != id; });
    if (group.empty() && HasAssignments(id))
      return joined;
    return AddAssignment(id, group) || joined;
  }
  std::vector<std::string> Ids(const std::string &group = {}) const {
    std::vector<std::string> ids;
    std::set<std::string> seen;
    for (const auto &m : members)
      if (wall_ids_.count(m.id) && (group.empty() || m.group == group) &&
          seen.insert(m.id).second)
        ids.push_back(m.id);
    return ids;
  }
  // History filtering uses group assignments, independently of wall enrollment.
  // nullopt means all devices; an empty name means no named group.
  bool MatchesGroup(const std::string &id,
                    const std::optional<std::string> &group) const {
    if (!group)
      return true;
    if (group->empty())
      return std::none_of(members.begin(), members.end(), [&](const auto &m) {
        return m.id == id && !m.group.empty();
      });
    return std::any_of(members.begin(), members.end(), [&](const auto &m) {
      return m.id == id && m.group == *group;
    });
  }
  size_t AddMany(const std::vector<std::string> &ids,
                 const std::string &group = {}) {
    size_t added = 0;
    for (const auto &id : ids) {
      // Without a target group, an existing membership already puts the device
      // on the wall. An explicit group may reuse that same device/session.
      if (group.empty() && wall_ids_.count(id))
        continue;
      if (Add(id, group))
        ++added;
    }
    return added;
  }
  // Both editors replace the same group assignments without changing wall
  // enrollment. Keep the position of devices already on the wall untouched.
  bool SetGroups(const std::string &id, const std::vector<std::string> &names) {
    if (!ValidId(id))
      return false;
    std::vector<Member> replacements;
    std::set<std::string> seen;
    for (const auto &name : names)
      if (!name.empty() && seen.insert(name).second)
        replacements.push_back({id, name});
    if (replacements.empty())
      replacements.push_back({id, ""});
    std::vector<Member> updated;
    bool inserted = false;
    for (const auto &member : members) {
      if (member.id != id) {
        updated.push_back(member);
      } else if (!inserted) {
        updated.insert(updated.end(), replacements.begin(), replacements.end());
        inserted = true;
      }
    }
    if (!inserted)
      updated.insert(updated.end(), replacements.begin(), replacements.end());
    members = std::move(updated);
    for (const auto &member : replacements)
      if (!member.group.empty() &&
          std::find(groups.begin(), groups.end(), member.group) == groups.end())
        groups.push_back(member.group);
    PruneGroups();
    return true;
  }
  void Remove(const std::string &id, const std::string &group = {}) {
    if (group.empty()) {
      // Removing from the wall leaves history's group assignments intact.
      wall_ids_.erase(id);
    } else if (MatchesGroup(id, group)) {
      // Removing the last named group leaves an ungrouped wall member.
      // Reuse the editor path so enrollment and card order stay unchanged.
      std::vector<std::string> remaining;
      for (const auto &member : members)
        if (member.id == id && member.group != group)
          remaining.push_back(member.group);
      SetGroups(id, remaining);
    }
    selected.erase(id);
  }
  void ResizePage(int new_capacity, int count) {
    const int first = page * capacity;
    capacity = std::max(1, new_capacity);
    page = std::clamp(first / capacity, 0, std::max(0, (count - 1) / capacity));
  }
  // The caller supplies every ID in the current filter, across all pages.
  void ToggleSelectAll(const std::vector<std::string> &ids) {
    std::set<std::string> targets(ids.begin(), ids.end());
    if (selected == targets)
      selected.clear();
    else
      selected = std::move(targets);
  }
  bool Load(const std::filesystem::path &path) {
    try {
      if (!std::filesystem::exists(path))
        return true;
      std::ifstream file(path);
      const auto doc = nlohmann::json::parse(file);
      const int version = doc.at("version").get<int>();
      if ((version != 1 && version != 2) || !doc.at("members").is_array())
        return false;
      if (version == 2 && !doc.at("wall_ids").is_array())
        return false;
      Workspace loaded;
      for (const auto &member : doc.at("members")) {
        const auto id = member.at("id").get<std::string>();
        loaded.AddAssignment(id, member.value("group", ""));
        // Every saved member in the old format was already on the wall.
        if (version == 1 && ValidId(id))
          loaded.wall_ids_.insert(id);
      }
      if (version == 2)
        for (const auto &value : doc.at("wall_ids")) {
          const auto id = value.get<std::string>();
          if (ValidId(id) && loaded.HasAssignments(id))
            loaded.wall_ids_.insert(id);
        }
      members = std::move(loaded.members);
      groups = std::move(loaded.groups);
      wall_ids_ = std::move(loaded.wall_ids_);
      return true;
    } catch (...) {
      return false;
    }
  }
  bool Save(const std::filesystem::path &path) const {
    try {
      nlohmann::json doc{{"version", 2},
                         {"members", nlohmann::json::array()},
                         {"wall_ids", wall_ids_}};
      for (const auto &m : members)
        doc["members"].push_back({{"id", m.id}, {"group", m.group}});
      return WriteAtomically(path, doc.dump(2));
    } catch (...) {
      return false;
    }
  }

private:
  std::set<std::string> wall_ids_;

  bool HasAssignments(const std::string &id) const {
    return std::any_of(members.begin(), members.end(),
                       [&](const auto &m) { return m.id == id; });
  }
  bool AddAssignment(const std::string &id, const std::string &group) {
    if (!ValidId(id))
      return false;
    if (std::any_of(members.begin(), members.end(), [&](const auto &m) {
          return m.id == id && m.group == group;
        }))
      return false;
    if (!group.empty() &&
        std::find(groups.begin(), groups.end(), group) == groups.end())
      groups.push_back(group);
    members.push_back({id, group});
    return true;
  }
  void PruneGroups() {
    groups.erase(std::remove_if(groups.begin(), groups.end(),
                                [&](const auto &name) {
                                  return std::none_of(members.begin(),
                                                      members.end(),
                                                      [&](const auto &m) {
                                                        return m.group == name;
                                                      });
                                }),
                 groups.end());
  }
};
} // namespace crossdesk::gallery

#endif

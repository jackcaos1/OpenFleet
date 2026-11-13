#pragma once
// Computer player: AIP files (plain text parameter lists of the original, AI/AIPs/*.aip) and the strategic goal rules of AI/AIPs/Scheduler_README.txt.
// GL-free and free of game objects so it can be tested on its own. What the original DLLs (`*_Medium_Script.dsl`, `*_Rules.drl`) do on top of this is not read.
#include <map>
#include <string>
#include <vector>

namespace fleet {

struct AipElement {
  std::string unit;
  int count = 1;
  int base = 0;  // "Better_Build_List_Element": which base a station is placed at (only used for stations)
};

struct Aip {
  std::map<std::string, double> params;  // `int NAME = 5;` / `double NAME = 0.5;` (names lower case)
  std::vector<AipElement> build_list;    // the_build_list
  double Get(const char* name, double fallback) const;
};

// Reads the assignments and the build list; unknown lines are ignored. Returns false when nothing was found.
bool ParseAip(const std::string& text, Aip* out);
// Adds the parameters of `over` on top of `base` (later files win, as in an #include chain); the build list of `over` replaces the list when it is not empty.
void MergeAip(Aip* base, const Aip& over);

// Walks a build list: each element is built `count` times before the next one is reached.
class BuildPlanner {
 public:
  void Reset(const std::vector<AipElement>& list);
  bool Done() const { return index_ >= list_.size(); }
  bool Peek(AipElement* out) const;
  void Advance();  // one unit of the current element is under way
  void Skip();     // drop the current element (e.g. something this client cannot build)
  size_t Index() const { return index_; }

 private:
  std::vector<AipElement> list_;
  size_t index_ = 0;
  int built_ = 0;
};

// Target count of each element when counts add up along the list: `"fbase" 1 ... "fbase" 1` means two bases once the second entry is reached.
// (ASSUMPTION about the original's reading of `units_to_build`; it makes lists work on maps that already start with some of the objects.)
std::vector<int> CumulativeTargets(const std::vector<AipElement>& list);

// ---- strategic goals (Scheduler_README.txt, steps 1 to 8) ----
enum class GoalKind { AttackBase, Attack, Defend };

struct GoalCell {
  int cx = 0, cz = 0;               // grid cell (400 units)
  float x = 0, z = 0;               // aim point (centre of the cell's enemy objects, or of the own stations for Defend)
  int enemy_stations = 0, own_stations = 0;
  float enemy_shield = 0;           // total shield strength of the enemy units in the cell
  float threat = 0;                 // threat value of the cell
  float dist_home = 0, dist_enemy = 0;
  bool perimeter = false;
};

struct Goal {
  GoalKind kind = GoalKind::Attack;
  GoalCell cell;
  double raw = 0;
  bool valid = false;
};

// Raw priority of one goal at one cell (step 3). Invalid goals have valid == false.
Goal EvaluateGoal(GoalKind kind, const GoalCell& c, const Aip& aip);

// Match utility of a unit and a goal (step 4): distance * distance_from_goal_match_modifier + raw priority.
double MatchUtility(const Goal& g, float unit_x, float unit_z, const Aip& aip);

struct SquadUnit {
  int id = 0;
  float x = 0, z = 0;
  float shield = 0;  // shield strength of the unit
};

// Steps 5 to 8: goals in descending order of their best match; units are committed nearest first until their total shield strength exceeds the enemy shield
// at the target; goals that do not reach it are rolled back. Returns for each goal (by index) the committed unit ids.
std::vector<std::vector<int>> AssignGoals(const std::vector<Goal>& goals, const std::vector<SquadUnit>& units, const Aip& aip);

}  // namespace fleet

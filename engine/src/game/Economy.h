#pragma once
// Team resources and the build queue, following the executable (docs/formats/economy.md):
//   team record 0x449340 (AddDilithium), Producer::StartNextBuild 0x45ba40, CancelBuild 0x45bc20, completion 0x45cdf0,
//   cost accessors 0x46bbf0 / 0x46bc80 / 0x46bd10 / 0x46bdd0, Starbase::Update 0x45f830 (crew income).
// GL-free and free of game objects so it can be tested on its own.
#include <string>
#include <vector>

namespace fleet {

struct TeamResources {
  float dilithium = 0, dilithium_max = 999999;  // cfgMaxDilithium
  int officers = 0, officers_max = 100;         // cfgMaxOfficers
  float crew = 0, crew_max = 99999;             // cfgMaxCrew

  // 0x449340: the sum is clamped to the maximum only (a negative amount may take the value below zero; callers check first).
  void AddDilithium(float a) { dilithium = dilithium + a > dilithium_max ? dilithium_max : dilithium + a; }
  void AddOfficers(int a) { officers = officers + a > officers_max ? officers_max : officers + a; }
  void AddCrew(float a) { crew = crew + a > crew_max ? crew_max : crew + a; }
};

// Slot kind from the lobby (slot state at +0xc4 of the player record: 1 human, 3 computer easy, 4 medium, 5 hard).
enum class AiLevel { Human = 0, Easy, Medium, Hard };

struct CostRates {
  float medium = 0.8f, hard = 0.4f;  // RTS_CFG.h MEDIUM/HARD_AI_COST_AND_SPEED_HANDICAP
};
// Factor on dilithium, officers, crew and build time of one team (0x46bbf0 and siblings). The factor only applies when the game
// type setting (+0x78) is not 2 (the lobby default 2 is "not in the list"); game_type_set is true when the player picked a type.
float CostFactor(AiLevel level, bool game_type_set, const CostRates& rates);

struct BuildCost {
  int dilithium = 0, officers = 0, crew = 0;
  float time = 0;
};
// Costs are `(int)(base * factor)` (truncated, 0x5bbb00), the time is `base * factor`.
BuildCost ScaleCost(int dilithium, int officers, int crew, float time, float factor);

enum class StartResult { Ok, NoDilithium, NoOfficers, NoCrew };  // messages 0xb, 0xd, 0xc of 0x5ab930
// Checks in the order of the executable: dilithium + 0.5 < cost, officers (only when cost > 0), crew.
StartResult CanStart(const TeamResources& r, const BuildCost& c);
void Pay(TeamResources* r, const BuildCost& c);          // start of a build: all three are taken
void RefundCancel(TeamResources* r, const BuildCost& c);  // cancel: all three come back
void ReturnOfficers(TeamResources* r, const BuildCost& c);  // completion: only the officers come back (crew becomes the new craft's crew)

// Starbase income: crew += rate * factor * dt, clamped (0x45f830). factor = 1 without a planet nearby.
void AccumulateCrew(TeamResources* r, float rate, float factor, float dt);

struct QueueItem {
  std::string odf;
  BuildCost cost;
};

// One producer (shipyard, starbase, construction ship): a queue of at most 5 items of which the first one is being built.
struct BuildQueue {
  static constexpr size_t kMax = 5;
  std::vector<QueueItem> items;
  bool active = false;
  float remaining = 0, total = 0;  // 0x7a0 / 0x7a4 of the producer object

  bool Enqueue(const QueueItem& it);  // false when five items are already queued (0x45b3a0)
  size_t Size() const { return items.size(); }

  enum class Event { None, Started, Failed, Completed, Cancelled };
  struct Result {
    Event event = Event::None;
    StartResult why = StartResult::Ok;
    QueueItem item;
  };
  // Starts the first item when idle (a failed start drops it, as 0x45b420 does), otherwise advances the build.
  Result Update(TeamResources* res, float dt);
  // 0x45bc20: only while building; removes the first item and refunds its cost.
  Result Cancel(TeamResources* res);
  float Progress() const { return active && total > 0 ? (total - remaining) / total : 0.0f; }
};

}  // namespace fleet

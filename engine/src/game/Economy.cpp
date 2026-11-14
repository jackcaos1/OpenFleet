#include "game/Economy.h"

namespace fleet {

float CostFactor(AiLevel level, bool game_type_set, const CostRates& rates) {
  if (!game_type_set) return 1.0f;
  if (level == AiLevel::Hard) return rates.hard;
  if (level == AiLevel::Medium) return rates.medium;
  return 1.0f;
}

BuildCost ScaleCost(int dilithium, int officers, int crew, float time, float factor) {
  BuildCost c;
  c.dilithium = static_cast<int>(static_cast<float>(dilithium) * factor);
  c.officers = static_cast<int>(static_cast<float>(officers) * factor);
  c.crew = static_cast<int>(static_cast<float>(crew) * factor);
  c.time = time * factor;
  return c;
}

StartResult CanStart(const TeamResources& r, const BuildCost& c) {
  if (r.dilithium + 0.5f < static_cast<float>(c.dilithium)) return StartResult::NoDilithium;
  if (c.officers > r.officers && c.officers != 0) return StartResult::NoOfficers;
  if (r.crew < static_cast<float>(c.crew)) return StartResult::NoCrew;
  return StartResult::Ok;
}

void Pay(TeamResources* r, const BuildCost& c) {
  r->AddDilithium(-static_cast<float>(c.dilithium));
  const int o = r->officers - c.officers;
  r->officers = o > r->officers_max ? r->officers_max : o;
  const float cr = r->crew - static_cast<float>(c.crew);
  r->crew = cr > r->crew_max ? r->crew_max : cr;
}

void RefundCancel(TeamResources* r, const BuildCost& c) {
  r->AddDilithium(static_cast<float>(c.dilithium));
  r->AddOfficers(c.officers);
  r->AddCrew(static_cast<float>(c.crew));
}

void ReturnOfficers(TeamResources* r, const BuildCost& c) { r->AddOfficers(c.officers); }

void AccumulateCrew(TeamResources* r, float rate, float factor, float dt) { r->AddCrew(rate * factor * dt); }

bool BuildQueue::Enqueue(const QueueItem& it) {
  if (items.size() >= kMax) return false;
  items.push_back(it);
  return true;
}

BuildQueue::Result BuildQueue::Update(TeamResources* res, float dt) {
  Result out;
  if (items.empty()) return out;
  if (!active) {
    const QueueItem& head = items.front();
    const StartResult sr = CanStart(*res, head.cost);
    if (sr != StartResult::Ok) {
      out.event = Event::Failed;
      out.why = sr;
      out.item = head;
      items.erase(items.begin());
      return out;
    }
    Pay(res, head.cost);
    active = true;
    total = remaining = head.cost.time;
    out.event = Event::Started;
    out.item = head;
    return out;
  }
  remaining -= dt;
  if (remaining < 0.0f) {  // 0x45ccb0: finished when the remaining time drops below zero
    out.event = Event::Completed;
    out.item = items.front();
    ReturnOfficers(res, out.item.cost);
    items.erase(items.begin());
    active = false;
    remaining = total = 0;
  }
  return out;
}

BuildQueue::Result BuildQueue::Cancel(TeamResources* res) {
  Result out;
  if (!active || items.empty()) return out;
  out.event = Event::Cancelled;
  out.item = items.front();
  RefundCancel(res, out.item.cost);
  items.erase(items.begin());
  active = false;
  remaining = total = 0;
  return out;
}

}  // namespace fleet

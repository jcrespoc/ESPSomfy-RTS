#ifndef MQTT_PURGER_H
#define MQTT_PURGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <vector>

enum class MQTTPurgeState : uint8_t {
  Idle,
  Subscribing,
  Collecting,
  Unsubscribing,
  Draining,
  Completed
};

typedef bool (*PurgeFilterFn)(const char* topic, const byte* payload, size_t length);
typedef void (*PurgeActionFn)(const char* topic);

struct PurgeRule {
  String topicPattern;
  PurgeFilterFn shouldPurge;
  PurgeActionFn action;
  String description;
};

namespace PurgeHelper {
  bool topicMatches(const char* topic, const char* pattern);
  int extractIdFromSubtopic(const char* topic);
}

class MQTTPurgeManager {
private:
  MQTTPurgeState state = MQTTPurgeState::Idle;
  uint32_t timer = 0;
  std::vector<PurgeRule> rules;
  std::vector<std::pair<String, PurgeActionFn>> queue;

  #ifdef PURGEMANAGER_ONERUN
  bool executedOnce = false;
  #endif

  void subscribeAll(PubSubClient& client);
  void unsubscribeAll(PubSubClient& client);

public:
  void addRule(const char* topicPattern, PurgeFilterFn shouldPurge, PurgeActionFn action = nullptr, const char* description = nullptr);
  void clearRules();
  void start();
  void loop(PubSubClient& client);
  void onMessageReceived(const char* topic, const byte* payload, size_t length);
  bool isCollecting() const { return this->state == MQTTPurgeState::Collecting; }
  bool isBusy() const { return this->state != MQTTPurgeState::Idle && this->state != MQTTPurgeState::Completed; }
  void reset();
};

extern MQTTPurgeManager mqttPurgeManager;

#endif // MQTT_PURGER_H

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

struct PurgeRule {
  String topicPattern;
  PurgeFilterFn shouldPurge;
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
  std::vector<String> queue;
  bool executedOnce = false;

  void subscribeAll(PubSubClient& client);
  void unsubscribeAll(PubSubClient& client);

public:
  void addRule(const char* topicPattern, PurgeFilterFn shouldPurge, const char* description = nullptr);
  void clearRules();
  void start();
  void loop(PubSubClient& client);
  void onMessageReceived(const char* topic, const byte* payload, size_t length);
  bool isCollecting() const { return this->state == MQTTPurgeState::Collecting; }
  bool isBusy() const { return this->state != MQTTPurgeState::Idle && this->state != MQTTPurgeState::Completed; }
  void reset();
};

extern MQTTPurgeManager mqttPurge;

#endif // MQTT_PURGER_H

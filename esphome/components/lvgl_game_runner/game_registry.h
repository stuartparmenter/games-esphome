// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "game_base.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace esphome::lvgl_game_runner {

/**
 * Game registry using Factory Method pattern.
 * Similar to FxRegistry in lvgl-canvas-fx.
 *
 * Games are registered at startup with string keys (e.g., "snake", "breakout").
 * Runtime game switching is done via make("game_name").
 *
 * Supports two registration methods:
 * 1. register_factory() - Factory functions that create instances (for future use)
 * 2. register_instance() - Pre-created instances (from Python components)
 */
class GameRegistry {
 public:
  using Factory = std::function<std::unique_ptr<GameBase>()>;

  /**
   * Register a game factory function with a string key.
   */
  static void register_factory(const std::string &key, Factory factory) { get_factories_()[key] = std::move(factory); }

  /**
   * Register a pre-created game instance (e.g., from Python component).
   * Registry does not take ownership - caller must ensure lifetime.
   */
  static void register_instance(const std::string &key, GameBase *instance) { get_instances_()[key] = instance; }

  /**
   * Get a game instance by key.
   * Returns nullptr if key is not found.
   * For registered instances, returns the stored pointer.
   * For factories, creates a new instance and caches it.
   */
  static GameBase *make(const std::string &key) {
    // First check for pre-registered instances
    auto &instances = get_instances_();
    auto inst_it = instances.find(key);
    if (inst_it != instances.end()) {
      return inst_it->second;
    }

    // Then check factories and create instance
    auto &factories = get_factories_();
    auto factory_it = factories.find(key);
    if (factory_it != factories.end()) {
      // Create and cache the instance
      auto unique_instance = factory_it->second();
      GameBase *raw_ptr = unique_instance.get();
      get_owned_instances_()[key] = std::move(unique_instance);
      return raw_ptr;
    }

    return nullptr;
  }

  /**
   * Check if a game is registered.
   */
  static bool has_game(const std::string &key) {
    return get_instances_().find(key) != get_instances_().end() || get_factories_().find(key) != get_factories_().end();
  }

 private:
  /**
   * Static singleton map for factories.
   */
  static std::map<std::string, Factory> &get_factories_() {
    static std::map<std::string, Factory> map;
    return map;
  }

  /**
   * Static singleton map for pre-created instances.
   */
  static std::map<std::string, GameBase *> &get_instances_() {
    static std::map<std::string, GameBase *> map;
    return map;
  }

  /**
   * Static singleton map for factory-created instances (owned).
   */
  static std::map<std::string, std::unique_ptr<GameBase>> &get_owned_instances_() {
    static std::map<std::string, std::unique_ptr<GameBase>> map;
    return map;
  }
};

}  // namespace esphome::lvgl_game_runner

#pragma once

#include "YamlSettings.hpp"

// glorified settings manager
class ProfileSettings {
public:
  static std::shared_ptr<YamlSettings> get();
  static std::shared_ptr<YamlSettings> getOf(const QString& name = "default");
  static QList<QString> list();
};

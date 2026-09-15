#pragma once

#include "YamlSettings.hpp"

// glorified settings manager
class ProfileSettings {
public:
  static std::shared_ptr<YamlSettings> get(const QString &name = "");
  static QList<QString> list();
};

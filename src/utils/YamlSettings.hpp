
#pragma once

#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QStandardPaths>
#include <QDir>
#include <yaml-cpp/yaml.h>

class YamlSettings {
public:
    explicit YamlSettings(const QString& fileName = "settings.yaml");
    ~YamlSettings();

    // Value Accessors
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    // Direct helpers for sequence/array handling
    void setArray(const QString& key, const QVariantList& list);
    QVariantList getArray(const QString& key) const;

    // Content lookup 2
    bool contains(const QString& key) const;

    // File Management
    bool save();
    bool load();
    QString filePath() const { return m_filePath; }

private:
    QString m_filePath;
    YAML::Node m_rootNode;

    // Conversion Utilities
    static YAML::Node qvariantToYaml(const QVariant& var);
    static QVariant yamlToQVariant(const YAML::Node& node);
    
    YAML::Node navigateToKey(const QString& key, bool createMissing = false);
    const YAML::Node navigateToKeyConst(const QString& key) const;
};


#include "YamlSettings.hpp"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
#include <filesystem>
#include <qlogging.h>

YamlSettings::YamlSettings(const QString& fileName) {
    qDebug() << "YamlSettings: constructing" << fileName;
    // Standard AppData/AppConfig path (~/.config/<AppName>/ on Linux)
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    
    m_filePath = QDir(configDir).filePath(fileName);
    qDebug() << "YamlSettings: resolved file path" << m_filePath;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(m_filePath.toStdString()).parent_path(), ec);
    if (ec) qWarning() << "YamlSettings: could not create parent directory:"
                       << QString::fromStdString(ec.message());
    load();
}

YamlSettings::~YamlSettings() {
    qDebug() << "YamlSettings: destroying and saving" << m_filePath;
    save();
}

bool YamlSettings::load() {
    QFile file(m_filePath);
    if (!file.exists()) {
        qDebug() << "YamlSettings: file does not exist, creating empty map:" << m_filePath;
        m_rootNode = YAML::Node(YAML::NodeType::Map);
        return true;
    }

    try {
        m_rootNode = YAML::LoadFile(m_filePath.toStdString());
        qDebug() << "YamlSettings: loaded" << m_filePath;
        return true;
    } catch (const YAML::Exception& e) {
        qWarning() << "Failed to parse YAML file:" << e.what();
        m_rootNode = YAML::Node(YAML::NodeType::Map);
        return false;
    }
}

bool YamlSettings::save() {
    qDebug() << "YamlSettings: saving" << m_filePath;
    try {
        QFile file(m_filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "YamlSettings: could not open for writing:" << m_filePath
                       << "error" << file.errorString();
            return false;
        }

        YAML::Emitter out;
        out << m_rootNode;

        QTextStream stream(&file);
        stream << out.c_str() << "\n";
        qDebug() << "YamlSettings: save complete:" << m_filePath;
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to write YAML file:" << e.what();
        return false;
    }
}

YAML::Node navigateToKeyInner(YAML::Node current, QStringList& remainingParts, bool createMissing) {
  std::string part = remainingParts.takeFirst().toStdString();
  if (remainingParts.size() == 0) {
    return current[part];
  }
  if (!current[part] || !current[part].IsMap()) {
    if (createMissing) {
      current[part] = YAML::Node(YAML::NodeType::Map);
    } else {
      return YAML::Node();
    }
  }
  return navigateToKeyInner(current[part], remainingParts, createMissing);
}

YAML::Node YamlSettings::navigateToKey(const QString& key, bool createMissing) {
    QStringList parts = key.split('/', Qt::SkipEmptyParts);
    YAML::Node current = m_rootNode;
    
    return navigateToKeyInner(current, parts, createMissing);
}

const YAML::Node navigateToKeyConstInner(YAML::Node current, QStringList& remainingParts, bool createMissing) {
  std::string part = remainingParts.takeFirst().toStdString();
  if (remainingParts.size() == 0) {
    return current[part];
  }
  if (!current[part] || !current[part].IsMap()) {
    return YAML::Node();
  }
  return navigateToKeyConstInner(current[part], remainingParts, createMissing);
}
const YAML::Node YamlSettings::navigateToKeyConst(const QString& key) const {
    QStringList parts = key.split('/', Qt::SkipEmptyParts);
    YAML::Node current = m_rootNode;

    return navigateToKeyConstInner(current, parts, false);
}

void YamlSettings::setValue(const QString& key, const QVariant& value) {
    qDebug() << "YamlSettings: set" << key << "=" << value << "in" << m_filePath;
    YAML::Node node = navigateToKey(key, true);

    node = qvariantToYaml(value);
}

QVariant YamlSettings::value(const QString& key, const QVariant& defaultValue) const {
    YAML::Node node = navigateToKeyConst(key);
    if (!node || node.IsNull()) {
        qDebug() << "YamlSettings: missing" << key << "in" << m_filePath
                 << "using default" << defaultValue;
        return defaultValue;
    }
    QVariant result = yamlToQVariant(node);
    qDebug() << "YamlSettings: read" << key << "=" << result << "from" << m_filePath;
    return result;
}

void YamlSettings::setArray(const QString& key, const QVariantList& list) {
    setValue(key, list);
}

QVariantList YamlSettings::getArray(const QString& key) const {
    return value(key, QVariantList()).toList();
}

bool YamlSettings::contains(const QString& key) const {
    YAML::Node node = navigateToKeyConst(key);
    return node && !node.IsNull();
}

// Recursive QVariant <-> YAML Converters
YAML::Node YamlSettings::qvariantToYaml(const QVariant& var) {
    YAML::Node node;

    if (var.typeId() == QMetaType::QVariantList || var.typeId() == QMetaType::QStringList) {
        node = YAML::Node(YAML::NodeType::Sequence);
        for (const QVariant& item : var.toList()) {
            node.push_back(qvariantToYaml(item));
        }
    } else if (var.typeId() == QMetaType::QVariantMap) {
        node = YAML::Node(YAML::NodeType::Map);
        QVariantMap map = var.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            node[it.key().toStdString()] = qvariantToYaml(it.value());
        }
    } else {
        node = var.toString().toStdString();
    }

    return node;
}

QVariant YamlSettings::yamlToQVariant(const YAML::Node& node) {
    if (node.IsSequence()) {
        QVariantList list;
        for (const auto& child : node) {
            list.append(yamlToQVariant(child));
        }
        return list;
    } else if (node.IsMap()) {
        QVariantMap map;
        for (auto it = node.begin(); it != node.end(); ++it) {
            QString key = QString::fromStdString(it->first.as<std::string>());
            map.insert(key, yamlToQVariant(it->second));
        }
        return map;
    } else if (node.IsScalar()) {
        return QString::fromStdString(node.as<std::string>());
    }

    return QVariant();
}

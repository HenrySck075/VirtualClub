#include "utils.hpp"
#include "consts.hpp"
#include "utils/ProfileSettings.hpp"
#include <QFileDialog>

#include <random>
#include <sstream>

int findChildWidgetIndex(QLayout* layout, QWidget* child) {
    if (!layout) return -1;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        // Check if the item is a QWidget
        if (QWidget* widget = item->widget()) {
            if (widget == child) {
                return i;
            }
        }
        
        /*
        // Optional: Recurse into sub-layouts
        if (QLayout* childLayout = item->layout()) {
            if (QWidget* found = findChildWidgetBy(childLayout, predicate)) {
                return found;
            }
        }
        */
    }

    return -1;
}
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate) {
    if (!layout) return nullptr;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        // Check if the item is a QWidget
        if (QWidget* widget = item->widget()) {
            if (predicate(widget)) {
                return widget;
            }
        }
        
        /*
        // Optional: Recurse into sub-layouts
        if (QLayout* childLayout = item->layout()) {
            if (QWidget* found = findChildWidgetBy(childLayout, predicate)) {
                return found;
            }
        }
        */
    }

    return nullptr;
}

bool askForBasePathChange() {
  auto settings = ProfileSettings::get();
  auto directory = QFileDialog::getExistingDirectory(
      nullptr, "Select a base game directory.");
  if (directory != "") {
    settings->setValue(STK_BASEPATH, directory);
    return true;
  }
  return false;
}


std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;

    uint32_t data[4] = { dis(gen), dis(gen), dis(gen), dis(gen) };

    // Set UUID version 4 (bits 12-15 of time_hi_and_version to 0100)
    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
    // Set UUID variant (bits 6-7 of clock_seq_hi_and_reserved to 10)
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << data[0] << "-"
       << std::setw(4) << (data[1] >> 16) << "-"
       << std::setw(4) << (data[1] & 0xFFFF) << "-"
       << std::setw(4) << (data[2] >> 16) << "-"
       << std::setw(4) << (data[2] & 0xFFFF)
       << std::setw(8) << data[3];

    return ss.str();
}


#pragma once

#include <QObject>

class VFSPathResolverTests : public QObject {
  Q_OBJECT

private slots:
  void modOverridesBaseAndWriteUsesDelta();
  void baseIsFallback();
  void libUsesModOrBaseEngine();
  void launcherPatchOverridesAllLayers();
  void mountAndUnmount();
};

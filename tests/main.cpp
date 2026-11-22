#include <QCoreApplication>
#include <QtTest>

#include "ModIndexTests.hpp"
#include "VFSPathResolverTests.hpp"
#include "utils/ProfileSingleApp.hpp"

int main(int argc, char** argv) {
  ProfileSingleApp app(argc, argv);
  app.setOrganizationName("VirtualClub");
  app.setApplicationName("VirtualClubTests");
  app.setProfileId("test");
  int status = 0;

  ModIndexTests modIndexTests;
  status |= QTest::qExec(&modIndexTests, argc, argv);

  VFSPathResolverTests vfsTests;
  status |= QTest::qExec(&vfsTests, argc, argv);
  return status;
}

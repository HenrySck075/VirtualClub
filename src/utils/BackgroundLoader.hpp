#pragma once

#include <exception>
class nonexistent_path : public std::exception {
public:
  const char* what() const noexcept override {
    return "Path does not exist";
  }
};

#include <QStringList>

// the only reason im making this (and ProfileSettings) a class rather than a namespace 
// is that clangd does not try to complete namespace declarations 
// and not even auto-including headers on autocomplete for some of the functions. not sure why
// you get my idea
class BackgroundLoader {
public:
  static QStringList getPacks();
  // get a list of images in the current background pack
  static QStringList getImages();
  // get the image for single-image background, 
  // or the image corresponding to the current time for the multiple variant.
  static QString getImage();

  struct BackgroundInfo {
    enum BackgroundType {
      Single, Multiple
    };
    QString name;
    QString displayName;
    BackgroundType type;
  };
  static BackgroundInfo getBackgroundInfo();
};

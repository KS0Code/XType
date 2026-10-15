#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "apps_known.h"

static void writeFile(const QString &path, const QByteArray &content) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(content);
}

TEST_CASE("parseSeenApps reads names, most recent first", "[apps_known]") {
    const auto list = AppsKnown::parseSeenApps(
        R"({"apps":[{"name":"omawrite","last_seen":100},
                    {"name":"com.t3tools.T3Code","last_seen":300},
                    {"name":"","last_seen":999},
                    {"last_seen":5}]})");
    REQUIRE(list.size() == 2);
    CHECK(list[0].toMap()["name"].toString() == "com.t3tools.T3Code");
    CHECK(list[1].toMap()["name"].toString() == "omawrite");
    CHECK(list[1].toMap()["last_seen"].toLongLong() == 100);
}

TEST_CASE("parseSeenApps tolerates garbage", "[apps_known]") {
    CHECK(AppsKnown::parseSeenApps("not json").isEmpty());
    CHECK(AppsKnown::parseSeenApps("").isEmpty());
}

TEST_CASE("parseDesktopEntry takes unlocalised Name, Icon, StartupWMClass", "[apps_known]") {
    const auto info = AppsKnown::parseDesktopEntry(
        "[Desktop Entry]\n"
        "Name[pt_BR]=Escritor\n"
        "Name=Omawrite\n"
        "Icon=omawrite\n"
        "StartupWMClass=omawrite-bin\n"
        "[Desktop Action new]\n"
        "Name=New document\n");
    CHECK(info.name == "Omawrite");
    CHECK(info.icon == "omawrite");
    CHECK(info.wmClass == "omawrite-bin");
}

TEST_CASE("findDesktopFile matches the file name, then StartupWMClass", "[apps_known]") {
    QTemporaryDir dir; REQUIRE(dir.isValid());
    writeFile(dir.filePath("com.t3tools.T3Code.desktop"), "[Desktop Entry]\nName=T3 Code\n");
    writeFile(dir.filePath("teams.desktop"),
              "[Desktop Entry]\nName=Teams\nStartupWMClass=teams-for-linux\n");
    const QStringList dirs{dir.path()};

    CHECK(AppsKnown::findDesktopFile("com.t3tools.T3Code", dirs).endsWith("com.t3tools.T3Code.desktop"));
    CHECK(AppsKnown::findDesktopFile("com.T3TOOLS.t3code", dirs).endsWith("com.t3tools.T3Code.desktop"));
    CHECK(AppsKnown::findDesktopFile("teams-for-linux", dirs).endsWith("teams.desktop"));
    CHECK(AppsKnown::findDesktopFile("unknown", dirs).isEmpty());
}

TEST_CASE("fallbackLabel uses the last part of reverse-DNS ids", "[apps_known]") {
    CHECK(AppsKnown::fallbackLabel("com.t3tools.T3Code") == "T3Code");
    CHECK(AppsKnown::fallbackLabel("teams-for-linux") == "teams-for-linux");
    CHECK(AppsKnown::fallbackLabel("trailing.") == "trailing.");
}

TEST_CASE("appList merges seen and configured apps", "[apps_known]") {
    QStandardPaths::setTestModeEnabled(true);   // no real .desktop files
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    AppsKnown apps(tmp.filePath("active_apps.json"));

    const QVariantList seen = AppsKnown::parseSeenApps(
        R"({"apps":[{"name":"omawrite","last_seen":300},{"name":"kate","last_seen":100}]})");
    QVariantMap configured;
    configured["kate"]          = QVariantMap{{"enabled", false}};
    configured["zed.Editor"]    = QVariantMap{{"enabled", true}};
    configured["alpha.Thing"]   = QVariantMap{{"enabled", true}};

    const auto list = apps.appList(configured, seen);
    REQUIRE(list.size() == 4);
    // Seen apps first, by recency; then configured-only apps by label.
    CHECK(list[0].toMap()["name"].toString() == "omawrite");
    CHECK(list[0].toMap()["configured"].toBool() == false);
    CHECK(list[1].toMap()["name"].toString() == "kate");
    CHECK(list[1].toMap()["configured"].toBool() == true);
    CHECK(list[2].toMap()["label"].toString() == "Editor");
    CHECK(list[3].toMap()["label"].toString() == "Thing");
}

TEST_CASE("seenApps reads the engine's file", "[apps_known]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString path = tmp.filePath("active_apps.json");
    writeFile(path, R"({"apps":[{"name":"omawrite","last_seen":1}]})");
    AppsKnown apps(path);
    REQUIRE(apps.seenApps().size() == 1);
    CHECK(apps.seenApps()[0].toMap()["name"].toString() == "omawrite");
}

#include <catch2/catch_test_macros.hpp>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include "corpus_stats.h"
#include "engine_probe.h"

static void writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

static QByteArray stamp(const QDateTime &t) {
    return QByteArray::number(t.toSecsSinceEpoch()) + '\n';
}

// ── CorpusStats::count ───────────────────────────────────────────────────────

TEST_CASE("count: missing corpus yields zeros and 7 empty day buckets", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), QDateTime::currentDateTime());
    CHECK(c.sentences == 0);
    CHECK(c.words     == 0);
    CHECK(c.bytes     == 0);
    CHECK(c.wordsPerDay == QList<int>(CorpusStats::kDays, 0));
}

TEST_CASE("count: sentences, words and bytes; blank lines are not sentences", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QByteArray data = "hello there world\n\n   \nsecond  line\twith tabs\n";
    writeFile(tmp.filePath("corpus.txt"), data);

    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), QDateTime::currentDateTime());
    CHECK(c.sentences == 2);
    CHECK(c.words     == 7);
    CHECK(c.bytes     == data.size());
}

TEST_CASE("count: UTF-8 words are counted as whole words", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    writeFile(tmp.filePath("corpus.txt"), "não sei se a configuração está certa\n");

    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), QDateTime::currentDateTime());
    CHECK(c.words == 7);
}

TEST_CASE("count: words are bucketed per day from the timestamps file", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QDateTime now(QDate(2026, 10, 14), QTime(12, 0));

    writeFile(tmp.filePath("corpus.txt"),
              "one two three\n"          // today
              "four five\n"              // today
              "six\n"                    // yesterday
              "seven eight nine ten\n"   // 6 days ago (oldest bucket)
              "eleven twelve\n"          // 7 days ago (outside window)
              "thirteen\n"               // malformed timestamp
              "fourteen fifteen\n");     // no timestamp line at all
    writeFile(tmp.filePath("corpus_timestamps.txt"),
              stamp(now) + stamp(now.addSecs(-3600)) + stamp(now.addDays(-1))
              + stamp(now.addDays(-6)) + stamp(now.addDays(-7)) + "garbage\n");

    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), now);
    CHECK(c.words == 15);
    CHECK(c.wordsPerDay == QList<int>{4, 0, 0, 0, 0, 1, 5});
}

TEST_CASE("count: rotated files add to totals but not to day buckets; .tmp is ignored", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QDateTime now = QDateTime::currentDateTime();

    writeFile(tmp.filePath("corpus.txt"),     "live line here\n");
    writeFile(tmp.filePath("corpus_timestamps.txt"), stamp(now));
    writeFile(tmp.filePath("corpus.txt.1"),   "rotated one\nrotated two\n");
    writeFile(tmp.filePath("corpus.txt.tmp"), "transient prune file\n");

    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), now);
    CHECK(c.sentences == 3);
    CHECK(c.words     == 7);
    CHECK(c.wordsPerDay.last() == 3);
}

TEST_CASE("wipeFiles: removes corpus, rotations, timestamps, scratch files and profile", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QStringList learned = {
        "corpus.txt", "corpus.txt.1", "corpus.txt.tmp",
        "corpus_timestamps.txt", "corpus_timestamps.txt.tmp", "style_profile.json",
    };
    for (const QString &f : learned) writeFile(tmp.filePath(f), "x\n");
    // Engine state that is not learned text must survive.
    for (const QString &f : {QString("metrics.json"), QString("fcitx5.log")})
        writeFile(tmp.filePath(f), "keep\n");

    CHECK(CorpusStats::wipeFiles(tmp.filePath("corpus.txt")) == learned.size());
    for (const QString &f : learned)
        CHECK_FALSE(QFile::exists(tmp.filePath(f)));
    CHECK(QFile::exists(tmp.filePath("metrics.json")));
    CHECK(QFile::exists(tmp.filePath("fcitx5.log")));

    const auto c = CorpusStats::count(tmp.filePath("corpus.txt"), QDateTime::currentDateTime());
    CHECK(c.words == 0);
}

TEST_CASE("wipeFiles: nothing to delete is not an error", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    CHECK(CorpusStats::wipeFiles(tmp.filePath("corpus.txt")) == 0);
}

TEST_CASE("wipeFiles: reports failure when a file cannot be removed", "[corpus_stats]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    writeFile(tmp.filePath("corpus.txt"), "x\n");
    QFile dir(tmp.path());
    REQUIRE(dir.setPermissions(QFile::ReadOwner | QFile::ExeOwner));   // read-only dir
    CHECK(CorpusStats::wipeFiles(tmp.filePath("corpus.txt")) == -1);
    dir.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

// ── EngineProbe::parseMetrics ────────────────────────────────────────────────

TEST_CASE("parseMetrics: reads the counters the engine writes", "[engine_probe]") {
    // Verbatim shape of XTypeEngine::writeMetrics() output.
    const QByteArray json =
        R"({"latency_p50_ms":110,"latency_p95_ms":292,"suggestions_generated":147,)"
        R"("suggestions_accepted":14,"chars_accepted":47,"updated_at":1792011683})" "\n";

    const auto m = EngineProbe::parseMetrics(json);
    REQUIRE(m.has_value());
    CHECK(m->latencyP50    == 110);
    CHECK(m->generated     == 147);
    CHECK(m->accepted      == 14);
    CHECK(m->charsAccepted == 47);
}

TEST_CASE("parseMetrics: empty or truncated file is rejected", "[engine_probe]") {
    CHECK_FALSE(EngineProbe::parseMetrics("").has_value());
    CHECK_FALSE(EngineProbe::parseMetrics(R"({"latency_p50_ms":11)").has_value());
}

TEST_CASE("time-saved estimate assumes 40 WPM", "[engine_probe]") {
    // 200 accepted chars at 40 WPM x 5 chars/word = one minute of typing.
    CHECK(200.0 / EngineProbe::kCharsPerSecond == 60.0);
}

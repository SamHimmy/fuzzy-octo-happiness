#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    struct Fields {
        // Fully qualified — 'using namespace' does not apply inside Fields
        geode::async::TaskHolder<geode::utils::web::WebResponse> m_ngTask;
    };

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    // Only intercept when boomlings rejects (result == "-1")
    // Whitelisted songs pass through completely untouched
    void onGetSongInfoCompleted(gd::string songIDStr, gd::string result) {
        if (result != "-1" && !result.empty()) {
            SongUnlockerMDM::onGetSongInfoCompleted(songIDStr, result);
            return;
        }

        int songID = 0;
        try { songID = std::stoi(std::string(songIDStr)); }
        catch (...) {
            SongUnlockerMDM::onGetSongInfoCompleted(songIDStr, result);
            return;
        }

        auto capturedIDStr = std::string(songIDStr);

        // Geode v5 async API — callback is called on main thread automatically
        auto req = web::WebRequest();
        m_fields->m_ngTask.spawn(
            req.get(fmt::format("https://www.newgrounds.com/audio/load/{}", songID)),
            [this, capturedIDStr, songID](web::WebResponse value) {
                if (!value.ok()) {
                    SongUnlockerMDM::onGetSongInfoCompleted(capturedIDStr, "-1");
                    return;
                }

                auto jsonResult = value.json();
                if (jsonResult.isErr()) {
                    SongUnlockerMDM::onGetSongInfoCompleted(capturedIDStr, "-1");
                    return;
                }

                auto const& json = jsonResult.unwrap();
                auto title  = json["title"].asString().unwrapOr("Unknown Song");
                auto artist = json["author"].asString().unwrapOr("Unknown Artist");
                auto dlUrl  = json["url"].asString().unwrapOr("");

                // Boomlings-format response — GD parses this natively
                auto fakeResponse = fmt::format(
                    "1~|~{}~|~2~|~{}~|~3~|~0~|~4~|~{}~|~5~|~0~|~6~|~~|~7~|~~|~10~|~{}",
                    songID, title, artist, dlUrl
                );

                SongUnlockerMDM::onGetSongInfoCompleted(capturedIDStr, fakeResponse);
            }
        );
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget
// ─────────────────────────────────────────────────────────────────────────────
class $modify(CustomSongWidget) {
    bool init(SongInfoObject* songInfo, CustomSongDelegate* delegate,
              bool showSongSelect, bool showPlayMusic, bool showUseButton,
              bool isRobtopSong, bool unkBool, bool isMusicLibrary, int unk) {
        if (songInfo) songInfo->m_verified = true;
        return CustomSongWidget::init(
            songInfo, delegate, showSongSelect, showPlayMusic,
            showUseButton, isRobtopSong, unkBool, isMusicLibrary, unk
        );
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: LevelEditorLayer
// ─────────────────────────────────────────────────────────────────────────────
class $modify(LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (level && level->m_songID != 0) {
            auto* mdm = MusicDownloadManager::sharedState();
            if (auto* song = mdm->getSongInfoObject(level->m_songID))
                song->m_verified = true;
        }
        return LevelEditorLayer::init(level, unk);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: EditLevelLayer
// ─────────────────────────────────────────────────────────────────────────────
class $modify(EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (level && level->m_songID != 0) {
            auto* mdm = MusicDownloadManager::sharedState();
            if (auto* song = mdm->getSongInfoObject(level->m_songID))
                song->m_verified = true;
        }
        return EditLevelLayer::init(level);
    }
};

$on_mod(Loaded) {
    log::info("Song Unlocker loaded.");
}

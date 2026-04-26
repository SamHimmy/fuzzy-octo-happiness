#include <Geode/Geode.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Parse the NG audio/load JSON and build a SongInfoObject
SongInfoObject* songInfoFromNG(int songID, matjson::Value const& json) {
    auto* obj = SongInfoObject::create(
        /*songID*/     songID,
        /*songName*/   json["title"].asString().unwrapOr("Unknown"),
        /*artistName*/ json["author"].asString().unwrapOr("Unknown"),
        /*fileSize*/   0.f,
        /*youtubeURL*/ "",
        /*songURL*/    json["url"].asString().unwrapOr(""),
        /*isVerified*/ true,
        /*isRobtop*/   false
    );
    return obj;
}

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    // Always report every song as verified locally
    bool isVerifiedSong(int songID) { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID) { return true; }

    // Intercept song info fetching
    void getSongInfo(int songID, bool isRobtop) {
        // Let normal path run — it may succeed for whitelisted songs
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        // Simultaneously fire a direct Newgrounds request as a fallback.
        // If the boomlings server already responded fine, the result here
        // will be a no-op because the song will already be cached.
        if (!isRobtop) {
            auto url = fmt::format(
                "https://www.newgrounds.com/audio/load/{}",
                songID
            );

            auto req = web::WebRequest();
            req.header("Accept", "application/json");

            auto task = req.get(url);
            task.listen([this, songID](web::WebResponse* res) {
                if (!res || !res->ok()) return;

                // If the song is already loaded, don't overwrite it
                if (this->getSongInfoObject(songID)) return;

                auto jsonResult = res->json();
                if (jsonResult.isErr()) return;
                auto json = jsonResult.unwrap();

                // Build and register the song object
                if (auto* songObj = songInfoFromNG(songID, json)) {
                    songObj->m_verified = true;
                    // Dispatch to any waiting delegates
                    this->addSongInfoObject(songObj);
                    this->onGetSongInfoCompleted(
                        fmt::format("{}", songID), "1"
                    );
                }
            });
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget — suppress the warning UI
// ─────────────────────────────────────────────────────────────────────────────
class $modify(CustomSongWidget) {

    bool init(SongInfoObject* songInfo, CustomSongDelegate* delegate,
              bool showSongSelect, bool showPlayMusic, bool showUseButton,
              bool isRobtopSong, bool unkBool, bool isMusicLibrary, int unk) {

        if (songInfo) songInfo->m_verified = true;

        return CustomSongWidget::init(
            songInfo, delegate,
            showSongSelect, showPlayMusic, showUseButton,
            isRobtopSong, unkBool, isMusicLibrary, unk
        );
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: LevelEditorLayer
// ─────────────────────────────────────────────────────────────────────────────
class $modify(LevelEditorLayer) {

    bool init(GJGameLevel* level, bool unk) {
        if (level && level->m_songID != 0) {
            auto mdm = MusicDownloadManager::sharedState();
            if (auto song = mdm->getSongInfoObject(level->m_songID))
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
            auto mdm = MusicDownloadManager::sharedState();
            if (auto song = mdm->getSongInfoObject(level->m_songID))
                song->m_verified = true;
        }
        return EditLevelLayer::init(level);
    }
};

$on_mod(Loaded) {
    log::info("Song Unlocker loaded — fetching non-whitelisted songs via NG API.");
}

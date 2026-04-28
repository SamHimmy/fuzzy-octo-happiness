#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <thread>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Fetch song info from Newgrounds and feed it back through GD's own parser.
// Must only be called from a background thread.
// ─────────────────────────────────────────────────────────────────────────────
static void fetchFromNG(int songID) {
    // Build a minimal HTTP GET — no Geode async, just synchronous curl via web::WebRequest
    // web::WebRequest::send() is synchronous when awaited synchronously
    auto result = web::fetch(
        fmt::format("https://www.newgrounds.com/audio/load/{}", songID)
    );

    Loader::get()->queueInMainThread([songID, result = std::move(result)]() {
        auto* mdm = MusicDownloadManager::sharedState();
        std::string capturedID = std::to_string(songID);

        if (result.isErr()) {
            // Let GD show its normal error
            mdm->onGetSongInfoCompleted(capturedID, "-1");
            return;
        }

        auto const& body = result.unwrap();
        auto jsonResult  = matjson::parse(body);
        if (jsonResult.isErr()) {
            mdm->onGetSongInfoCompleted(capturedID, "-1");
            return;
        }

        auto const& json = jsonResult.unwrap();
        auto title  = json["title"].asString().unwrapOr("Unknown Song");
        auto artist = json["author"].asString().unwrapOr("Unknown Artist");
        auto dlUrl  = json["url"].asString().unwrapOr("");

        // Craft a boomlings-style response that GD's own parser understands
        auto fakeResponse = fmt::format(
            "1~|~{}~|~2~|~{}~|~3~|~0~|~4~|~{}~|~5~|~0~|~6~|~~|~7~|~~|~10~|~{}",
            songID, title, artist, dlUrl
        );

        mdm->onGetSongInfoCompleted(capturedID, fakeResponse);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    // ── Whitelist bypass ─────────────────────────────────────────────────────
    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    // ── Intercept boomlings failure response only ────────────────────────────
    void onGetSongInfoCompleted(gd::string songIDStr, gd::string result) {
        // Boomlings succeeded — let GD handle it normally, don't touch it
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

        // Boomlings rejected it — fetch from NG on a background thread
        std::thread([songID]() {
            fetchFromNG(songID);
        }).detach();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget — suppress the "not whitelisted" warning banner
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

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <unordered_set>

using namespace geode::prelude;

static std::unordered_set<int> s_ngInFlight;
static std::unordered_set<int> s_ngDone;

// File-scope aliases — 'using namespace geode::prelude' IS in scope here.
// Using these inside Fields avoids all namespace resolution issues.
using NGWebTask = Task<web::WebResponse, web::WebProgress>;
using NGListener = EventListener<NGWebTask>;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    struct Fields {
        NGListener m_listener; // file-scope alias resolves cleanly here
    };

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        // Always run boomlings path (whitelisted songs handled here)
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        if (isRobtop)                    return;
        if (s_ngDone.count(songID))      return;
        if (s_ngInFlight.count(songID))  return;

        s_ngInFlight.insert(songID);

        // Docs pattern: bind() first, then setFilter()
        m_fields->m_listener.bind([this, songID](NGWebTask::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                s_ngInFlight.erase(songID);

                if (!res->ok()) return;
                // Boomlings already resolved it — skip
                if (this->getSongInfoObject(songID)) return;

                auto jsonResult = res->json();
                if (jsonResult.isErr()) return;

                auto const& json = jsonResult.unwrap();
                auto title  = json["title"].asString().unwrapOr("Unknown Song");
                auto artist = json["author"].asString().unwrapOr("Unknown Artist");
                auto dlUrl  = json["url"].asString().unwrapOr("");

                s_ngDone.insert(songID);

                // Feed a boomlings-format string into GD's OWN parser.
                // We do NOT hook onGetSongInfoCompleted, so this goes straight
                // to the original GD function — no vtable re-entry, no crash.
                auto fakeResponse = fmt::format(
                    "1~|~{}~|~2~|~{}~|~3~|~0~|~4~|~{}~|~5~|~0~|~6~|~~|~7~|~~|~10~|~{}",
                    songID, title, artist, dlUrl
                );
                this->onGetSongInfoCompleted(
                    std::to_string(songID), fakeResponse
                );
            } else if (e->isCancelled()) {
                s_ngInFlight.erase(songID);
            }
        });

        auto req = web::WebRequest();
        m_fields->m_listener.setFilter(
            req.get(fmt::format("https://www.newgrounds.com/audio/load/{}", songID))
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

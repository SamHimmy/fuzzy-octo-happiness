// Exact API from https://docs.geode-sdk.org/tutorials/fetch/
// EventListener<web::WebTask> with .bind() then .setFilter()

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

// Percent-encode a URL so GD's parser handles it correctly
static std::string urlEncode(std::string const& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ||
            c == ':' || c == '/' || c == '%' || c == '?' || c == '=') {
            out += (char)c;
        } else {
            out += fmt::format("%{:02X}", c);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    // Exact pattern from official docs
    struct Fields {
        EventListener<web::WebTask> m_listener;
    };

    // ── Whitelist bypass ─────────────────────────────────────────────────────
    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        if (isRobtop)                   return;
        if (s_ngDone.count(songID))     return;
        if (s_ngInFlight.count(songID)) return;

        s_ngInFlight.insert(songID);

        m_fields->m_listener.bind([this, songID](web::WebTask::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                s_ngInFlight.erase(songID);

                if (!res->ok()) return;
                if (this->getSongInfoObject(songID)) return;

                auto jsonResult = res->json();
                if (jsonResult.isErr()) return;

                auto const& json = jsonResult.unwrap();
                auto title  = json["title"].asString().unwrapOr("Unknown Song");
                auto artist = json["author"].asString().unwrapOr("Unknown Artist");
                auto dlUrl  = urlEncode(json["url"].asString().unwrapOr(""));

                if (dlUrl.empty()) return;

                s_ngDone.insert(songID);

                // Correct GD song response format:
                // Key 1  = song ID
                // Key 2  = song name
                // Key 3  = artist ID (0 = unknown)
                // Key 4  = artist name
                // Key 5  = file size MB
                // Key 6  = YouTube video ID
                // Key 7  = YouTube channel URL
                // Key 8  = isVerified (1 = artist is NG-scouted / whitelisted)
                // Key 10 = download URL (percent-encoded mp3 link)
                auto fakeResponse = fmt::format(
                    "1~|~{}~|~2~|~{}~|~3~|~0~|~4~|~{}~|~5~|~0~|~6~|~~|~7~|~~|~8~|~1~|~10~|~{}",
                    songID, title, artist, dlUrl
                );

                auto capturedID  = std::to_string(songID);
                auto capturedRes = fakeResponse;

                // queueInMainThread — async callbacks may fire on worker
                // threads on Android; all GD calls must be on main thread
                Loader::get()->queueInMainThread([this, capturedID, capturedRes]() {
                    // Call base class directly to avoid vtable re-entry crash
                    MusicDownloadManager::onGetSongInfoCompleted(capturedID, capturedRes);
                });

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

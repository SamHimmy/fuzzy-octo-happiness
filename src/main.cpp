#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <unordered_set>
#include <map>
#include <memory>

using namespace geode::prelude;

// All statics at file scope — 'using namespace geode::prelude' IS active here.
// EventListener cannot live inside $modify Fields because the $modify macro
// creates a template context where name lookup fails for geode:: types.
using WebListener = EventListener<Task<web::WebResponse, web::WebProgress>>;
static std::map<int, std::unique_ptr<WebListener>> s_listeners;
static std::unordered_set<int> s_ngInFlight;
static std::unordered_set<int> s_ngDone;

static std::string urlEncode(std::string const& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~'
            || c == ':' || c == '/' || c == '%' || c == '?' || c == '=') {
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

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        if (isRobtop)                   return;
        if (s_ngDone.count(songID))     return;
        if (s_ngInFlight.count(songID)) return;

        s_ngInFlight.insert(songID);

        auto listener = std::make_unique<WebListener>();
        listener->bind([this, songID](Task<web::WebResponse, web::WebProgress>::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                s_ngInFlight.erase(songID);
                s_listeners.erase(songID);

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

                // Key 8 = isVerified (artist is NG-scouted / whitelisted)
                auto fakeResponse = fmt::format(
                    "1~|~{}~|~2~|~{}~|~3~|~0~|~4~|~{}~|~5~|~0~|~"
                    "6~|~~|~7~|~~|~8~|~1~|~10~|~{}",
                    songID, title, artist, dlUrl
                );

                auto capturedID  = std::to_string(songID);
                auto capturedRes = fakeResponse;

                // Must be on main thread — GD calls crash off main thread on Android
                Loader::get()->queueInMainThread([this, capturedID, capturedRes]() {
                    // Call base class directly — avoids vtable re-entry crash
                    MusicDownloadManager::onGetSongInfoCompleted(capturedID, capturedRes);
                });

            } else if (e->isCancelled()) {
                s_ngInFlight.erase(songID);
                s_listeners.erase(songID);
            }
        });

        auto req = web::WebRequest();
        listener->setFilter(
            req.get(fmt::format("https://www.newgrounds.com/audio/load/{}", songID))
        );
        s_listeners[songID] = std::move(listener);
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

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>

using namespace geode::prelude;

static std::unordered_map<int, Ref<SongInfoObject>> s_ngSongCache;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// Using the exact Fields + EventListener<web::WebTask> pattern from the docs
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    // Docs pattern: EventListener<web::WebTask> inside Fields
    struct Fields {
        EventListener<web::WebTask> m_listener;
    };

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    SongInfoObject* getSongInfoObject(int songID) {
        if (auto* obj = MusicDownloadManager::getSongInfoObject(songID))
            return obj;
        auto it = s_ngSongCache.find(songID);
        if (it != s_ngSongCache.end())
            return it->second.data();
        return nullptr;
    }

    void getSongInfo(int songID, bool isRobtop) {
        SongUnlockerMDM::getSongInfo(songID, isRobtop);
        if (isRobtop) return;

        auto url = fmt::format("https://www.newgrounds.com/audio/load/{}", songID);

        // Docs pattern: bind then setFilter
        m_fields->m_listener.bind([this, songID](web::WebTask::Event* e) {
            if (auto* res = e->getValue()) {
                if (!res->ok()) return;
                if (MusicDownloadManager::getSongInfoObject(songID)) return;

                auto jsonResult = res->json();
                if (jsonResult.isErr()) return;
                auto const& json = jsonResult.unwrap();

                auto title  = json["title"].asString().unwrapOr("Unknown Song");
                auto artist = json["author"].asString().unwrapOr("Unknown Artist");
                auto dlUrl  = json["url"].asString().unwrapOr("");

                auto* songObj = SongInfoObject::create(
                    songID, title, artist,
                    0, 0.f, "", "", dlUrl, "", 0, "", false, 0, 0
                );
                if (!songObj) return;
                songObj->m_verified = true;

                s_ngSongCache[songID] = songObj;

                this->onGetSongInfoCompleted(
                    cocos2d::CCString::createWithFormat("%i", songID)->getCString(),
                    "1"
                );
            }
        });

        auto req = web::WebRequest();
        m_fields->m_listener.setFilter(req.get(url));
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

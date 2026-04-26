#include <Geode/Geode.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    struct Fields {
        std::unordered_map<int, EventListener<Task<web::WebResponse, web::WebProgress>>> m_ngListeners;
    };

    bool isVerifiedSong(int songID)                  { return true; }
    bool isSongVerified(int songID, bool includeLocal){ return true; }
    bool isMusicAllowed(int songID)                  { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        // Always attempt normal path (whitelisted songs still go through boomlings)
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        if (isRobtop) return;

        // Parallel direct-NG fetch as fallback
        auto url = fmt::format("https://www.newgrounds.com/audio/load/{}", songID);

        auto& listener = m_fields->m_ngListeners[songID];
        listener.bind([this, songID](Task<web::WebResponse, web::WebProgress>::Event* e) {
            auto* res = e->getValue();
            if (!res || !res->ok()) return;

            // Boomlings already resolved it — nothing to do
            if (this->getSongInfoObject(songID)) return;

            auto jsonResult = res->json();
            if (jsonResult.isErr()) return;
            auto const& json = jsonResult.unwrap();

            auto title  = json["title"].asString().unwrapOr("Unknown Song");
            auto artist = json["author"].asString().unwrapOr("Unknown Artist");
            auto dlUrl  = json["url"].asString().unwrapOr("");

            // Build a SongInfoObject with the full 14-arg create so every
            // field is initialised correctly from the start
            auto* songObj = SongInfoObject::create(
                songID,          // songID
                title,           // songName
                artist,          // artistName
                0,               // artistID
                0.f,             // filesize
                "",              // youtubeVideo
                "",              // youtubeChannel
                dlUrl,           // url
                "",              // unknown
                0,               // nongType
                "",              // extraArtistIDs
                false,           // isNew
                0,               // libraryOrder
                0                // priority
            );
            if (!songObj) return;
            songObj->m_verified = true;

            // Store in the manager's song object cache
            this->m_songObjects->setObject(
                songObj,
                cocos2d::CCString::createWithFormat("%i", songID)->getCString()
            );

            // Notify any waiting UI (CustomSongWidget etc.)
            this->onGetSongInfoCompleted(
                cocos2d::CCString::createWithFormat("%i", songID)->getCString(),
                "1"
            );
        });
        listener.setFilter(web::WebRequest().get(url));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget — hide the "not whitelisted" warning
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
            auto mdm = MusicDownloadManager::sharedState();
            if (auto* song = mdm->getSongInfoObject(level->m_songID))
                song->m_verified = true;
        }
        return EditLevelLayer::init(level);
    }
};

$on_mod(Loaded) {
    log::info("Song Unlocker loaded.");
}

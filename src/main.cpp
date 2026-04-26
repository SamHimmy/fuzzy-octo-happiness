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

    // One EventListener per pending NG request, keyed by songID
    struct Fields {
        std::unordered_map<int, EventListener<web::WebTask>> m_ngListeners;
    };

    bool isVerifiedSong(int songID) { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID) { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        // Always run the normal path (works for whitelisted songs)
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        // Fire a parallel direct NG request for custom songs
        if (isRobtop) return;

        auto url = fmt::format("https://www.newgrounds.com/audio/load/{}", songID);

        auto& listener = m_fields->m_ngListeners[songID];
        listener.bind([this, songID](web::WebTask::Event* e) {
            auto* res = e->getValue();
            if (!res || !res->ok()) return;

            // Don't overwrite if boomlings already resolved it
            if (this->getSongInfoObject(songID)) return;

            auto jsonResult = res->json();
            if (jsonResult.isErr()) return;
            auto const& json = jsonResult.unwrap();

            // Use the single-arg overload, then fill fields manually
            auto* songObj = SongInfoObject::create(songID);
            if (!songObj) return;

            songObj->m_songName    = json["title"].asString().unwrapOr("Unknown Song");
            songObj->m_artistName  = json["author"].asString().unwrapOr("Unknown Artist");
            songObj->m_downloadURL = json["url"].asString().unwrapOr("");
            songObj->m_verified    = true;

            // Register in the song dictionary so getSongInfoObject() finds it
            this->m_songs->setObject(songObj, fmt::format("{}", songID));

            // Tell any waiting UI that the song info arrived
            this->onGetSongInfoCompleted(fmt::format("{}", songID), "1");
        });
        listener.setFilter(web::WebRequest().get(url));
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

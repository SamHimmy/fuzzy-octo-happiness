#include <Geode/Geode.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>

using namespace geode::prelude;

// Typedef defined at file scope (outside any macro-generated class) so it is
// available inside struct Fields where 'using namespace geode::prelude' does
// NOT apply.
using NGWebTask = Task<web::WebResponse, web::WebProgress>;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    struct Fields {
        std::unordered_map<int, EventListener<NGWebTask> > m_ngListeners;
    };

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    void getSongInfo(int songID, bool isRobtop) {
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        if (isRobtop) return;

        auto url = fmt::format("https://www.newgrounds.com/audio/load/{}", songID);

        auto& listener = m_fields->m_ngListeners[songID];
        listener.bind([this, songID](NGWebTask::Event* e) {
            auto* res = e->getValue();
            if (!res || !res->ok()) return;

            if (this->getSongInfoObject(songID)) return;

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

            this->m_songObjects->setObject(
                songObj,
                cocos2d::CCString::createWithFormat("%i", songID)->getCString()
            );

            this->onGetSongInfoCompleted(
                cocos2d::CCString::createWithFormat("%i", songID)->getCString(),
                "1"
            );
        });
        listener.setFilter(web::WebRequest().get(url));
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

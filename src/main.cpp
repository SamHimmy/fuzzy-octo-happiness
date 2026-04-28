#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <unordered_set>

using namespace geode::prelude;

static std::unordered_set<int> s_ngInFlight;
static std::unordered_set<int> s_ngDone;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(SongUnlockerMDM, MusicDownloadManager) {

    struct Fields {
        geode::async::TaskHolder<geode::utils::web::WebResponse> m_ngTask;
    };

    bool isVerifiedSong(int songID)                    { return true; }
    bool isSongVerified(int songID, bool includeLocal) { return true; }
    bool isMusicAllowed(int songID)                    { return true; }

    // Hook getSongInfo — NOT onGetSongInfoCompleted (causes vtable re-entry crash)
    void getSongInfo(int songID, bool isRobtop) {
        // Always run boomlings path (handles whitelisted songs)
        SongUnlockerMDM::getSongInfo(songID, isRobtop);

        // Skip robtop songs, already fetched, or already in flight
        if (isRobtop) return;
        if (s_ngDone.count(songID)) return;
        if (s_ngInFlight.count(songID)) return;

        s_ngInFlight.insert(songID);

        auto req = web::WebRequest();
        m_fields->m_ngTask.spawn(
            req.get(fmt::format("https://www.newgrounds.com/audio/load/{}", songID)),
            [this, songID](web::WebResponse value) {
                s_ngInFlight.erase(songID);

                if (!value.ok()) return;

                // If boomlings already resolved it, nothing to do
                if (this->getSongInfoObject(songID)) return;

                auto jsonResult = value.json();
                if (jsonResult.isErr()) return;

                auto const& json = jsonResult.unwrap();
                auto title  = json["title"].asString().unwrapOr("Unknown Song");
                auto artist = json["author"].asString().unwrapOr("Unknown Artist");
                auto dlUrl  = json["url"].asString().unwrapOr("");

                // Build SongInfoObject using the full 14-arg create
                auto* obj = SongInfoObject::create(
                    songID, title, artist,
                    0,      // artistID
                    0.f,    // filesize
                    "",     // ytVideo
                    "",     // ytChannel
                    dlUrl,  // downloadURL
                    "",     // unknown
                    0,      // nongType
                    "",     // extraArtistIDs
                    false,  // isNew
                    0,      // libraryOrder
                    0       // priority
                );
                if (!obj) return;
                obj->m_verified = true;

                s_ngDone.insert(songID);

                // Store in MDM's song object dictionary
                auto key = cocos2d::CCString::createWithFormat("%i", songID);
                this->m_songObjects->setObject(obj, key->getCString());

                // Notify all registered GJSongDelegate listeners
                auto* delegates = this->m_songDelegates;
                if (!delegates) return;

                cocos2d::CCDictElement* el = nullptr;
                CCDICT_FOREACH(delegates, el) {
                    auto* delegate = static_cast<GJSongDelegate*>(el->getObject());
                    if (delegate)
                        delegate->songInfoDidLoad(obj);
                }
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

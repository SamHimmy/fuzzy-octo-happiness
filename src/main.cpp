#include <Geode/Geode.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
// ─────────────────────────────────────────────────────────────────────────────
class $modify(MusicDownloadManager) {

    bool isVerifiedSong(int songID) {
        return true;
    }

    bool isSongVerified(int songID, bool includeLocal) {
        return true;
    }

    bool isMusicAllowed(int songID) {
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget
// ─────────────────────────────────────────────────────────────────────────────
class $modify(CustomSongWidget) {

    bool init(SongInfoObject* songInfo, CustomSongDelegate* delegate,
              bool showSongSelect, bool showPlayMusic, bool showUseButton,
              bool isRobtopSong, bool unkBool, bool isMusicLibrary, int unk) {

        if (songInfo) {
            songInfo->m_verified = true;
        }

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
            if (auto song = mdm->getSongInfoObject(level->m_songID)) {
                song->m_verified = true;
            }
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
            if (auto song = mdm->getSongInfoObject(level->m_songID)) {
                song->m_verified = true;
            }
        }
        return EditLevelLayer::init(level);
    }
};

$on_mod(Loaded) {
    log::info("Song Unlocker loaded — non-whitelisted songs are now allowed.");
}

#include <Geode/Geode.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/SongInfoObject.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Hook: MusicDownloadManager
//
// isVerifiedSong() is used by the game to decide whether an NG song may be
// attached to a level.  Returning true unconditionally lifts the whitelist.
// ─────────────────────────────────────────────────────────────────────────────
class $modify(MusicDownloadManager) {

    // GD 2.2+: primary whitelist gate
    bool isVerifiedSong(int songID) {
        return true;
    }

    // Older builds / secondary check used in some codepaths
    bool isSongVerified(int songID, bool includeLocal) {
        return true;
    }

    // Prevents the "this song is not allowed" toast from appearing
    bool isMusicAllowed(int songID) {
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: CustomSongWidget
//
// The widget shows a warning banner and disables the "use" button when a song
// is not whitelisted.  We suppress that UI so every song looks usable.
// ─────────────────────────────────────────────────────────────────────────────
class $modify(CustomSongWidget) {

    // Called whenever the widget refreshes its display
    bool init(SongInfoObject* songInfo, CustomSongDelegate* delegate,
              bool showSongSelect, bool showPlayMusic, bool showUseButton,
              bool isRobtopSong, bool unkBool, bool isMusicLibrary, int unk) {

        // Force the widget to believe the song is always "allowed"
        if (songInfo) {
            songInfo->m_isVerified = true;
        }

        return CustomSongWidget::init(
            songInfo, delegate,
            showSongSelect, showPlayMusic, showUseButton,
            isRobtopSong, unkBool, isMusicLibrary, unk
        );
    }

    // Suppresses the warning label/icon that would otherwise overlay the widget
    void updateSongObject(SongInfoObject* song) {
        if (song) {
            song->m_isVerified = true;
        }
        CustomSongWidget::updateSongObject(song);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: LevelEditorLayer
//
// When the editor saves or verifies a level it re-checks the song; we mark the
// song verified before that check runs so the level passes validation.
// ─────────────────────────────────────────────────────────────────────────────
class $modify(LevelEditorLayer) {

    bool init(GJGameLevel* level, bool unk) {
        // Mark the level's custom song as verified before the editor opens
        if (level && level->m_songID != 0) {
            auto mdm = MusicDownloadManager::sharedState();
            if (auto song = mdm->getSongInfoObject(level->m_songID)) {
                song->m_isVerified = true;
            }
        }
        return LevelEditorLayer::init(level, unk);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Hook: EditLevelLayer
//
// The "Edit Level" screen also shows a warning when the attached song is not
// whitelisted.  We pre-verify the song object so no warning ever appears.
// ─────────────────────────────────────────────────────────────────────────────
class $modify(EditLevelLayer) {

    bool init(GJGameLevel* level) {
        if (level && level->m_songID != 0) {
            auto mdm = MusicDownloadManager::sharedState();
            if (auto song = mdm->getSongInfoObject(level->m_songID)) {
                song->m_isVerified = true;
            }
        }
        return EditLevelLayer::init(level);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Mod lifecycle
// ─────────────────────────────────────────────────────────────────────────────
$on_mod(Loaded) {
    log::info("Song Unlocker loaded — non-whitelisted songs are now allowed.");
}

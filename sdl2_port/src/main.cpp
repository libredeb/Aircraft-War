#include "game.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <libgen.h>
#include <climits>

static bool hasImageDir(const std::string& root) {
    return access((root + "/Image").c_str(), F_OK) == 0;
}

// Directory containing the running binary (Linux: /proc/self/exe).
static std::string executableDir(const char* argv0) {
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(dirname(buf));
    }
#endif
    if (argv0 && argv0[0]) {
        char tmp[PATH_MAX];
        strncpy(tmp, argv0, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        // Absolute path in argv0
        if (tmp[0] == '/') return std::string(dirname(tmp));
        // Relative: resolve against cwd
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            std::string full = std::string(cwd) + "/" + tmp;
            strncpy(tmp, full.c_str(), sizeof(tmp) - 1);
            return std::string(dirname(tmp));
        }
        return std::string(dirname(tmp));
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) return cwd;
    return ".";
}

static std::string findDataPath(const char* argv0) {
    const std::string exeDir = executableDir(argv0);

    // Candidates relative to the binary location (order matters):
    //  1) install: /usr/local/bin/aircraftwar → /usr/local/share/aircraftwar
    //  2) Image/ next to the binary
    //  3) one/two levels up (sdl2_port/build → repo root)
    const std::vector<std::string> candidates = {
        exeDir + "/../share/aircraftwar",
        exeDir + "/share/aircraftwar",
        exeDir,
        exeDir + "/..",
        exeDir + "/../..",
#ifdef DATA_DIR
        DATA_DIR,
#endif
        ".",
        "..",
        "../..",
    };

    for (const auto& c : candidates) {
        if (hasImageDir(c)) {
            // Normalize "bin/../share/..." for logs
            fprintf(stderr, "Data path: %s (from binary at %s)\n", c.c_str(), exeDir.c_str());
            return c;
        }
    }

    fprintf(stderr, "WARNING: Image/ not found near binary (%s). Use: aircraftwar -d /path/to/data\n",
            exeDir.c_str());
    return ".";
}

int main(int argc, char* argv[]) {
    int width = 720, height = 720;
    bool fullscreen = false;
    std::string dataPath;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0) && i + 1 < argc) {
            dataPath = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Aircraft War - SDL2 Native Port\n\n"
                   "Usage: aircraftwar [options]\n"
                   "  -f, --fullscreen     Run in fullscreen mode\n"
                   "  -w, --width  <N>     Window width  (default: 720)\n"
                   "  -h, --height <N>     Window height (default: 720)\n"
                   "  -d, --data   <path>  Path to game data (dir containing Image/)\n"
                   "\nData is resolved from the binary location:\n"
                   "  <bindir>/../share/aircraftwar   (install layout)\n"
                   "  <bindir>/.. or ../..             (dev build layout)\n"
                   "\nControls (keyboard):\n"
                   "  Arrow keys / WASD    Move\n"
                   "  Z / Space / Enter    Confirm\n"
                   "  X / Shift            Use Bomb\n"
                   "  P / Esc              Pause\n"
                   "\nControls (gamepad - Arduino Leonardo preferred):\n"
                   "  D-Pad / Left stick   Move\n"
                   "  A                    Confirm\n"
                   "  B / X / Y / L / R    Use Bomb\n"
                   "  Start                Pause\n"
                   "  Back                 Back / menu\n");
            return 0;
        }
    }

    if (dataPath.empty()) dataPath = findDataPath(argv[0]);
    else fprintf(stderr, "Data path: %s (-d)\n", dataPath.c_str());

    Game game;
    if (!game.init(dataPath, width, height, fullscreen)) {
        fprintf(stderr, "Failed to initialize game.\n");
        return 1;
    }

    game.run();
    game.shutdown();
    return 0;
}

#include "game.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <libgen.h>

static bool hasImageDir(const std::string& root) {
    return access((root + "/Image").c_str(), F_OK) == 0;
}

static std::string findDataPath(const char* argv0) {
    // 1) Compile-time repo root (dev builds)
#ifdef DEV_DATA_DIR
    if (hasImageDir(DEV_DATA_DIR)) {
        fprintf(stderr, "Data path: %s (DEV_DATA_DIR)\n", DEV_DATA_DIR);
        return DEV_DATA_DIR;
    }
#endif

    // 2) Installed prefix
#ifdef DATA_DIR
    if (hasImageDir(DATA_DIR)) {
        fprintf(stderr, "Data path: %s (DATA_DIR)\n", DATA_DIR);
        return DATA_DIR;
    }
#endif

    // 3) Relative to executable (Linux /proc, and argv0 dirname)
    auto tryExeDir = [](const std::string& dir) -> std::string {
        if (hasImageDir(dir)) return dir;
        if (hasImageDir(dir + "/..")) return dir + "/..";
        if (hasImageDir(dir + "/../..")) return dir + "/../..";
        return {};
    };

#if defined(__linux__)
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string exeDir = dirname(buf);
        std::string found = tryExeDir(exeDir);
        if (!found.empty()) {
            fprintf(stderr, "Data path: %s (exe)\n", found.c_str());
            return found;
        }
    }
#endif

    if (argv0 && argv0[0]) {
        char tmp[4096];
        strncpy(tmp, argv0, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        std::string exeDir = dirname(tmp);
        // Resolve relative argv0 against cwd
        if (exeDir == "." || (exeDir.size() > 0 && exeDir[0] != '/')) {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) {
                if (exeDir == ".") exeDir = cwd;
                else exeDir = std::string(cwd) + "/" + exeDir;
            }
        }
        std::string found = tryExeDir(exeDir);
        if (!found.empty()) {
            fprintf(stderr, "Data path: %s (argv0)\n", found.c_str());
            return found;
        }
    }

    // 4) CWD heuristics
    const char* cwdCandidates[] = {
        ".", "..", "../..", "sdl2_port/..",
    };
    for (auto* c : cwdCandidates) {
        if (hasImageDir(c)) {
            fprintf(stderr, "Data path: %s (cwd)\n", c);
            return c;
        }
    }

    fprintf(stderr, "WARNING: Image/ not found. Use: aircraftwar -d /path/to/repo\n");
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
                   "  -d, --data   <path>  Path to game data directory (repo root)\n"
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

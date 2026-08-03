#include "game.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <libgen.h>

static std::string findDataPath(const char* /*argv0*/) {
#ifdef DATA_DIR
    if (access(DATA_DIR "/Image", F_OK) == 0)
        return DATA_DIR;
#endif
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string dir = dirname(buf);
        std::string candidate = dir + "/../Image";
        if (access(candidate.c_str(), F_OK) == 0)
            return dir + "/..";
        candidate = dir + "/Image";
        if (access(candidate.c_str(), F_OK) == 0)
            return dir;
    }
    if (access("../Image", F_OK) == 0) return "..";
    if (access("Image", F_OK) == 0)    return ".";
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
                   "  -d, --data   <path>  Path to game data directory\n"
                   "\nControls:\n"
                   "  Arrow keys / D-Pad   Move\n"
                   "  Z / Space / A btn    Confirm / Shoot\n"
                   "  X / Shift / B btn    Use Bomb\n"
                   "  P / Esc / Start      Pause\n");
            return 0;
        }
    }

    if (dataPath.empty()) dataPath = findDataPath(argv[0]);

    Game game;
    if (!game.init(dataPath, width, height, fullscreen)) {
        fprintf(stderr, "Failed to initialize game.\n");
        return 1;
    }

    game.run();
    game.shutdown();
    return 0;
}

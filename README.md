Aircraft-War
============

Fork de [zccrs/Aircraft-War](https://github.com/zccrs/Aircraft-War), originalmente Qt/QML
(Symbian, MeeGo, Sailfish, Ubuntu Touch).

Este repo incluye un **port nativo SDL2** pensado para Linux / Raspberry Pi Zero 2 W
(consola handheld), sin RetroPie ni Qt.

---

## Port SDL2 (recomendado)

Código en `sdl2_port/`. Binario standalone con teclado + gamepads
(prioridad: Arduino Leonardo / GamerCard).

### Dependencias

**Debian / Raspberry Pi OS / Ubuntu:**

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

**macOS (Homebrew):**

```bash
brew install cmake pkg-config sdl2 sdl2_image sdl2_mixer sdl2_ttf
```

### Compilar

Desde la raíz del repositorio:

```bash
cd sdl2_port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Ejecutar

```bash
# Desde sdl2_port/ (el binario encuentra solo los assets del repo)
./build/aircraftwar

# Pantalla completa (típico en la consola / Pi)
./build/aircraftwar -f

# Resolución distinta
./build/aircraftwar -w 720 -h 720

# Si hace falta forzar la ruta a Image/, sound/ y fzmw.ttf:
./build/aircraftwar -d /ruta/al/repo
```

### Instalar en el sistema (Linux / Pi)

```bash
cd sdl2_port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j
sudo cmake --install build
aircraftwar -f
```

Assets quedan en `/usr/local/share/aircraftwar/`.

### Cross-compilación para Pi Zero 2 W (aarch64)

En una máquina x86_64 con toolchain:

```bash
sudo apt install g++-aarch64-linux-gnu
cd sdl2_port
cmake -S . -B build-pi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-zero2w.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-pi -j
```

(Necesitás las libs SDL2 para aarch64 en el sysroot / multiarch.)

### Controles

| Acción    | Teclado              | Gamepad                         |
|-----------|----------------------|---------------------------------|
| Mover     | Flechas / WASD       | D-Pad / stick                   |
| Confirmar | Z / Space / Enter    | A                               |
| Bomba     | X / Shift            | B / X / Y / L / R               |
| Pausar    | P / Esc              | Start                           |
| Atrás     | Backspace / Tab      | Back                            |

Mapeos de gamepad: `sdl2_port/data/gamecontrollerdb.txt`
(Arduino Leonardo, THEGamepad, 8BitDo, PS3/4/5, Xbox, Switch, etc.).

---

## Port Qt / Ubuntu Touch (legado)

```bash
clickable -c qtc_packaging/ubuntu_touch/clickable.json
```

Puede no compilar ya en Symbian/MeeGo.

---

## Licencia

GPL (ver código original).

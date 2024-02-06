Credits
- https://vulkan-tutorial.com
- https://gist.github.com/YukiSnowy/dc31f47448ac61dd6aedee18b5d53858
- https://github.com/lonelydevil/Vulkan-Triangle
- https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
- https://github.com/recp/cglm/
When having problems installing SDL2
- https://github.com/tcbrindle/sdl2-cmake-scripts
Usage:
Prerequisites:
1. Have `gcc` or `clang` installed in `PATH`
2. have `glslc` installed in `PATH`
Prepare build:
1. Run `regen_extern` to clone extern dependencies
2. Adjust compile settings in `regen_cmake`
3. Run `regen_cmake`
Build:
- Run `release` for release build
- Or `debug` for debug build

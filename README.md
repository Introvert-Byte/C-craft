# C+craft - Voxel Engine Foundation (Modern C++20)

A high-performance, modular voxel game engine foundation inspired by Horizon
and early Minecraft, written in Modern C++20. Focus: speed, multi-threaded
chunk generation, and data-oriented memory management.

## Features

* **Data-oriented voxel & chunk structure** – `Block` IDs/metadata and a
  `Chunk` (16 x 256 x 16) stored in a flat, cache-friendly array.
* **Face-culling meshing** – hidden internal faces are discarded; only exposed
  faces are uploaded to an OpenGL VBO/VAO.
* **Procedural world generation** – self-contained Perlin noise (heightmaps +
  caves) with soil layers (grass -> dirt -> stone) and sand beaches.
* **Multi-threaded chunk generation** – chunks are generated with
  `std::async` on background threads so the render thread never stutters.
* **Player & camera** – first-person camera, smooth WASD + mouse look (Euler
  angles), and precise AABB collision detection/resolution against solid voxels.
* **Rendering** – OpenGL 4.3 core, shader class (vertex/fragment), lighting
  (ambient + diffuse) and a single texture atlas for all block faces
  (top/side/bottom variants).
* **Memory management** – `std::vector`, `std::unique_ptr`/`std::shared_ptr`,
  RAII throughout.

## Build

```bash
git clone <your-repo-url>
cd C+craft
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./C+craft
```

### Dependencies

* **GLFW 3.3+**, **OpenGL 4.3+**, **GLM** – required.
* **GLAD** – vendored under `deps/glad/` (compiled directly into the engine).
* **GLM** – vendored under `deps/glm/` (header-only).
* A minimal X11 stub header is vendored under `deps/x11/` for build
  environments missing the X11 development packages.

By default the build uses pre-built local dependencies under `deps/`.
Set `-DCRAFT_USE_LOCAL_DEPS=OFF` to use system-installed packages via
`find_package(GLFW3)`, `find_package(OpenGL)` and `find_package(glm)`.

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Space | Jump |
| Left Shift | Sprint |
| Mouse | Look around |
| Left click | Capture cursor (look freely) |
| Escape | Quit |
| F1 | Toggle wireframe |

## Git setup

See `scripts/init-git.sh` – it initializes the repo, stages the sources,
commits (`"Initial commit: Voxel Engine core foundation"`), links the GitHub
remote using the supplied Personal Access Token, and pushes to `main`.

Replace `USERNAME` and `REPOSITORY_NAME` in the script with your own before
running, or run the equivalent commands manually:

```bash
git init
git add -A
git commit -m "Initial commit: Voxel Engine core foundation"
git remote add origin https://<GITHUB_PAT>@github.com/USERNAME/REPOSITORY_NAME.git
git branch -M main
git push -u origin main
```

## License

MIT

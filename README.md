# C++ Terminal Animations

Small dependency-free terminal animations written in C++20.

## Build

```sh
make
```

## Run

```sh
./anim
```

Use `Ctrl-C` to quit. The default scene is a centered 60 FPS shaded hedron.
It now starts at 96 faces for a smoother default shape; use `--faces 8` to start from the original octahedron.
Press `Up Arrow` to split one triangular face, making the shape progressively closer to a sphere.
Press `Down Arrow` to remove one face and simplify the shape again.
The faces are filled with left-side lighting so the facets are easier to distinguish.

You can also run one scene directly:

```sh
./anim --scene hedron
./anim --scene cube
./anim --scene orbits
./anim --scene waves
./anim --scene starfield
./anim --scene rain
```

Useful options:

```sh
./anim --width 120 --height 40 --fps 60
./anim --scene hedron --fps 120
./anim --scene hedron --faces 240 --width 180 --height 60
./anim --scene waves
```

## Standalone Animations

Each non-hedron animation file also has its own `main()` and can be built directly:

```sh
make orbits waves starfield rain
./orbits
./waves
./starfield
./rain
```

All of these run until `Ctrl-C` by default.

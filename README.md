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
Press `Up Arrow` to split one triangular face. It starts as an 8-face octahedron and gets closer to a sphere as you add facets.
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
./anim --scene waves --frames 300
```

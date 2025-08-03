# Web Shell System (WSS)

WSS is an experimental shell building framework for Wayland allowing you to easily create shell widgets with
well-established web
technologies. It is built with GTK4 and WebkitGTK (or QT/QTWebEngine as an alternative) in C++ and integrates seamlessly
with any web framework your heart
desires, such as React (or Preact), Vue, Angular or just plain HTML/JS.

> [!CAUTION]
> WSS is currently in super early development and is not yet ready to be called even remotely "feature complete". It is
> a work
> in
> progress, and it probably will be for a while.
> If you want to help out, please do! Any contributions are welcome, and if you have any questions, feel free to ask.

> [!NOTE]
> WSS is not a full desktop environment, but rather a framework to build your own shell on top of Wayland.

## Roadmap

- [x] GTK window management
- [x] GTK layer shell integration
- [x] Webkit integration
- [x] Experimental Qt backend
- [x] TOML configuration
- [ ] WebSocket JSON IPC
    - [ ] Hiding, showing widgets
    - [x] Managing click regions
    - [ ] Command execution
    - [x] Passing widget/monitor state
    - [x] Getting mouse state
    - [ ] Custom integrations (look below)
- [ ] Running widgets through the CLI (for keybinds)
- [ ] Opening widgets based on mouse position (for docks, popups, etc.)
- [x] Basic universal TypeScript library for IPC
- [ ] Integrated modules (with optional React hooks)
    - [ ] Bluetooth
    - [ ] Wifi / Network
    - [ ] Brightness
    - [ ] Battery / Power
    - [ ] Audio
    - [ ] Hyprland
    - [ ] Tray
    - [x] Notifications
    - [x] Apps
    - [ ] Custom commands
- [ ] React Hooks
    - [x] useAutoClickRegion
    - [ ] useWidget / useWidgets
    - [x] useMousePosition
- [ ] Better (or any) documentation
- [ ] Example fully fledged shell to demonstrate capabilities
- [ ] Support for other compositors (Sway, etc.)
- [ ] Plugins system
- [ ] Fix the damn 60hz webkit lock

## Features

- **Web-based widgets**: Create shell widgets using web technologies like React/Preact, Vue, Angular, or plain HTML/JS.
- **Wayland integration**: Seamlessly integrates with Wayland, allowing you to create widgets that work well with the
  Wayland protocol.
- **Solid foundation**: Built on top of GTK4 and WebkitGTK 6.0, providing a solid foundation for the project.
- **Easily configurable**: Configure the structure of your widgets using a simple TOML configuration file.
- **React integration**: Comes with a React-based component/hook library for integrating your shell with your system.
  Provides out of the box integration for bluetooth, wifi, hyprland, tray and more.
- **IPC-like websocket system**: Allows you to communicate with the C++ backend using a websocket system, making it
  easy to build deeply integrated shell widgets. You're not stuck with the components I made!
- **Easy to learn**: Comes with a fully fledged (and lovingly documented) shell to get you started, including a top bar,
  launcher, dock and more.
- **Transparency**: Supports transparency out of the box, allowing you to create visually appealing widgets that blend
  well with your desktop environment.
- (Coming soon*ish*) **Plugins**: Supports C++ plugins for the ultimate control.

## Motivation

_I hear you._ I understand that there are already many shell frameworks out there, such
as [AGS](https://github.com/Aylur/ags), [Fabric](https://wiki.ffpy.org/), the
legendary [EWW](https://github.com/elkowar/eww) and the new kid on the block, [Quickshell](https://quickshell.org/).

Unfortunately - **none of them fit my needs**. They are either poorly documented, use outdated GTK versions, are very
much early in development or have an in-house developed configuration language that is notoriously hard to learn and
use. (Looking at you, EWW).
And the most glaring issue of all: you need to know either GTK or QT pretty well to get started with them. They're
objectively great frameworks, but they're just not for me and I suspect not for many others as well. There's a reason
Waybar is so popular!

There's far more web developers out there than GTK or QT developers, and I want to make it easy for them to create their
own shell widgets without having to learn a new language or framework. WSS is designed to be a bridge between the web
development world and the shell development world, allowing you to use your existing skills to create beautiful and
functional shell widgets.

**Know how to make websites?** Good. You already know how to make shell widgets with WSS. Use tailwindcss, bootstrap, or
any other CSS framework you like. Use React, Vue, Angular or just plain HTML/JS. WSS is designed to be flexible at all
costs.

**You might ask - but isn't it slower than a "native" (GTK/QT) shell?** It's a good question, and the answer is, yes, it
is. But the performance difference is negligible unless you're running an older system, and the ease of use and
flexibility of WSS outweighs the performance hit for me. Plus, with the power of modern web technologies, you can create
highly performant widgets that are indistinguishable from native ones. No one forces you to install 500 heavy-weight npm
packages after all. ;)

## Installation

WSS is not yet available in any package manager, so you will have to build it from source. To do so, you will need the
following dependencies:

- GTK4 (4.18 or later)
- GTK4 Layer Shell (1.1.1 or later)
- WebkitGTK (2.48 or later)
- JSON-C
- Libwebsockets
- SD-Bus C++
- Your preferred C compiler and CMake

> [!TIP]
> There is an experimental Qt backend available. It has it's quirks, but it seems to work a bit smoother than the
> GTK/Webkit backend on some systems. If you want to try it out, you can build it with the `-DWSS_USE_QT=ON` flag when
> running CMake. Keep in mind that GTK backend is still the default and recommended one.

### Arch / Hyprland

If you are on Arch Linux, you can install the dependencies with the following command:

```bash
sudo pacman -S gtk4 gtk4-layer-shell webkitgtk-6.0 json-c libwebsockets sdbus-cpp
```

If building with the Qt backend, you will also need to install the following packages (on top of the ones above):

```bash
sudo pacman -S qt6 qt6-webengine layer-shell-qt
```

Compile WSS with:

```bash
git clone https://github.com/SocketByte/web-shell-system.git
cd web-shell-system
mkdir build && cd build
cmake ..
mv wss /usr/bin/wss
```

Add the following line to your `~/.config/hypr/hyprland.conf` file to enable the WSS shell:

```ini
exec-once = wss start
```

You can also provide a custom configuration file by adding the `-c` flag:

```ini
exec-once = wss start -c /path/to/your/config.toml
```

For other useful CLI options, run `wss --help`.

## Hyprland / Other Compositors

WSS is **predominantly designed to work with Hyprland**, but it should work with any Wayland compositor that supports
GTK4 Layer Shell protocol.

> [!NOTE]
> I strongly recommend using Hyprland if you can. WSS is tested and developed
> primarily with Hyprland in mind, and some features may not work as expected.
> Full support for other compositors (such as Sway) is something that will come in the future as WSS gets more
> stable.

If you do use Hyprland, these layer rules might be useful for you:

```
layerrule = noanim, wss.shell         # Disable compositor animations (strongly recommended)

layerrule = blur, wss.shell           # Allow background blur on the shell
layerrule = ignorealpha 0, wss.shell  # Ignore blur when alpha is 0 (transparent)
# Keep in mind that blur doesn't play well with opacity animations.
```

## Known Quirks

- WebkitGTK web view is locked to 60hz (only on nvidia drivers?), which is very unfortunate. I'm _actively_ working on
  finding a workaround for this. I might opt for CEF in the future if I can't find a solution but it would be a pretty
  big undertaking.
- Since it's based on Webkit, CSS support is not as good as in Gecko or Chromium. Some CSS features may not work as
  expected, and you may need to use workarounds or polyfills for certain features. I recommend using something like
  TailwindCSS to alleviate this problem a bit.
- Realtime thumbnail preview for widgets like docks might be pretty hard to pull off as of right now. (The performance
  hit of sending video data through websockets might be too high). This is something that can be improved in the future.
  Native widget system is better for some things and that's just life.

## License

WSS is licensed under the [MIT License](LICENSE). You are free to use, modify, and distribute it as you wish, but please
keep the original license text in any copies or substantial portions of the software. Thank you!
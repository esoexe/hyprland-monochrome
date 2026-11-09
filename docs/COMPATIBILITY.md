# Compatibility and Validation

This project targets Hyprland systems and aims to adapt safely across common Linux distributions and hardware. This page separates what the repository can detect or configure from what has actually been exercised in automated tests. A hardware profile is not a claim that every machine in that category has been physically tested.

## Platform matrix

| Platform | Installer path | Project support status | Validation in this repository |
| --- | --- | --- | --- |
| Arch Linux | `pacman` | Primary target | Shell/config checks and isolated installer tests; CI runs on Ubuntu, not on a physical Arch desktop |
| EndeavourOS | `pacman` | Primary target | Same Arch-based package path; no dedicated hosted runner |
| CachyOS | `pacman` | Primary target | Same Arch-based package path; no dedicated hosted runner |
| Manjaro | `pacman` | Primary target | Same Arch-based package path; no dedicated hosted runner |
| Debian / Ubuntu | `apt-get` | Installer has a package path; Hyprland availability and version depend on the distro release | Installer sandbox and config checks; a real desktop session is not part of CI |
| Fedora | `dnf` | Installer has a package path; package names and Hyprland version depend on enabled repositories | Installer sandbox and config checks; a real desktop session is not part of CI |
| openSUSE | `zypper` | Installer has a package path; package names and Hyprland version depend on enabled repositories | Installer sandbox and config checks; a real desktop session is not part of CI |
| Other Linux distributions | `./install.sh --no-packages` | Config-only install may work if compatible Hyprland and runtime dependencies are already present | Not claimed as a tested distribution |

## Hardware profiles

| Hardware / session | Configuration path | What is automated | What still needs real-machine validation |
| --- | --- | --- | --- |
| AMD Radeon | Hardware probe and Hyprland environment/profile settings | Scripted config, parser, and fallback tests | GPU/display combinations, suspend/resume, multi-monitor behavior |
| Intel graphics | Hardware probe and compositor profile | Scripted config and fallback tests | Older iGPU generations, video decode, hybrid setups |
| NVIDIA | NVIDIA-specific environment and anti-flicker options | Presence and selection of the configuration path | Driver versions, Wayland behavior, external displays, resume behavior |
| Hybrid graphics / Optimus | Hardware probe and environment selection | Scripted selection logic | Per-laptop firmware and PRIME behavior |
| Virtual machines | Conservative software-rendering profile | Config path and installer sandbox | Hypervisor-specific input, GPU acceleration, and display resize |
| HiDPI / multiple monitors | Monitor module and scaling settings | Config generation and Lua syntax tests | Physical panel scaling, mixed-DPI placement, hotplug |

## Requirements and boundaries

- Hyprland **0.55 or newer** is required for this repository's Lua configuration format. The exact features available can vary with the installed Hyprland release.
- The normal install path expects a supported package manager (`pacman`, `apt-get`, `dnf`, or `zypper`) and asks before changing the system. `--no-packages` installs configuration files only.
- The SDDM login theme is optional. It does not install, enable, or replace a display manager automatically. It is intended for SDDM installations with the Qt Quick modules required by their greeter.
- Some features are optional and depend on external programs such as `awww`, `rofi`, `kitty`, `wf-recorder`, `gpu-screen-recorder`, PipeWire, and portals. Missing optional tools should not be interpreted as a working feature.
- The repository's automated tests use stubs and sandboxes where appropriate. They validate scripts and config logic, not the quality of a physical GPU driver or a live login session.

## Local validation

From the repository root:

```sh
make -j"$(nproc)"
bash test/run-all.sh
bash test/install-sandbox.sh
```

To preview the optional SDDM theme without changing the active login manager, see [First Login and Optional SDDM Theme](FIRST-LOGIN.md).

## Adding verified results

When adding a hardware or distro result, record the exact distro release, Hyprland version, GPU and driver, display-manager version, test date, and any deviations. Keep `configured` and `tested` as separate statuses. Do not mark a category tested based only on a passing shell smoke test.

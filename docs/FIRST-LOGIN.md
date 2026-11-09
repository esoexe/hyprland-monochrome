# First Login and Optional SDDM Theme

This guide covers the first start after installing Hyprland Monochrome. It does not create, rename, or change credentials for Linux users. The account menu manages only the currently logged-in user's own profile actions.

## Before starting

1. Read [Compatibility and Validation](COMPATIBILITY.md). Confirm that your distribution provides Hyprland 0.55 or newer and the packages you need.
2. Keep the installer's backup directory. The installer stores backups under `~/.config-backups/` and provides a restore script.
3. If you already use another display manager, keep using it. The optional SDDM theme below is only for machines already running SDDM.

## First Hyprland session

1. Select **Hyprland** from your existing display manager's session list. If you start from a TTY, use your distribution's supported session command.
2. Wait for the bar, wallpaper service, and launcher to start. If a component is missing, open a terminal and run:

   ```sh
   ~/.config/hypr/scripts/doctor.sh --plain
   ```

3. Open **Super + I** for Settings. Use **Account** to inspect the current user or start password, display-name, or login-shell changes in a terminal. These changes affect only that user and remain subject to the normal system tools' policy.
4. Open **Super + Shift + X** for Keyboard Studio and **Super + Ctrl + C** for Cursor Studio. The current values are also shown in the settings menus.
5. Use **Super + D** to launch applications and **Super + Tab** to lock the current session.
6. If this is an upgrade, test your normal applications and monitors before deleting the backup. Restore instructions are available in the installer's timestamped backup folder.

## Optional animated SDDM greeter

The repository includes a monochrome animated SDDM theme. Applying it changes the SDDM theme selection only; it does not install, enable, start, stop, or replace SDDM. Do not run this step if you use GDM, greetd, Ly, or another display manager.

1. Verify SDDM is already installed and that you can recover through a TTY or another administrator account.
2. Preview the theme first without changing system configuration:

   ```sh
   ./tools/install-sddm-theme.sh --preview
   ```

   The helper stages the correct Qt 5 or Qt 6 metadata and wallpaper for the installed greeter, then opens its test window. Preview mode does not authenticate or change the configured display manager.
3. Apply the theme only after you have reviewed it:

   ```sh
   sudo ./tools/install-sddm-theme.sh --install
   ```

   The installer backs up its own configuration file before writing a theme override. It does not edit your existing display-manager enablement or service state.
4. Reboot only when you are ready to test. Keep a TTY available. To undo the override:

   ```sh
   sudo ./tools/install-sddm-theme.sh --remove
   ```

The theme is an optional convenience, not a requirement for using Hyprland Monochrome. If a preview fails, leave your current SDDM configuration unchanged and use [Troubleshooting](TROUBLESHOOTING.md).

-- ═══════════════════════════════════════════════════════════════════════════
--  hardware.lua — Universal PC Hardware, Multi-GPU, VM & HiDPI Auto-Tuning
--
--  Reads Linux sysfs/procfs directly (zero subprocesses, <0.2 ms) so the exact
--  same configuration boots cleanly on:
--    • AMD Radeon / Intel Arc & Iris desktops and laptops
--    • NVIDIA RTX / GTX desktops (proprietary or nvidia-open)
--    • Hybrid Dual-GPU Optimus laptops (Intel+NVIDIA or AMD+NVIDIA)
--    • Virtual Machines & Cloud instances (QEMU/KVM, VirtIO, VMware, VirtualBox,
--      Hyper-V, UTM, Parallels) with automatic software-renderer fallback
--    • Any display density (1080p, 1440p, Ultrawide, 4K HiDPI) with crisp
--      zero-blur XWayland scaling
--
--  Optional per-machine overrides can be placed in `modules/hardware-user.lua`.
-- ═══════════════════════════════════════════════════════════════════════════

local M = {
    has_nvidia = false,
    has_amd    = false,
    has_intel  = false,
    is_vm      = false,
    is_hybrid  = false,
    is_laptop  = false,
}

local function read_first_line(path)
    local f = io.open(path, "r")
    if not f then return "" end
    local line = f:read("*l") or ""
    f:close()
    return line
end

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

-- 1. Probe PCI DRM GPU vendors (/sys/class/drm/card0..card3/device/vendor)
for idx = 0, 3 do
    local vendor = read_first_line(string.format("/sys/class/drm/card%d/device/vendor", idx)):lower()
    if vendor:find("0x10de", 1, true) then
        M.has_nvidia = true
    elseif vendor:find("0x1002", 1, true) then
        M.has_amd = true
    elseif vendor:find("0x8086", 1, true) then
        M.has_intel = true
    elseif vendor:find("0x1af4", 1, true) or vendor:find("0x15ad", 1, true) or vendor:find("0x80ee", 1, true) or vendor:find("0x1b36", 1, true) then
        M.is_vm = true
    end
end

if file_exists("/proc/driver/nvidia/version") then
    M.has_nvidia = true
end

M.is_hybrid = M.has_nvidia and (M.has_intel or M.has_amd)

-- 2. Probe DMI chassis & hypervisor signatures
local dmi = (
    read_first_line("/sys/class/dmi/id/sys_vendor") .. " " ..
    read_first_line("/sys/class/dmi/id/product_name") .. " " ..
    read_first_line("/sys/class/dmi/id/board_name")
):lower()

for _, sig in ipairs({ "qemu", "kvm", "vmware", "virtualbox", "innotek", "bochs", "parallels", "xen", "amazon ec2", "google compute" }) do
    if dmi:find(sig, 1, true) then
        M.is_vm = true
        break
    end
end

-- Laptop chassis types in SMBIOS: 8=Portable, 9=Laptop, 10=Notebook, 14=Sub-Notebook, 31=Convertible, 32=Detachable
local chassis = read_first_line("/sys/class/dmi/id/chassis_type")
if chassis == "8" or chassis == "9" or chassis == "10" or chassis == "14" or chassis == "31" or chassis == "32"
   or file_exists("/sys/class/power_supply/BAT0/status")
   or file_exists("/sys/class/power_supply/BAT1/status") then
    M.is_laptop = true
end

-- 3. Universal XWayland + Cursor + OpenGL safety baseline (works on every GPU)
hl.config({
    xwayland = {
        enabled            = true,
        force_zero_scaling = true,
        use_nearest_neighbor = false,
    },
    cursor = {
        -- 2 = Hyprland 'auto': HW cursors enabled on AMD/Intel, auto-disabled on NVIDIA
        no_hardware_cursors = M.is_vm and 1 or 2,
        enable_hyprcursor   = true,
    },
    opengl = {
        nvidia_anti_flicker = true,
    },
})

-- 4. GPU-specific & VM-specific environment tuning
if M.is_vm then
    hl.env("WLR_RENDERER_ALLOW_SOFTWARE", "1")
    hl.env("AQ_NO_MODIFIERS", "1")
    hl.env("LIBGL_ALWAYS_SOFTWARE", os.getenv("LIBGL_ALWAYS_SOFTWARE") or "0")
    hl.config({
        decoration = {
            blur   = { enabled = false },
            shadow = { enabled = false },
        },
        misc = { vrr = 0 },
    })
elseif M.has_nvidia then
    hl.env("LIBVA_DRIVER_NAME", "nvidia")
    hl.env("__GLX_VENDOR_LIBRARY_NAME", "nvidia")
    hl.env("NVD_BACKEND", "direct")
    hl.env("MOZ_DISABLE_RDD_SANDBOX", "1")
    if M.is_hybrid then
        -- Hybrid Optimus laptop: prevent external HDMI/DP ports wired to dGPU from stalling
        hl.env("AQ_FORCE_LINEAR_BLIT", "0")
    else
        hl.env("GBM_BACKEND", "nvidia-drm")
    end
elseif M.has_intel and not M.has_amd then
    if file_exists("/usr/lib/dri/iHD_drv_video.so") or file_exists("/usr/lib/x86_64-linux-gnu/dri/iHD_drv_video.so") then
        hl.env("LIBVA_DRIVER_NAME", "iHD")
    end
end

-- 5. Optional per-machine override file (`~/.config/hypr/modules/hardware-user.lua`)
pcall(require, "modules/hardware-user")

return M

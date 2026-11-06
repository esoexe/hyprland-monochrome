-- ═══════════════════════════════════════════════════════════════════════════
--  ~/.config/nvim/init.lua — hyprland-monochrome minimal zero-plugin config
-- ═══════════════════════════════════════════════════════════════════════════

vim.opt.termguicolors  = true
vim.opt.number         = true
vim.opt.relativenumber = true
vim.opt.cursorline     = true
vim.opt.signcolumn     = "yes"
vim.opt.expandtab      = true
vim.opt.shiftwidth     = 4
vim.opt.tabstop        = 4
vim.opt.smartindent    = true
vim.opt.ignorecase     = true
vim.opt.smartcase      = true
vim.opt.scrolloff      = 6
vim.opt.updatetime     = 200
vim.opt.splitright     = true
vim.opt.splitbelow     = true
vim.opt.clipboard      = "unnamedplus"
vim.opt.laststatus     = 2
vim.opt.showmode       = false
vim.opt.statusline     = "  %#TabLineSel# %{toupper(mode())} %#StatusLine#  %f %m %= %y  %l:%c  %p%%  "

pcall(vim.cmd.colorscheme, "monochrome")

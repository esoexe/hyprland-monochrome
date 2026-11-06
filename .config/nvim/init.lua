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
pcall(function() vim.opt.smoothscroll = true end)
vim.opt.statusline     = "  %#TabLineSel# %{toupper(mode())} %#StatusLine#  %f %m %= %y  %l:%c  %p%%  "

-- Crisp centered viewport glides on half-page / search jumps
vim.keymap.set("n", "<C-d>", "<C-d>zz", { silent = true })
vim.keymap.set("n", "<C-u>", "<C-u>zz", { silent = true })
vim.keymap.set("n", "n",     "nzzzv",   { silent = true })
vim.keymap.set("n", "N",     "Nzzzv",   { silent = true })

-- Monochrome silver yank pulse (160ms flash when copying lines/words)
vim.api.nvim_create_autocmd("TextYankPost", {
    callback = function()
        local yk = vim.hl and vim.hl.on_yank or vim.highlight.on_yank
        pcall(yk, { higroup = "IncSearch", timeout = 160 })
    end,
})

pcall(vim.cmd.colorscheme, "monochrome")

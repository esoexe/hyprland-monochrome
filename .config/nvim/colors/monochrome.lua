-- ═══════════════════════════════════════════════════════════════════════════
--  ~/.config/nvim/colors/monochrome.lua — typographic greyscale colorscheme
--  Syntax hierarchy relies on luminance, weight, and italics — zero rainbow.
-- ═══════════════════════════════════════════════════════════════════════════

vim.cmd("highlight clear")
if vim.fn.exists("syntax_on") == 1 then
    vim.cmd("syntax reset")
end
vim.o.background = "dark"
vim.g.colors_name = "monochrome"

local hi = function(group, opts)
    vim.api.nvim_set_hl(0, group, opts)
end

-- Core UI surfaces (transparent bg so Kitty's blurred glass shines through)
hi("Normal",       { fg = "#e6e6e6", bg = "NONE" })
hi("NormalNC",     { fg = "#b8b8b8", bg = "NONE" })
hi("NormalFloat",  { fg = "#e6e6e6", bg = "#101010" })
hi("FloatBorder",  { fg = "#3a3a3a", bg = "#101010" })
hi("FloatTitle",   { fg = "#ffffff", bg = "#101010", bold = true })
hi("CursorLine",   { bg = "#141414" })
hi("CursorColumn", { bg = "#141414" })
hi("ColorColumn",  { bg = "#121212" })
hi("LineNr",       { fg = "#4a4a4a" })
hi("CursorLineNr", { fg = "#ffffff", bold = true })
hi("SignColumn",   { fg = "#6e6e6e", bg = "NONE" })
hi("VertSplit",    { fg = "#242424", bg = "NONE" })
hi("WinSeparator", { fg = "#242424", bg = "NONE" })
hi("EndOfBuffer",  { fg = "#1a1a1a" })

-- Selection, search & completion (inverted silver/obsidian)
hi("Visual",       { fg = "#0a0a0a", bg = "#e6e6e6" })
hi("Search",       { fg = "#0a0a0a", bg = "#bdbdbd" })
hi("IncSearch",    { fg = "#0a0a0a", bg = "#ffffff", bold = true })
hi("CurSearch",    { fg = "#0a0a0a", bg = "#ffffff", bold = true })
hi("MatchParen",   { fg = "#ffffff", bg = "#2c2c2c", bold = true })
hi("Pmenu",        { fg = "#d0d0d0", bg = "#111111" })
hi("PmenuSel",     { fg = "#0a0a0a", bg = "#e6e6e6", bold = true })
hi("PmenuSbar",    { bg = "#161616" })
hi("PmenuThumb",   { bg = "#4a4a4a" })

-- Statusline & tabs
hi("StatusLine",   { fg = "#e6e6e6", bg = "#121212" })
hi("StatusLineNC", { fg = "#6e6e6e", bg = "#0c0c0c" })
hi("TabLine",      { fg = "#7a7a7a", bg = "#0e0e0e" })
hi("TabLineSel",   { fg = "#0a0a0a", bg = "#e6e6e6", bold = true })
hi("TabLineFill",  { bg = "#0a0a0a" })
hi("Directory",    { fg = "#ffffff", bold = true })
hi("Title",        { fg = "#ffffff", bold = true })
hi("NonText",      { fg = "#333333" })
hi("SpecialKey",   { fg = "#444444" })

-- Typographic syntax ramp
hi("Comment",      { fg = "#686868", italic = true })
hi("Constant",     { fg = "#f0f0f0" })
hi("String",       { fg = "#b8b8b8", italic = true })
hi("Character",    { fg = "#c4c4c4" })
hi("Number",       { fg = "#ffffff" })
hi("Boolean",      { fg = "#ffffff", bold = true })
hi("Identifier",   { fg = "#e0e0e0" })
hi("Function",     { fg = "#ffffff", bold = true })
hi("Statement",    { fg = "#ffffff", bold = true })
hi("Conditional",  { fg = "#ffffff", bold = true })
hi("Repeat",       { fg = "#ffffff", bold = true })
hi("Keyword",      { fg = "#ffffff", bold = true })
hi("Operator",     { fg = "#a8a8a8" })
hi("PreProc",      { fg = "#d0d0d0" })
hi("Type",         { fg = "#dcdcdc", bold = true })
hi("Special",      { fg = "#f4f4f4" })
hi("Delimiter",    { fg = "#8e8e8e" })
hi("Todo",         { fg = "#0a0a0a", bg = "#e6e6e6", bold = true })
hi("Error",        { fg = "#ffffff", bg = "#2a2a2a", bold = true, underline = true })

-- Treesitter & LSP diagnostics (pure luminance + underline)
hi("@variable",           { fg = "#e2e2e2" })
hi("@property",           { fg = "#cccccc" })
hi("@function.call",      { fg = "#f4f4f4" })
hi("@keyword.return",     { fg = "#ffffff", bold = true })
hi("@punctuation.bracket",{ fg = "#8a8a8a" })
hi("DiagnosticError",     { fg = "#ffffff", bold = true })
hi("DiagnosticWarn",      { fg = "#bdbdbd" })
hi("DiagnosticInfo",      { fg = "#969696" })
hi("DiagnosticHint",      { fg = "#727272" })
hi("DiagnosticUnderlineError", { sp = "#ffffff", undercurl = true })
hi("DiagnosticUnderlineWarn",  { sp = "#9e9e9e", underline = true })
hi("DiffAdd",    { fg = "#ffffff", bg = "#1c1c1c" })
hi("DiffChange", { fg = "#cccccc", bg = "#161616" })
hi("DiffDelete", { fg = "#5a5a5a", bg = "#111111" })

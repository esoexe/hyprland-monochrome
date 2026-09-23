# ═══════════════════════════════════════════════════════════════════════════
#  ~/.zshrc — hyprland-monochrome
#
#  Zsh + Starship + the usual quality-of-life plugins.
#  Plugins are the Arch packages (already in packages.txt):
#     zsh-autosuggestions · zsh-syntax-highlighting · fzf
#
#  Reload after editing:  source ~/.zshrc
# ═══════════════════════════════════════════════════════════════════════════

# ── Environment ───────────────────────────────────────────────────────────
export EDITOR="${EDITOR:-nvim}"
export VISUAL="$EDITOR"
export PAGER="${PAGER:-less}"
export MANPAGER="less -R"
export LESS="-R -F -X"
export KEYTIMEOUT=1                     # shorter Esc delay for vi-style binds
export STARSHIP_CONFIG="$HOME/.config/starship.toml"

# Keep history tidy across many terminals
export HISTFILE="$HOME/.zsh_history"
export HISTSIZE=50000
export SAVEHIST=50000

# Make GTK/Qt and Electron apps feel native on Wayland by default
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland;xcb}"

# ── History ───────────────────────────────────────────────────────────────
setopt HIST_IGNORE_ALL_DUPS      # no repeated entries
setopt HIST_IGNORE_SPACE         # a leading space keeps a command out of history
setopt HIST_REDUCE_BLANKS
setopt SHARE_HISTORY             # every terminal writes to the same history
setopt INC_APPEND_HISTORY
setopt EXTENDED_HISTORY          # timestamps

# ── Shell behaviour ───────────────────────────────────────────────────────
setopt AUTO_CD                   # typing a folder name cd's into it
setopt AUTO_PUSHD PUSHD_IGNORE_DUPS PUSHD_SILENT
setopt INTERACTIVE_COMMENTS
setopt NO_BEEP
setopt EXTENDED_GLOB
setopt GLOB_DOTS
setopt PROMPT_SUBST
setopt TRANSIENT_RPROMPT         # right prompt disappears after you type
unsetopt FLOW_CONTROL            # Ctrl+S / Ctrl+Q stay usable
WORDCHARS='*?_-.[]~&;!#$%^(){}<>'

# ── Completion ────────────────────────────────────────────────────────────
autoload -Uz compinit
if [[ -n ${ZDOTDIR:-$HOME}/.zcompdump(#qN.mh+24) ]]; then
    compinit -C
else
    compinit
fi
zstyle ':completion:*' menu select
zstyle ':completion:*' matcher-list 'm:{a-zA-Z}={A-Za-z}'      # case-insensitive
zstyle ':completion:*' list-colors "${(s.:.)LS_COLORS}"
zstyle ':completion:*' group-name ''
zstyle ':completion:*:descriptions' format '%F{8}── %d%f'
zstyle ':completion:*:warnings' format '%F{8}no matches%f'
zstyle ':completion:*' use-cache on
zstyle ':completion:*' cache-path "$HOME/.cache/zsh/compcache"
zstyle ':completion:*:processes' command 'ps -u $USER -o pid,stat,%cpu,cmd'
zstyle ':completion:*:*:kill:*:processes' force-list always
zstyle ':fzf-tab:*' fzf-flags --color=mono:16 --height=60%

# ── Key bindings ──────────────────────────────────────────────────────────
bindkey -e                                        # emacs mode
autoload -Uz up-line-or-beginning-search down-line-or-beginning-search
zle -N up-line-or-beginning-search
zle -N down-line-or-beginning-search
bindkey '^[[A' up-line-or-beginning-search       # ↑ searches history by prefix
bindkey '^[[B' down-line-or-beginning-search
bindkey '^[[1;5C' forward-word                    # Ctrl + Right
bindkey '^[[1;5D' backward-word                   # Ctrl + Left
bindkey '^[[H'   beginning-of-line
bindkey '^[[F'   end-of-line
bindkey '^ '     autosuggest-accept               # Ctrl+Space accepts a suggestion

# ── Plugins ───────────────────────────────────────────────────────────────
# zsh-autosuggestions: grey ghost text from your history (→ accepts the rest)
if [[ -f /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh ]]; then
    ZSH_AUTOSUGGEST_HIGHLIGHT_STYLE='fg=#4a4a4a'
    ZSH_AUTOSUGGEST_STRATEGY=(history completion)
    source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh
fi

# fzf: Ctrl+R history search, Ctrl+T files, Alt+C cd
if [[ -f /usr/share/fzf/key-bindings.zsh ]]; then
    export FZF_DEFAULT_OPTS="--height 40% --layout=reverse --border --prompt='  ' \
--color=bg+:#1a1a1a,bg:#0d0d0d,spinner:#ffffff,hl:#ffffff,fg:#e6e6e6 \
--color=header:#8a8a8a,info:#8a8a8a,pointer:#ffffff,marker:#ffffff,fg+:#ffffff \
--color=prompt:#8a8a8a,hl+:#ffffff,border:#2a2a2a"
    export FZF_DEFAULT_COMMAND='fd --type f --hidden --follow --exclude .git 2>/dev/null || find . -type f -not -path "*/\.git/*"'
    export FZF_CTRL_T_COMMAND="$FZF_DEFAULT_COMMAND"
    source /usr/share/fzf/key-bindings.zsh
    [[ -f /usr/share/fzf/completion.zsh ]] && source /usr/share/fzf/completion.zsh
fi

# zsh-syntax-highlighting must be loaded last
if [[ -f /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh ]]; then
    typeset -A ZSH_HIGHLIGHT_STYLES
    ZSH_HIGHLIGHT_STYLES[command]='fg=#ffffff,bold'
    ZSH_HIGHLIGHT_STYLES[builtin]='fg=#e6e6e6,bold'
    ZSH_HIGHLIGHT_STYLES[alias]='fg=#c8c8c8'
    ZSH_HIGHLIGHT_STYLES[path]='fg=#bdbdbd'
    ZSH_HIGHLIGHT_STYLES[globbing]='fg=#9a9a9a'
    ZSH_HIGHLIGHT_STYLES[unknown-token]='fg=#ffffff,underline'
    ZSH_HIGHLIGHT_STYLES[comment]='fg=#5a5a5a'
    source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh
fi

# ── Prompt (Starship) ─────────────────────────────────────────────────────
if command -v starship >/dev/null 2>&1; then
    eval "$(starship init zsh)"
fi

# ── Aliases ───────────────────────────────────────────────────────────────
# Modern replacements (installed by packages.txt)
if command -v eza >/dev/null 2>&1; then
    alias ls='eza --group-directories-first --icons=never'
    alias ll='eza -lh --group-directories-first --git --icons=never'
    alias la='eza -lah --group-directories-first --git --icons=never'
    alias lt='eza --tree --level=2 --icons=never'
    alias lsize='eza -lh --sort=size --reverse | head -20'
else
    alias ls='ls --color=auto'
    alias ll='ls -lh'
    alias la='ls -lah'
fi
command -v bat >/dev/null 2>&1 && {
    alias cat='bat --style=plain --pager=never'
    alias catn='bat --style=numbers --pager=never'
}

# Navigation
alias ..='cd ..'
alias ...='cd ../..'
alias ....='cd ../../..'
alias ~='cd ~'
alias -- -='cd -'
alias c='clear'
alias reload='source ~/.zshrc && echo "zsh config reloaded"'

# Hyprland / setup helpers
alias hr='hyprctl reload && echo "hyprland reloaded"'
alias hb='hyprctl binds | less'
alias hc='hyprctl clients | less'
alias hm='hyprctl monitors'
alias hlog='hyprctl rollinglog'
alias conf='cd ~/.config/hypr && $EDITOR .'
alias dots='cd ~/.config && $EDITOR .'
alias walls='~/.config/hypr/scripts/wallpaper.sh pick'
alias settings='~/.config/hypr/scripts/settings.sh'
alias keys='~/.config/hypr/scripts/menu.sh keybinds'
alias lock='hyprlock'
alias shot='~/.config/hypr/scripts/screenshot.sh region'

# Media
alias play='playerctl play-pause'
alias next='playerctl next'
alias prev='playerctl previous'
alias vol='~/.config/hypr/scripts/volume.sh status'

# Pacman & friends — the everyday Arch commands, spelled short
alias pac='sudo pacman'
alias pacs='pacman -Ss'
alias paci='sudo pacman -S'
alias pacr='sudo pacman -Rns'
alias pacu='sudo pacman -Syu'
alias pacq='pacman -Q'
alias pacown='pacman -Qo'
alias pacclean='sudo pacman -Rns $(pacman -Qtdq) 2>/dev/null; sudo paccache -rk1'
alias orphan='pacman -Qtdq'
alias mirrors='sudo reflector --latest 20 --sort rate --save /etc/pacman.d/mirrorlist'
alias services='systemctl list-units --type=service --state=running'
alias logs='journalctl -p 3 -xb'
alias meminfo='free -h && echo && ps -eo pid,ppid,cmd,%mem,%cpu --sort=-%mem | head -12'
alias ports='ss -tulpn'
alias myip='curl -s ifconfig.me; echo'

# Git
alias gs='git status -sb'
alias ga='git add'
alias gaa='git add -A'
alias gc='git commit -m'
alias gca='git commit --amend --no-edit'
alias gp='git push'
alias gpl='git pull --rebase'
alias gd='git diff'
alias gds='git diff --staged'
alias gl='git log --oneline --graph --decorate -20'
alias gco='git checkout'
alias gb='git branch -vv'
alias gnew='git switch -c'

# ── Functions ─────────────────────────────────────────────────────────────
mkcd()   { mkdir -p "$1" && cd "$1" || return 1; }
extract() {                                  # extract almost anything
    [ -f "$1" ] || { echo "usage: extract <archive>"; return 1; }
    case "$1" in
        *.tar.bz2|*.tbz2) tar xjf "$1" ;;
        *.tar.gz|*.tgz)   tar xzf "$1" ;;
        *.tar.xz|*.txz)   tar xJf "$1" ;;
        *.tar.zst)        tar --zstd -xf "$1" ;;
        *.tar)            tar xf "$1" ;;
        *.zip)            unzip "$1" ;;
        *.rar)            unrar x "$1" ;;
        *.7z)             7z x "$1" ;;
        *.gz)             gunzip "$1" ;;
        *)                echo "don't know how to extract '$1'" >&2; return 1 ;;
    esac
}
targz()  { tar -czf "${1%/}.tar.gz" "$1"; }
serve()  { python3 -m http.server "${1:-8000}"; }        # quick file server
weather(){ curl -s "wttr.in/${1:-Bechar}?format=3"; echo; }
note()   { ${EDITOR:-nano} "$HOME/Notes/$(date +%Y-%m-%d)-${1:-note}.md"; }
fkill()  { ps -u "$USER" -o pid,cmd | fzf -m | awk '{print $1}' | xargs -r kill -9; }
gend()   { openssl rand -base64 "${1:-24}"; }
bkp()    { cp -r "$1" "$1.bak-$(date +%Y%m%d-%H%M%S)" && echo "backed up $1"; }
# mkdir + cd with tab-completion
compdef _mkdir mkcd 2>/dev/null || true

# ── Autostart helpers ─────────────────────────────────────────────────────
# Print the current wallpaper / hyprsunset state on demand
wallinfo() { hyprctl hyprsunset profile 2>/dev/null; ~/.config/hypr/scripts/wallpaper.sh --help 2>/dev/null; }

# ── Welcome ───────────────────────────────────────────────────────────────
if [[ -o interactive ]] && command -v fastfetch >/dev/null 2>&1; then
    [[ "${MONO_NO_BANNER:-0}" = "1" ]] || fastfetch --config ~/.config/fastfetch/config.jsonc 2>/dev/null
fi

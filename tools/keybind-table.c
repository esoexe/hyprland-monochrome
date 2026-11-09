// ═══════════════════════════════════════════════════════════════════════════
//  tools/keybind-table.c — Generate docs/KEYBINDS.md from binds.lua (Pure C11)
//
//    ./bin/keybind-table            # rewrite docs/KEYBINDS.md
//    ./bin/keybind-table --check    # exit 1 if out of date
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "../include/keybind_table.h"

int main(int argc, char *argv[]) {
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--check") == 0) check_only = true;
    }

    char repo[PATH_MAX], binds_path[PATH_MAX], out_path[PATH_MAX];
    mono_find_repo_root(argv[0], repo, sizeof(repo));
    mono_path_join(binds_path, sizeof(binds_path), repo, ".config/hypr/modules/binds.lua");
    mono_path_join(out_path, sizeof(out_path), repo, "docs/KEYBINDS.md");

    char *binds_txt = mono_read_file(binds_path);
    KbDoc doc = kb_parse(binds_txt, repo);
    free(binds_txt);

    char *rendered = kb_render_markdown(&doc);

    if (check_only) {
        int rc = 1;
        if (mono_file_exists(out_path)) {
            char *cur = mono_read_file(out_path);
            if (strcmp(cur, rendered) == 0) {
                printf("docs/KEYBINDS.md is up to date.\n");
                rc = 0;
            }
            free(cur);
        }
        if (rc != 0) {
            fprintf(stderr, "docs/KEYBINDS.md is OUT OF DATE — run make docs (./bin/keybind-table)\n");
        }
        free(rendered);
        kb_doc_free(&doc);
        return rc;
    }

    mono_write_file(out_path, rendered);
    size_t total = 0;
    for (size_t i = 0; i < doc.len; ++i) total += doc.secs[i].rows.len;
    printf("wrote docs/KEYBINDS.md — %zu rows in %zu sections\n", total, doc.len);

    free(rendered);
    kb_doc_free(&doc);
    return 0;
}

#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "font8x8.h"

#define THEMES_DIR    "/_pico/themes"
#define SETTINGS_FILE "/_pico/settings.json"
#define MAX_THEMES    64
#define MAX_NAME_LEN  128

#define COL_BG       RGB15(3, 3, 3)
#define COL_CARD     RGB15(6, 6, 6)
#define COL_CARD_SEL RGB15(9, 9, 9)
#define COL_TITLE    RGB15(31, 31, 31)
#define COL_SELECT   RGB15(10, 31, 10)
#define COL_NORMAL   RGB15(25, 25, 25)
#define COL_CURRENT  RGB15(5, 25, 31)
#define COL_ERROR    RGB15(31, 10, 10)
#define COL_OK       RGB15(10, 31, 10)
#define COL_DIM      RGB15(15, 15, 15)

static char  themes[MAX_THEMES][MAX_NAME_LEN];
static int   theme_count  = 0;
static int   cursor       = 0;
static char  current_theme[MAX_NAME_LEN] = "";
static u16*  vramMain = NULL;
static u16*  vramSub  = NULL;

static void draw_rect(u16* vram, int x, int y, int w, int h, u16 color) {
    if (!vram) return;
    u16 c = color | BIT(15);
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= 192) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix < 0 || ix >= 256) continue;
            vram[iy * 256 + ix] = c;
        }
    }
}

static void draw_char(u16* vram, int x, int y, char c, u16 color) {
    if (!vram || c < 32 || c > 127) return;
    int idx = c - 32;
    u16 c_alpha = color | BIT(15);
    for (int iy = 0; iy < 8; iy++) {
        int row = font8x8[idx][iy];
        for (int ix = 0; ix < 8; ix++) {
            if (row & (1 << (7 - ix))) {
                if (x+ix >= 0 && x+ix < 256 && y+iy >= 0 && y+iy < 192) {
                    vram[(y+iy) * 256 + (x+ix)] = c_alpha;
                }
            }
        }
    }
}

static void draw_string(u16* vram, int x, int y, const char* str, u16 color) {
    if (!vram) return;
    while (*str) {
        draw_char(vram, x, y, *str, color);
        x += 8;
        str++;
    }
}

static void clear_screen(u16* vram, u16 color) {
    if (!vram) return;
    u16 c = color | BIT(15);
    for(int i = 0; i < 256 * 192; i++) {
        vram[i] = c;
    }
}

static void read_current_theme(void) {
    FILE *f = fopen(SETTINGS_FILE, "rb");
    if (!f) return;

    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    char *key = strstr(buf, "\"theme\"");
    if (!key) return;

    char *colon = strchr(key + 7, ':');
    if (!colon) return;

    char *open_q = strchr(colon + 1, '"');
    if (!open_q) return;

    char *close_q = strchr(open_q + 1, '"');
    if (!close_q) return;

    size_t len = close_q - open_q - 1;
    if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
    strncpy(current_theme, open_q + 1, len);
    current_theme[len] = '\0';
}

static int load_themes(void) {
    DIR *dir = opendir(THEMES_DIR);
    if (!dir) return 0;

    struct dirent *entry;
    theme_count = 0;

    while ((entry = readdir(dir)) != NULL && theme_count < MAX_THEMES) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", THEMES_DIR, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(themes[theme_count], entry->d_name, MAX_NAME_LEN - 1);
            themes[theme_count][MAX_NAME_LEN - 1] = '\0';
            theme_count++;
        }
    }
    closedir(dir);
    return theme_count;
}

static int apply_theme(const char *new_theme) {
    FILE *f = fopen(SETTINGS_FILE, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); return -2; }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    char *key = strstr(buf, "\"theme\"");
    if (!key) { free(buf); return -3; }

    char *colon = strchr(key + 7, ':');
    if (!colon) { free(buf); return -3; }

    char *open_q = strchr(colon + 1, '"');
    if (!open_q) { free(buf); return -3; }

    char *close_q = strchr(open_q + 1, '"');
    if (!close_q) { free(buf); return -3; }

    size_t prefix_len = open_q + 1 - buf;
    size_t suffix_len = size - (close_q - buf);
    size_t new_theme_len = strlen(new_theme);
    size_t new_size = prefix_len + new_theme_len + suffix_len;

    char *new_buf = (char *)malloc(new_size + 1);
    if (!new_buf) { free(buf); return -2; }

    memcpy(new_buf, buf, prefix_len);
    memcpy(new_buf + prefix_len, new_theme, new_theme_len);
    memcpy(new_buf + prefix_len + new_theme_len, close_q, suffix_len);
    new_buf[new_size] = '\0';

    free(buf);

    f = fopen(SETTINGS_FILE, "wb");
    if (!f) { free(new_buf); return -4; }

    fwrite(new_buf, 1, new_size, f);
    fclose(f);
    free(new_buf);

    return 0;
}

static void draw_ui(void) {
    clear_screen(vramMain, COL_BG);

    draw_rect(vramMain, 0, 0, 256, 24, COL_CARD);
    draw_string(vramMain, 8, 8, "PICO THEME SWITCHER", COL_SELECT);

    draw_string(vramMain, 8, 30, "Active Theme:", COL_DIM);
    draw_string(vramMain, 116, 30, current_theme[0] ? current_theme : "None", COL_CURRENT);
    draw_rect(vramMain, 8, 44, 240, 1, COL_CARD);

    if (theme_count == 0) {
        draw_string(vramMain, 8, 64, "No themes found in /_pico/themes", COL_ERROR);
        draw_string(vramMain, 8, 80, "Press START to exit.", COL_DIM);
        return;
    }

    int max_visible = 10;
    int start = 0;
    if (cursor >= max_visible) start = cursor - max_visible + 1;

    for (int i = start; i < theme_count && i < start + max_visible; i++) {
        int y = 52 + (i - start) * 12;
        bool is_cursor  = (i == cursor);
        bool is_current = (strcmp(themes[i], current_theme) == 0);

        if (is_cursor) {
            draw_rect(vramMain, 4, y - 2, 248, 12, COL_CARD_SEL);
            draw_string(vramMain, 8, y, ">", COL_SELECT);
        }

        u16 text_color = is_current ? COL_CURRENT : COL_NORMAL;
        if (is_cursor && !is_current) text_color = COL_TITLE;

        draw_string(vramMain, 24, y, themes[i], text_color);
        
        if (is_current) {
            draw_string(vramMain, 24 + strlen(themes[i])*8 + 8, y, "(Active)", COL_DIM);
        }
    }

    draw_rect(vramMain, 0, 192 - 16, 256, 16, COL_CARD);
    draw_string(vramMain, 8, 192 - 12, "U/D:Sel  A:Apply  START:Exit", COL_DIM);
}

int main(void) {
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

    int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bg3Sub  = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramMain = (u16*)bgGetGfxPtr(bg3Main);
    vramSub  = (u16*)bgGetGfxPtr(bg3Sub);

    clear_screen(vramSub, COL_BG);
    draw_rect(vramSub, 16, 16, 224, 160, COL_CARD);
    draw_string(vramSub, 32, 32, "PICO THEME SWITCHER", COL_DIM);
    draw_string(vramSub, 32, 56, "Select a theme on the", COL_NORMAL);
    draw_string(vramSub, 32, 68, "top screen and press A", COL_NORMAL);
    draw_string(vramSub, 32, 80, "to apply it.", COL_NORMAL);
    draw_string(vramSub, 32, 110, "by Nogami", COL_CURRENT);

    if (!fatInitDefault()) {
        clear_screen(vramMain, COL_BG);
        draw_string(vramMain, 8, 8, "FAT init failed!", COL_ERROR);
        draw_string(vramMain, 8, 24, "Make sure DLDI is patched.", COL_NORMAL);
        while (1) {
            swiWaitForVBlank();
            scanKeys();
            if (keysDown() & KEY_START) return 0;
        }
    }

    read_current_theme();
    load_themes();

    for (int i = 0; i < theme_count; i++) {
        if (strcmp(themes[i], current_theme) == 0) {
            cursor = i;
            break;
        }
    }

    draw_ui();

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        u32 keys = keysDown();

        if (keys & KEY_START) break;

        if (theme_count > 0) {
            if (keys & KEY_UP) {
                cursor = (cursor - 1 + theme_count) % theme_count;
                draw_ui();
            }
            if (keys & KEY_DOWN) {
                cursor = (cursor + 1) % theme_count;
                draw_ui();
            }
            if (keys & KEY_A) {
                clear_screen(vramMain, COL_BG);
                draw_rect(vramMain, 0, 0, 256, 24, COL_CARD);
                draw_string(vramMain, 8, 8, "APPLYING THEME...", COL_SELECT);
                
                draw_string(vramMain, 8, 40, themes[cursor], COL_TITLE);

                int result = apply_theme(themes[cursor]);

                if (result == 0) {
                    strncpy(current_theme, themes[cursor], MAX_NAME_LEN - 1);
                    current_theme[MAX_NAME_LEN - 1] = '\0';
                    draw_string(vramMain, 8, 64, "Success! Theme applied.", COL_OK);
                    draw_string(vramMain, 8, 80, "Restart PicoLauncher to see it.", COL_NORMAL);
                } else {
                    char err[64];
                    snprintf(err, sizeof(err), "Error %d writing settings!", result);
                    draw_string(vramMain, 8, 64, err, COL_ERROR);
                }

                draw_rect(vramMain, 0, 192 - 16, 256, 16, COL_CARD);
                draw_string(vramMain, 8, 192 - 12, "Press any key to continue.", COL_DIM);

                while (1) {
                    swiWaitForVBlank();
                    scanKeys();
                    if (keysDown()) break;
                }
                draw_ui();
            }
        }
    }

    return 0;
}

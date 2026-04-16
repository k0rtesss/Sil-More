/* File: init-style.c */

#include "angband.h"
#include "fs/file.h"
#include "fs/path.h"
#include "init.h"
#include "init/init-parse-internal.h"
#include "log/log.h"

#ifdef ALLOW_TEMPLATES

static style_type* stl_ptr = NULL;
static byte g_default_vein_row = 19;
static byte g_default_vein_col = 0;
static bool g_overlay_key_enabled = false;
static byte g_overlay_key_r = 255;
static byte g_overlay_key_g = 0;
static byte g_overlay_key_b = 255;

static errr parse_style_short_desc_line(char* buf);
static errr parse_style_m1_line(char* buf);
static errr parse_style_m2_line(char* buf);
static errr parse_style_message_line(char* buf);
static int parse_partition_style_kind(const char* tok);
static int parse_big_cave_weight_token(const char* tok);
static char* trim_narrative_text(char* s);
static int parse_narrative_idx(char* colon_start, char** out_text);

byte get_default_vein_row(void) { return g_default_vein_row; }
byte get_default_vein_col(void) { return g_default_vein_col; }
bool get_overlay_key_enabled(void) { return g_overlay_key_enabled; }
void get_overlay_key_rgb(byte* r, byte* g, byte* b)
{
    if (r)
        *r = g_overlay_key_r;
    if (g)
        *g = g_overlay_key_g;
    if (b)
        *b = g_overlay_key_b;
}

errr parse_style_info(char* buf, header* head)
{
    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t')
            p++;
        if (((p[0] == 'D' || p[0] == 'd') && (p[1] == 'Y' || p[1] == 'y')
                && p[2] == ':')
            || ((p[0] == 'E' || p[0] == 'e') && p[1] == ':'))
        {
            const char* q = (p[0] == 'E' || p[0] == 'e') ? (p + 2) : (p + 3);
            int r;
            int c;

            if (2 != sscanf(q, "%d:%d", &r, &c))
                return PARSE_ERROR_GENERIC;

            g_default_vein_row = (byte)r;
            g_default_vein_col = (byte)c;
            return 0;
        }
    }

    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t')
            p++;
        if ((p[0] == 'E' || p[0] == 'e') && (p[1] == 'K' || p[1] == 'k')
            && p[2] == ':')
        {
            int r;
            int g;
            int b;

            if (3 != sscanf(p + 3, "%d:%d:%d", &r, &g, &b))
                return PARSE_ERROR_GENERIC;

            g_overlay_key_r = (byte)r;
            g_overlay_key_g = (byte)g;
            g_overlay_key_b = (byte)b;
            g_overlay_key_enabled = true;
            return 0;
        }
    }

    if (buf[0] == 'N')
    {
        int idx;
        char* s = strchr(buf + 2, ':');

        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        idx = atoi(buf + 2);

        if (idx <= error_idx)
            return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num)
            return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

        stl_ptr = ((style_type*)head->info_ptr) + idx;
        memset(stl_ptr, 0, sizeof(style_type));
        stl_ptr->floor_count = 0;
        stl_ptr->door_count = 0;
        stl_ptr->name = add_name(head, s);
        return 0;
    }

    if (!stl_ptr)
        return PARSE_ERROR_MISSING_RECORD_HEADER;

    if (buf[0] == 'G')
    {
        stl_ptr->group = (byte)atoi(buf + 2);
        return 0;
    }

    if (buf[0] == 'W')
    {
        int r;
        int c;

        if (2 != sscanf(buf + 2, "%d:%d", &r, &c))
            return PARSE_ERROR_GENERIC;

        stl_ptr->wall_row = (byte)r;
        stl_ptr->wall_col = (byte)c;
        return 0;
    }

    if (buf[0] == 'Y')
    {
        int r;
        int c;

        if (2 != sscanf(buf + 2, "%d:%d", &r, &c))
            return PARSE_ERROR_GENERIC;

        stl_ptr->vein_row = (byte)r;
        stl_ptr->vein_col = (byte)c;
        stl_ptr->vein_defined = true;
        return 0;
    }

    if (buf[0] == 'F')
    {
        const char* p = buf + 2;
        int added = 0;

        while (*p)
        {
            int r = -1;
            int c = -1;
            int n = 0;

            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '\0' || *p == '#')
                break;

            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2)
            {
                if (stl_ptr->floor_count == 0)
                {
                    stl_ptr->floor_row = (byte)r;
                    stl_ptr->floor_col = (byte)c;
                }
                if (stl_ptr->floor_count < 8)
                {
                    stl_ptr->floor_rowv[stl_ptr->floor_count] = (byte)r;
                    stl_ptr->floor_colv[stl_ptr->floor_count] = (byte)c;
                    stl_ptr->floor_count++;
                    added++;
                }
                p += n;
            }
            else
            {
                break;
            }
        }

        if (!added)
            return PARSE_ERROR_GENERIC;
        return 0;
    }

    if (buf[0] == 'D')
    {
        const char* p = buf + 2;
        int added = 0;

        while (*p)
        {
            int r = -1;
            int c = -1;
            int n = 0;

            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '\0' || *p == '#')
                break;

            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2)
            {
                if (stl_ptr->door_count == 0)
                {
                    stl_ptr->door_row = (byte)r;
                    stl_ptr->door_col = (byte)c;
                }
                if (stl_ptr->door_count < 8)
                {
                    stl_ptr->door_rowv[stl_ptr->door_count] = (byte)r;
                    stl_ptr->door_colv[stl_ptr->door_count] = (byte)c;
                    stl_ptr->door_count++;
                    added++;
                }
                p += n;
            }
            else
            {
                break;
            }
        }

        if (!added)
            return PARSE_ERROR_GENERIC;
        return 0;
    }

    if (buf[0] == 'S' && buf[1] == ':')
        return parse_style_short_desc_line(buf);

    if (buf[0] == 'M' && buf[1] == '1' && buf[2] == ':')
        return parse_style_m1_line(buf);

    if (buf[0] == 'M' && buf[1] == '2' && buf[2] == ':')
        return parse_style_m2_line(buf);

    if (buf[0] == 'M' && buf[1] == ':')
        return parse_style_message_line(buf);

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

static int parse_partition_style_kind(const char* tok)
{
    if (!tok)
        return -1;
    if (SDL_strcasecmp(tok, "CA") == 0 || SDL_strcasecmp(tok, "CA_BLOB") == 0
        || SDL_strcasecmp(tok, "CA_BLOBS") == 0)
    {
        return PART_STYLE_CA_BLOB;
    }
    if (SDL_strcasecmp(tok, "LAB") == 0 || SDL_strcasecmp(tok, "LABYRINTH") == 0)
        return PART_STYLE_LABYRINTH;
    if (SDL_strcasecmp(tok, "CHASM") == 0
        || SDL_strcasecmp(tok, "CHASM_FLOOR") == 0)
    {
        return PART_STYLE_CHASM_FLOOR;
    }
    if (SDL_strcasecmp(tok, "CHASM_BRIDGE") == 0
        || SDL_strcasecmp(tok, "CHASM_BRIDGES") == 0)
    {
        return PART_STYLE_CHASM_BRIDGE;
    }
    if (SDL_strcasecmp(tok, "BIG_ICE") == 0
        || SDL_strcasecmp(tok, "BIG_CAVE_ICE") == 0
        || SDL_strcasecmp(tok, "ICE") == 0)
    {
        return PART_STYLE_BIG_CAVE_ICE;
    }
    if (SDL_strcasecmp(tok, "BIG_FIRE") == 0
        || SDL_strcasecmp(tok, "BIG_CAVE_FIRE") == 0
        || SDL_strcasecmp(tok, "FIRE") == 0)
    {
        return PART_STYLE_BIG_CAVE_FIRE;
    }
    if (SDL_strcasecmp(tok, "BIG_POIS") == 0
        || SDL_strcasecmp(tok, "BIG_CAVE_POIS") == 0
        || SDL_strcasecmp(tok, "POIS") == 0
        || SDL_strcasecmp(tok, "POISON") == 0)
    {
        return PART_STYLE_BIG_CAVE_POIS;
    }
    return -1;
}

static int parse_big_cave_weight_token(const char* tok)
{
    if (!tok)
        return BIG_CAVE_NONE;
    if (SDL_strcasecmp(tok, "ICE") == 0 || SDL_strcasecmp(tok, "COLD") == 0)
        return BIG_CAVE_ICE;
    if (SDL_strcasecmp(tok, "FIRE") == 0)
        return BIG_CAVE_FIRE;
    if (SDL_strcasecmp(tok, "POIS") == 0 || SDL_strcasecmp(tok, "POISON") == 0)
        return BIG_CAVE_POIS;
    return BIG_CAVE_NONE;
}

errr parse_style_levels(char* buf, header* head)
{
    (void)head;

    if (buf[0] == 'V')
    {
        styles_rules_clear();
        styles_vault_rules_clear();
        styles_default_vault_clear();
        styles_partition_rules_clear();
        big_cave_type_rules_clear();
        log_debug("parse_style_levels: Version header encountered, cleared existing rules");
        return 0;
    }

    if (buf[0] == '#' || buf[0] == '\0')
        return 0;

    if (buf[0] == 'L')
    {
        int min_d = 0;
        int max_d = 0;
        char* s = strchr(buf + 2, ':');
        char* first_space;
        char* second_colon;
        char* t;
        int sidx[64];
        int wt[64];
        int n = 0;

        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        min_d = atoi(buf + 2);
        first_space = strchr(s, ' ');
        second_colon = strchr(s, ':');
        if (second_colon && (!first_space || second_colon < first_space))
        {
            *second_colon = '\0';
            max_d = atoi(s);
            t = second_colon + 1;
        }
        else
        {
            max_d = min_d;
            t = s;
        }

        while (*t)
        {
            char* e;
            char hold;
            char* c;
            int si;
            int w;

            while (*t == ' ')
                t++;
            if (!*t)
                break;
            e = t;
            while (*e && *e != ' ')
                e++;
            hold = *e;
            if (*e)
                *e = '\0';
            c = strchr(t, ':');
            if (!c)
            {
                if (hold)
                    *e = hold;
                break;
            }
            *c = '\0';
            si = atoi(t);
            w = atoi(c + 1);
            if (si >= 0 && w > 0 && n < 64)
            {
                sidx[n] = si;
                wt[n] = w;
                n++;
            }
            if (hold)
            {
                *e = hold;
                t = e + 1;
            }
            else
            {
                break;
            }
        }

        if (n > 0)
        {
            if (min_d < 1)
                min_d = 1;
            if (max_d > 31)
                max_d = 31;
            for (int d = min_d; d <= max_d; ++d)
                styles_add_level_rule(d, 0, sidx, wt, n);
            log_debug("parse_style_levels: L:%d..%d with %d entries (first sidx=%d w=%d)",
                min_d, max_d, n, sidx[0], wt[0]);
        }
        return 0;
    }

    if (buf[0] == 'P')
    {
        int min_d = 0;
        int max_d = 0;
        char* s = strchr(buf + 2, ':');
        int kind;
        char* t;
        char* first_space;
        char* second_colon;
        char* list_start;
        int sidx[64];
        int wt[64];
        int n = 0;

        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        kind = parse_partition_style_kind(buf + 2);
        if (kind < 0)
            return PARSE_ERROR_GENERIC;

        t = strchr(s, ':');
        if (!t)
            return PARSE_ERROR_GENERIC;
        *t++ = '\0';
        min_d = atoi(s);

        first_space = strchr(t, ' ');
        second_colon = strchr(t, ':');
        if (second_colon && (!first_space || second_colon < first_space))
        {
            *second_colon = '\0';
            max_d = atoi(t);
            list_start = second_colon + 1;
        }
        else
        {
            max_d = min_d;
            list_start = t;
        }

        while (*list_start)
        {
            char* e;
            char hold;
            char* c;
            int si;
            int w;

            while (*list_start == ' ')
                list_start++;
            if (!*list_start)
                break;
            e = list_start;
            while (*e && *e != ' ')
                e++;
            hold = *e;
            if (*e)
                *e = '\0';
            c = strchr(list_start, ':');
            if (!c)
            {
                if (hold)
                    *e = hold;
                break;
            }
            *c = '\0';
            si = atoi(list_start);
            w = atoi(c + 1);
            if (si >= 0 && w > 0 && n < 64)
            {
                sidx[n] = si;
                wt[n] = w;
                n++;
            }
            if (hold)
            {
                *e = hold;
                list_start = e + 1;
            }
            else
            {
                break;
            }
        }

        if (n > 0)
        {
            if (min_d < 1)
                min_d = 1;
            if (max_d > 31)
                max_d = 31;
            for (int d = min_d; d <= max_d; ++d)
                styles_add_partition_rule(d, kind, sidx, wt, n);
            log_debug("parse_style_levels: P:kind=%d %d..%d with %d entries (first sidx=%d w=%d)",
                kind, min_d, max_d, n, sidx[0], wt[0]);
        }
        return 0;
    }

    if (buf[0] == 'B')
    {
        int min_d = 0;
        int max_d = 0;
        char* s = strchr(buf + 2, ':');
        char* first_space;
        char* second_colon;
        char* list_start;
        int ice_w = 0;
        int fire_w = 0;
        int pois_w = 0;

        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        min_d = atoi(buf + 2);
        first_space = strchr(s, ' ');
        second_colon = strchr(s, ':');
        if (second_colon && (!first_space || second_colon < first_space))
        {
            *second_colon = '\0';
            max_d = atoi(s);
            list_start = second_colon + 1;
        }
        else
        {
            max_d = min_d;
            list_start = s;
        }

        while (*list_start)
        {
            char* e;
            char hold;
            char* c;
            int kind;
            int w;

            while (*list_start == ' ')
                list_start++;
            if (!*list_start)
                break;
            e = list_start;
            while (*e && *e != ' ')
                e++;
            hold = *e;
            if (*e)
                *e = '\0';
            c = strchr(list_start, ':');
            if (!c)
            {
                if (hold)
                    *e = hold;
                break;
            }
            *c = '\0';
            kind = parse_big_cave_weight_token(list_start);
            w = atoi(c + 1);
            if (w < 0)
                w = 0;
            if (kind == BIG_CAVE_ICE)
                ice_w = w;
            else if (kind == BIG_CAVE_FIRE)
                fire_w = w;
            else if (kind == BIG_CAVE_POIS)
                pois_w = w;
            if (hold)
            {
                *e = hold;
                list_start = e + 1;
            }
            else
            {
                break;
            }
        }

        if (min_d < 1)
            min_d = 1;
        if (max_d > 31)
            max_d = 31;
        for (int d = min_d; d <= max_d; ++d)
            big_cave_type_set_rule(d, ice_w, fire_w, pois_w);
        log_debug("parse_style_levels: B:%d..%d ice=%d fire=%d pois=%d",
            min_d, max_d, ice_w, fire_w, pois_w);
        return 0;
    }

    if (buf[0] == 'U')
    {
        char* s = strchr(buf + 2, ':');
        const char* depth_tok = buf + 2;
        int sidx[64];
        int wt[64];
        int n = 0;

        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        while (*s)
        {
            char* e;
            char hold;
            char* c;
            int si;
            int w;

            while (*s == ' ')
                s++;
            if (!*s)
                break;
            e = s;
            while (*e && *e != ' ')
                e++;
            hold = *e;
            if (*e)
                *e = '\0';
            c = strchr(s, ':');
            if (!c)
            {
                if (hold)
                    *e = hold;
                break;
            }
            *c = '\0';
            si = (s[0] == '*' && s[1] == '\0') ? -1 : atoi(s);
            w = atoi(c + 1);
            if (w > 0 && n < 64)
            {
                sidx[n] = si;
                wt[n] = w;
                n++;
            }
            if (hold)
            {
                *e = hold;
                s = e + 1;
            }
            else
            {
                break;
            }
        }

        if (depth_tok[0] == '*' && depth_tok[1] == '\0')
        {
            styles_default_vault_clear();
            for (int i = 0; i < n; ++i)
                styles_default_vault_add(sidx[i], wt[i]);
            log_debug("parse_style_levels: U:* default with %d entries (first=%d:%d)",
                n, sidx[0], wt[0]);
        }
        else
        {
            int d = atoi(depth_tok);
            if (n > 0)
            {
                styles_set_vault_rule(d, sidx, wt, n);
                log_debug("parse_style_levels: U:%d with %d entries (first=%d:%d)",
                    d, n, sidx[0], wt[0]);
            }
        }
        return 0;
    }

    return 0;
}

#define MAX_STYLE_MSG 8

static const char* g_style_short_desc[128];
static const char* g_style_m1_text[128][MAX_STYLE_MSG];
static byte g_style_m1_count[128];
static const char* g_style_m2_text[128][MAX_STYLE_MSG];
static byte g_style_m2_count[128];

const char* styles_get_style_short_desc(int sidx)
{
    if (sidx < 0 || sidx >= 128)
        return NULL;
    return g_style_short_desc[sidx];
}

const char* styles_get_style_m1(int sidx)
{
    byte n;
    int pick;

    if (sidx < 0 || sidx >= 128)
        return NULL;
    n = g_style_m1_count[sidx];
    if (!n)
        return NULL;
    pick = (n == 1) ? 0 : rand_int(n);
    return g_style_m1_text[sidx][pick];
}

const char* styles_get_style_m2(int sidx)
{
    byte n;
    int pick;

    if (sidx < 0 || sidx >= 128)
        return NULL;
    n = g_style_m2_count[sidx];
    if (!n)
        return NULL;
    pick = (n == 1) ? 0 : rand_int(n);
    return g_style_m2_text[sidx][pick];
}

const char* styles_get_style_display(int sidx)
{
    return styles_get_style_m1(sidx);
}

static char* trim_narrative_text(char* s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '"' || *s == '\'')
    {
        char q = *s++;
        char* e = strrchr(s, q);
        if (e)
            *e = '\0';
    }
    {
        char* h = strchr(s, '#');
        if (h)
            *h = '\0';
    }
    for (char* t = s + strlen(s) - 1;
         t >= s && (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n'); --t)
    {
        *t = '\0';
    }
    return s;
}

static int parse_narrative_idx(char* colon_start, char** out_text)
{
    if (*colon_start != ':')
        return -1;

    {
        char* p = colon_start + 1;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p >= '0' && *p <= '9')
        {
            char* c = strchr(p, ':');
            if (!c)
                return -1;
            *c++ = '\0';
            *out_text = c;
            return atoi(p);
        }
        *out_text = p;
    }

    return error_idx;
}

static errr parse_style_short_desc_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 1, &text);

    if (idx < 0 || idx >= 128 || !text)
        return PARSE_ERROR_GENERIC;

    text = trim_narrative_text(text);
    if (g_style_short_desc[idx])
        str_free(g_style_short_desc[idx]);
    g_style_short_desc[idx] = str_dup(text);
    return 0;
}

static errr parse_style_m1_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 2, &text);

    if (idx < 0 || idx >= 128 || !text)
        return PARSE_ERROR_GENERIC;

    text = trim_narrative_text(text);
    if (g_style_m1_count[idx] >= MAX_STYLE_MSG)
    {
        log_debug("parse_style_m1_line: style %d M1 list full, dropping", idx);
        return 0;
    }
    g_style_m1_text[idx][g_style_m1_count[idx]] = str_dup(text);
    g_style_m1_count[idx]++;
    return 0;
}

static errr parse_style_m2_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 2, &text);

    if (idx < 0 || idx >= 128 || !text)
        return PARSE_ERROR_GENERIC;

    text = trim_narrative_text(text);
    if (g_style_m2_count[idx] >= MAX_STYLE_MSG)
    {
        log_debug("parse_style_m2_line: style %d M2 list full, dropping", idx);
        return 0;
    }
    g_style_m2_text[idx][g_style_m2_count[idx]] = str_dup(text);
    g_style_m2_count[idx]++;
    return 0;
}

static errr parse_style_message_line(char* buf)
{
    char* text = NULL;
    int idx;

    if (buf[0] != 'M')
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    idx = parse_narrative_idx(buf + 1, &text);
    if (idx < 0 || idx >= 128 || !text)
        return PARSE_ERROR_GENERIC;

    text = trim_narrative_text(text);
    if (g_style_m1_count[idx] >= MAX_STYLE_MSG)
    {
        log_debug("parse_style_message_line: style %d M1 list full, dropping (legacy M:)",
            idx);
        return 0;
    }
    g_style_m1_text[idx][g_style_m1_count[idx]] = str_dup(text);
    g_style_m1_count[idx]++;
    return 0;
}

void styles_clear_display_messages(void)
{
    for (int i = 0; i < 128; ++i)
    {
        if (g_style_short_desc[i])
        {
            str_free(g_style_short_desc[i]);
            g_style_short_desc[i] = NULL;
        }
        for (int j = 0; j < MAX_STYLE_MSG; ++j)
        {
            if (g_style_m1_text[i][j])
            {
                str_free(g_style_m1_text[i][j]);
                g_style_m1_text[i][j] = NULL;
            }
            if (g_style_m2_text[i][j])
            {
                str_free(g_style_m2_text[i][j]);
                g_style_m2_text[i][j] = NULL;
            }
        }
        g_style_m1_count[i] = 0;
        g_style_m2_count[i] = 0;
    }
}

void styles_reload_messages_from_text(void)
{
    char path[1024];
    ang_file* fp;
    char buf[1024];

    styles_clear_display_messages();

    if (!path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "style")))
    {
        log_warn("styles_reload_messages_from_text: failed to build style.txt path");
        return;
    }

    fp = ang_file_open(path, "r");
    if (!fp)
    {
        log_warn("styles_reload_messages_from_text: couldn't open %s", path);
        return;
    }

    error_idx = -1;

    while (sdl_fgets(fp, buf, sizeof(buf)) == 0)
    {
        char* s = buf;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '\0' || *s == '#')
            continue;
        if (*s == 'N')
        {
            char* colon = strchr(s + 2, ':');
            if (!colon)
                continue;
            *colon = '\0';
            error_idx = atoi(s + 2);
            continue;
        }
        if (s[0] == 'S' && s[1] == ':')
        {
            (void)parse_style_short_desc_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == '1' && s[2] == ':')
        {
            (void)parse_style_m1_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == '2' && s[2] == ':')
        {
            (void)parse_style_m2_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == ':')
        {
            (void)parse_style_message_line(s);
            continue;
        }
    }

    ang_file_close(fp);
    log_info("styles_reload_messages_from_text: loaded per-style narrative text");
}

#endif /* ALLOW_TEMPLATES */

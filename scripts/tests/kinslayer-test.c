#include "angband.h"
#include "score/score_entry.c"
#include <assert.h>

int main(void)
{
    maxima limits = {0};
    player_race races[7] = {{0}};
    character_profile characters[4] = {{0}};
    byte bytes[sizeof(score_file_header) + sizeof(high_score)];
    log_set_level(LOG_ERROR);
    limits.c_max = 4;
    z_info = &limits; p_info = races; c_info = characters;
    c_name = "Target";
    races[3].choice[0] = 1U << 3;
    SDL_strlcpy(op_ptr->base_name, "Killer", sizeof(op_ptr->base_name));
    ANGBAND_DIR_APEX = ".";
    assert(parse_score_id(" 3") == 3);
    assert(parse_score_id("03") == 3);
    assert(parse_score_id("50") == 50);
    assert(parse_score_id("?3") == -1);

    for (int padded = 0; padded < 2; padded++)
    for (int state = 0; state < 4; state++) {
        score_file_header header = {0};
        high_score entry = {0}, after;
        score_file_ctx* ctx = score_file_active_ctx();
        header.entry_count = 1;
        header.version_major = SCORE_FILE_VERSION_MAJOR;
        header.version_minor = SCORE_FILE_VERSION_MINOR;
        header.version_patch = SCORE_FILE_VERSION_PATCH;
        header.version_extra = SCORE_FILE_VERSION_EXTRA;
        SDL_strlcpy(entry.who, "Target", sizeof(entry.who));
        SDL_strlcpy(entry.p_r, padded ? "03" : " 3", sizeof(entry.p_r));
        SDL_strlcpy(entry.p_h, padded ? "03" : " 3", sizeof(entry.p_h));
        SDL_strlcpy(entry.how, state == 1 ? "an orc" : "(alive and well)", sizeof(entry.how));
        entry.escaped[0] = state == 2 ? 't' : 'f';
        memcpy(bytes, &header, sizeof(header));
        memcpy(bytes + sizeof(header), &entry, sizeof(entry));
        ctx->version_major = header.version_major;
        ctx->version_minor = header.version_minor;
        ctx->version_patch = header.version_patch;
        ctx->version_extra = header.version_extra;
        ctx->entry_count = 1;
        ctx->fd = state == 3 ? SDL_IOFromConstMem(bytes, sizeof(bytes))
            : SDL_IOFromMem(bytes, sizeof(bytes));
        assert(ctx->fd);
        const char* killed = kinslayer_try_kill(1, false);
        assert(!ctx->fd && ctx->entry_count == 1);
        memcpy(&after, bytes + sizeof(header), sizeof(after));
        if (state == 0) {
            assert(killed && !strcmp(killed, "Target"));
            assert(!strcmp(after.how, "Killer"));
        } else {
            assert(!killed);
            assert(!memcmp(&after, &entry, sizeof(entry)));
        }
    }
    puts("Kinslayer: space/zero-padded IDs update existing live heroes, preserve dead/escaped heroes and reject failed writes: PASS");
    return 0;
}

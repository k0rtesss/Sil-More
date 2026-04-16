/* File: init-parse-partition.c */

#include "angband.h"
#include "drop_system.h"
#include "init.h"

#ifdef ALLOW_TEMPLATES

static level_partition_kind parse_partition_kind_token(const char* tok)
{
    if (!tok)
        return LEVEL_PART_NONE;
    if (SDL_strcasecmp(tok, "ROOMY") == 0)
        return LEVEL_PART_ROOMY;
    if (SDL_strcasecmp(tok, "CAVEY") == 0)
        return LEVEL_PART_CAVEY;
    if (SDL_strcasecmp(tok, "RUINED") == 0)
        return LEVEL_PART_RUINED;
    if (SDL_strcasecmp(tok, "LABYRINTH") == 0 || SDL_strcasecmp(tok, "LAB") == 0)
        return LEVEL_PART_LABYRINTH;
    if (SDL_strcasecmp(tok, "CHASM") == 0)
        return LEVEL_PART_CHASM;
    if (SDL_strcasecmp(tok, "BIG_CAVE") == 0
        || SDL_strcasecmp(tok, "BIGCAVE") == 0
        || SDL_strcasecmp(tok, "BIG-CAVE") == 0
        || SDL_strcasecmp(tok, "BIG") == 0)
    {
        return LEVEL_PART_BIG_CAVE;
    }
    return LEVEL_PART_NONE;
}

static errr parse_partition_profile_values(const char* data,
    level_partition_kind kind, partition_drop_source_t source)
{
    drop_profile profile;
    int weapon;
    int armor;
    int jewelry;
    int supply;
    int potion;
    int herb;
    int gem;
    int staff;
    int light;
    int arrows;
    int tunneling;
    int allow_damaged = 0;
    int parsed;

    drop_profile_default(&profile);

    parsed = sscanf(data, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
        &weapon, &armor, &jewelry, &supply, &potion, &herb, &gem, &staff,
        &light, &arrows, &tunneling, &allow_damaged);
    if (parsed != 11 && parsed != 12)
        return PARSE_ERROR_GENERIC;

    profile.weight_weapon = weapon;
    profile.weight_armor = armor;
    profile.weight_jewelry = jewelry;
    profile.weight_supply = supply;
    profile.supply_potion = potion;
    profile.supply_herb = herb;
    profile.supply_gem = gem;
    profile.supply_staff = staff;
    profile.supply_light = light;
    profile.supply_arrows = arrows;
    profile.supply_tunneling = tunneling;
    profile.allow_damaged = (parsed == 12) ? (allow_damaged ? true : false) : false;

    partition_config_set_drop_profile(kind, source, &profile);
    return 0;
}

errr parse_partition_info(char* buf, header* head)
{
    static level_partition_kind current_kind = LEVEL_PART_NONE;

    (void)head;

    if (buf[0] == 'V')
    {
        partition_config_reset();
        current_kind = LEVEL_PART_NONE;
        return 0;
    }

    if (buf[0] == '#' || buf[0] == '\0')
        return 0;

    if (buf[0] == 'N' && buf[1] == ':')
    {
        current_kind = parse_partition_kind_token(buf + 2);
        if (current_kind <= LEVEL_PART_NONE || current_kind >= LEVEL_PART_MAX)
            return PARSE_ERROR_GENERIC;
        return 0;
    }

    if (current_kind <= LEVEL_PART_NONE || current_kind >= LEVEL_PART_MAX)
        return PARSE_ERROR_MISSING_RECORD_HEADER;

    if (buf[0] == 'P' && buf[1] == 'F' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_FLOOR);

    if (buf[0] == 'P' && buf[1] == 'C' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_CHEST);

    if (buf[0] == 'P' && buf[1] == 'M' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_MONSTER);

    if (buf[0] == 'F' && buf[1] == 'L' && buf[2] == ':')
    {
        int allow_floor = 0;
        if (1 != sscanf(buf + 3, "%d", &allow_floor))
            return PARSE_ERROR_GENERIC;
        partition_config_set_floor_rules(current_kind, allow_floor ? true : false);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'B' && buf[2] == ':')
    {
        int numerator = 0;
        int denominator = 0;
        if (2 != sscanf(buf + 3, "%d:%d", &numerator, &denominator))
            return PARSE_ERROR_GENERIC;
        partition_config_set_base_monster_scale(current_kind, numerator, denominator);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'F' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        if (3 != sscanf(buf + 3, "%d:%d:%d", &divisor, &min_count, &max_count))
            return PARSE_ERROR_GENERIC;
        partition_config_set_direct_monster_rule(
            current_kind, divisor, min_count, max_count);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'D' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        int scale_pct_at_depth_20 = 0;
        int hard_cap_divisor = 0;
        if (5 != sscanf(buf + 3, "%d:%d:%d:%d:%d",
                &divisor, &min_count, &max_count, &scale_pct_at_depth_20,
                &hard_cap_divisor))
        {
            return PARSE_ERROR_GENERIC;
        }
        partition_config_set_depth_monster_rule(current_kind, divisor,
            min_count, max_count, scale_pct_at_depth_20, hard_cap_divisor);
        return 0;
    }

    if (buf[0] == 'O' && buf[1] == 'D' && buf[2] == ':')
    {
        int room_divisor = 0;
        int corridor_divisor = 0;
        if (2 != sscanf(buf + 3, "%d:%d", &room_divisor, &corridor_divisor))
            return PARSE_ERROR_GENERIC;
        partition_config_set_object_rules(current_kind, room_divisor, corridor_divisor);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'T' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        int min_depth = 0;
        if (4 != sscanf(buf + 3, "%d:%d:%d:%d",
                &divisor, &min_count, &max_count, &min_depth))
        {
            return PARSE_ERROR_GENERIC;
        }
        partition_config_set_metal_rule(
            current_kind, divisor, min_count, max_count, min_depth);
        return 0;
    }

    if (buf[0] == 'T' && buf[1] == ':')
    {
        partition_config_set_discovery_text(current_kind, buf + 2);
        return 0;
    }

    if (current_kind == LEVEL_PART_BIG_CAVE && buf[0] == 'B'
        && buf[2] == ':')
    {
        if (buf[1] == 'I')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_ICE, buf + 3);
            return 0;
        }
        if (buf[1] == 'F')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_FIRE, buf + 3);
            return 0;
        }
        if (buf[1] == 'P')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_POIS, buf + 3);
            return 0;
        }
    }

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

#endif /* ALLOW_TEMPLATES */

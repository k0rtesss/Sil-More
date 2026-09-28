#include "angband.h"
#include "externs.h"
#include "tutorial-world.h"
#include "tutorial.h"
#include "metarun.h"
#include "blitz.h"
#include "supplies.h"

typedef struct world_snapshot {
    int pack, harness, pack_limit, harness_limit, arrows, weight, light;
    int partition, cave_type, depth;
    int curse, blessing, silmarils;
    int quests[6], mandos_remaining, orome_killed, niena_seen;
    bool tulkas_complete;
    byte oath_broken, truce;
} world_snapshot;

static world_snapshot previous;
static bool initialized;

typedef struct monster_flag_lesson {
    int group;
    u32b flag;
    const char *id;
} monster_flag_lesson;

/* Only behavioural/combat knowledge: omit generation, cosmetic and loot flags.
 * Both the live race and recorded lore must contain the flag before offering. */
#define MF(G, F) {G, RF##G##_##F, "monster.rf" #G "_" #F}
static const monster_flag_lesson monster_lessons[] = {
    MF(1, UNIQUE), MF(1, PEACEFUL), MF(1, NEVER_BLOW), MF(1, NEVER_MOVE),
    MF(1, HIDDEN_MOVE), MF(1, NO_CRIT), MF(1, RES_CRIT), MF(1, RAND_25), MF(1, RAND_50),
    MF(2, MINDLESS), MF(2, SMART), MF(2, TERRITORIAL), MF(2, SHORT_SIGHTED),
    MF(2, INVISIBLE), MF(2, GLOW), MF(2, CRUEL_BLOW), MF(2, EXCHANGE_PLACES),
    MF(2, MULTIPLY), MF(2, REGENERATE), MF(2, RIPOSTE), MF(2, FLANKING),
    MF(2, CLOUD_SURROUND), MF(2, FLYING), MF(2, PASS_DOOR), MF(2, UNLOCK_DOOR),
    MF(2, OPEN_DOOR), MF(2, BASH_DOOR), MF(2, PASS_WALL), MF(2, KILL_WALL),
    MF(2, TUNNEL_WALL), MF(2, KILL_BODY), MF(2, TAKE_ITEM), MF(2, KILL_ITEM),
    MF(2, LOW_MANA_RUN), MF(2, CHARGE), MF(2, ELFBANE), MF(2, KNOCK_BACK),
    MF(2, CRIPPLING), MF(2, OPPORTUNIST), MF(2, ZONE_OF_CONTROL),
    MF(3, ORC), MF(3, TROLL), MF(3, SERPENT), MF(3, DRAGON), MF(3, RAUKO),
    MF(3, UNDEAD), MF(3, SPIDER), MF(3, WOLF), MF(3, MAN), MF(3, ELF),
    MF(3, GIANT), MF(3, CAT), MF(3, HORROR), MF(3, VAMPIRE),
    MF(3, HURT_LITE), MF(3, STONE), MF(3, HURT_FIRE), MF(3, HURT_COLD),
    MF(3, HAS_WEAPON), MF(3, HAS_ARMOUR), MF(3, RES_ELEC), MF(3, RES_FIRE),
    MF(3, RES_COLD), MF(3, RES_POIS), MF(3, NO_SLOW), MF(3, NO_FEAR),
    MF(3, NO_STUN), MF(3, NO_CONF), MF(3, NO_SLEEP),
    MF(4, ARROW1), MF(4, ARROW2), MF(4, BOULDER), MF(4, BRTH_FIRE),
    MF(4, BRTH_COLD), MF(4, BRTH_POIS), MF(4, BRTH_DARK), MF(4, EARTHQUAKE),
    MF(4, SHRIEK), MF(4, SCREECH), MF(4, DARKNESS), MF(4, FORGET),
    MF(4, SCARE), MF(4, CONF), MF(4, HOLD), MF(4, SLOW), MF(4, HATCH_SPIDER),
    MF(4, DIM), MF(4, SNG_BINDING), MF(4, SNG_PIERCING), MF(4, SNG_OATHS),
    MF(4, DWARFBANE), MF(4, EDAINBANE), MF(4, THROW_WEB), MF(4, RALLY),
    MF(4, NOLDORBANE), MF(4, SINDARBANE)
};
#undef MF

static bool world_available(void)
{
    return character_generated && p_ptr && p_ptr->playing
        && !p_ptr->tutorial_deferred && !p_ptr->is_dead
        && !death_spectator_active() && !run_mode_is_blitz();
}

static void world_observe(const char *id, const char *type,
    const char *subject, const char *text)
{
    tutorial_context context = {0};
    if (!tutorial_enabled()) return;
    SDL_strlcpy(context.subject_type,type?type:"",sizeof(context.subject_type));
    SDL_strlcpy(context.subject,subject?subject:"",sizeof(context.subject));
    SDL_strlcpy(context.text,text?text:"",sizeof(context.text));
    tutorial_observe(id,&context);
}

static int light_state(void)
{
    const object_type *light=&inventory[INVEN_LITE];
    if (!light->k_idx || !fuelable_light_p(light)) return 0;
    int fuel=player_light_fuel(light);
    return fuel<=0?2:fuel<=player_light_sputter_threshold(light)?1:0;
}

static int extra_silmarils(void)
{
    int count=0;
    for (int i=0;i<player_carried_extra_entry_count();++i) {
        const object_type *item=player_carried_extra_entry_at(i);
        if (!item || !item->k_idx) continue;
        if (item->tval==TV_LIGHT && item->sval==SV_LIGHT_SILMARIL) count+=item->number;
        if (item->name1>=ART_MORGOTH_1 && item->name1<=ART_MORGOTH_3)
            count+=item->name1-ART_MORGOTH_0;
    }
    return count;
}

static world_snapshot snapshot(void)
{
    world_snapshot now={0};
    now.pack=inventory_limit_usage_for_group(INV_LIMIT_PACK);
    now.harness=inventory_limit_usage_for_group(INV_LIMIT_HARNESS);
    now.pack_limit=inventory_limit_limit_for_group(INV_LIMIT_PACK);
    now.harness_limit=inventory_limit_limit_for_group(INV_LIMIT_HARNESS);
    now.arrows=player_quiver_arrow_count();
    now.weight=p_ptr->total_weight>weight_limit();
    now.light=light_state();
    now.partition=level_partition_kind_for_point(p_ptr->py,p_ptr->px);
    now.cave_type=level_partition_big_cave_type_for_point(p_ptr->py,p_ptr->px);
    now.depth=p_ptr->depth;
    for (int i=0;i<METAR_CURSE_SLOTS;++i) {
        int stack=CURSE_GET(i);
        if (!CURSE_SEEN(i)) continue;
        if (stack>0) now.curse+=stack;
        else now.blessing-=stack;
    }
    {
        const metarun *tale=metarun_current();
        /* major_blessing_count() is the catalogue capacity, not ownership. */
        u32b blessings=tale?tale->major_blessings:0;
        for (;blessings;blessings&=blessings-1) ++now.blessing;
    }
    now.silmarils=silmarils_possessed()+extra_silmarils();
    now.oath_broken=p_ptr->oaths_broken;
    now.truce=p_ptr->truce;
    now.quests[0]=p_ptr->tulkas_quest;
    now.quests[1]=p_ptr->aule_quest;
    now.quests[2]=p_ptr->mandos_quest;
    now.quests[3]=p_ptr->niena_quest;
    now.quests[4]=p_ptr->orome_quest;
    now.quests[5]=p_ptr->varda_quest;
    now.tulkas_complete=p_ptr->tulkas_quest_complete;
    now.mandos_remaining=p_ptr->mandos_monsters_remaining;
    now.orome_killed=p_ptr->orome_killed_count;
    now.niena_seen=p_ptr->niena_monsters_seen;
    return now;
}

void tutorial_world_start(void)
{
    initialized=world_available();
    if (initialized) previous=snapshot();
}

static void observe_monsters(void)
{
    bool present[N_ELEMENTS(monster_lessons)] = {false};
    const monster_type *subjects[N_ELEMENTS(monster_lessons)] = {NULL};
    int selected = -1;
    for (int i=1;i<mon_max;++i) {
        const monster_type *monster=&mon_list[i];
        if (p_ptr->image || p_ptr->blind || !monster->r_idx || !monster->ml
            || !player_has_los_bold(monster->fy, monster->fx)) continue;
        const monster_lore *lore=&l_list[monster->r_idx];
        const monster_race *race=&r_info[monster->r_idx];
        u32b known[4]={lore->flags1&race->flags1,lore->flags2&race->flags2,
            lore->flags3&race->flags3,lore->flags4&race->flags4};
        for (size_t j=0;j<N_ELEMENTS(monster_lessons);++j) {
            const monster_flag_lesson *lesson=&monster_lessons[j];
            if (!(known[lesson->group-1]&lesson->flag)) continue;
            present[j] = true;
            subjects[j] = monster;
        }
    }
    for (size_t j=0;j<N_ELEMENTS(monster_lessons);++j) {
        char id[80];
        SDL_strlcpy(id,monster_lessons[j].id,sizeof(id));
        for (char *p=id;*p;++p) *p=(char)tolower((unsigned char)*p);
        if (!present[j]) { tutorial_forget_observation(id); continue; }
        if (!tutorial_lesson_enabled(id)) continue;
        tutorial_status status=tutorial_lesson_status(id);
        if (status==TUTORIAL_COMPLETED || status==TUTORIAL_SKIPPED) continue;
        if (selected < 0) selected = (int)j;
        /* Refresh already queued subjects as well as the one new offer. */
        if ((int)j == selected || status == TUTORIAL_IN_PROGRESS) {
            char name[160];
            monster_desc(name,sizeof(name),subjects[j],0);
            world_observe(id,"monster",name,
                "This trait is already known about the visible creature. Inspect its recall before choosing your next action.");
        }
    }
}

void tutorial_world_checkpoint(void)
{
    static const char *quest_names[6]={"Tulkas","Aule","Mandos","Nienna","Orome","Varda"};
    world_snapshot now;
    char text[384],id[80];
    if (!world_available()) { initialized=false; return; }
    if (!initialized) { tutorial_world_start(); return; }
    now=snapshot();
    if ((now.pack!=previous.pack || now.pack_limit!=previous.pack_limit) && now.pack>0) {
        strnfmt(text,sizeof(text),"Pack space: %d/%d. Inspect the item's handling costs before accessing it.",
            now.pack,now.pack_limit);
        world_observe("storage.pack","storage","Pack",text);
    }
    if ((now.harness!=previous.harness || now.harness_limit!=previous.harness_limit) && now.harness>0) {
        strnfmt(text,sizeof(text),"Harness space: %d/%d. The Harness and Pack have separate volume limits.",
            now.harness,now.harness_limit);
        world_observe("storage.harness","storage","Harness",text);
    }
    if (now.arrows!=previous.arrows && now.arrows>0) {
        strnfmt(text,sizeof(text),"Arrows: %d. Current free Quiver space: %d.",now.arrows,player_quiver_arrow_space());
        world_observe("storage.quiver","arrows","Quiver",text);
    }
    if (now.weight)
        world_observe("storage.weight","storage","Encumbrance",
            "Carried weight exceeds your current limit and reduces speed. Excess Pack or Harness volume can also reduce speed.");
    else tutorial_forget_observation("storage.weight");
    if (now.light)
        world_observe(now.light==2?"world.light_out":"world.light_low","light","Light fuel",
            now.light==2?"Your fuelable light is out. Inspect a replacement or a suitable refill."
            :"Your light has reached its current sputtering threshold. Inspect fuel and a replacement before it runs out.");
    if (now.light != 1) tutorial_forget_observation("world.light_low");
    if (now.light != 2) tutorial_forget_observation("world.light_out");
    for (int i = LEVEL_PART_NONE + 1; i < LEVEL_PART_MAX; ++i)
        if (i != now.partition) {
            strnfmt(id,sizeof(id),"world.partition.%d",i);
            tutorial_forget_observation(id);
        }
    {
        if (now.partition>LEVEL_PART_NONE && now.partition<LEVEL_PART_MAX) {
            strnfmt(id,sizeof(id),"world.partition.%d",now.partition);
            world_observe(id,"terrain","Your current region",
                "This is the region you have entered. Read its effects before choosing your route.");
        }
    }
    {
        const char *element=now.cave_type==BIG_CAVE_FIRE?"fire":now.cave_type==BIG_CAVE_ICE?"cold":now.cave_type==BIG_CAVE_POIS?"poison":NULL;
        if (element) {
            strnfmt(id,sizeof(id),"world.partition.%s",element);
            world_observe(id,"terrain","Elemental cave",
                "You have entered this cave's environmental region. Check its terrain and your relevant resistances.");
        }
        const char *elements[] = {"fire", "cold", "poison"};
        for (int i = 0; i < (int)N_ELEMENTS(elements); ++i)
            if (!element || strcmp(element, elements[i])) {
                strnfmt(id,sizeof(id),"world.partition.%s",elements[i]);
                tutorial_forget_observation(id);
            }
    }
    for (int i=0;i<6;++i) {
        if (now.quests[i]==previous.quests[i]) continue;
        /* GIVER_PRESENT is also set during generation, before the player has
         * seen the giver. Acceptance and later states are public actions. */
        if (now.quests[i]<2) continue;
        strnfmt(id,sizeof(id),"quest.%d",i+1);
        world_observe(id,"quest",quest_names[i],"Your quest state has changed. Read its current objective and restrictions in Quests.");
        bool failed=(i==3 && now.quests[i]==NIENA_QUEST_FAILED)
            || (i==5 && now.quests[i]==VARDA_QUEST_FAILED);
        /* All six use 3 for first success; later reward states are distinct.
         * Aule's legacy state 4 is unused and is not a live failure event. */
        if (failed) world_observe("quest.failed","quest",quest_names[i],"This quest has failed. Review the recorded consequences in Quests.");
        else if (now.quests[i]==3)
            world_observe("quest.completed","quest",quest_names[i],"The quest objective is complete. Check whether a reward still needs to be claimed.");
        else if (now.quests[i]==2)
            world_observe("quest.progress","quest",quest_names[i],"The quest is active. Its restrictions now apply.");
    }
    if ((now.quests[2]==MANDOS_QUEST_ACTIVE && now.mandos_remaining<previous.mandos_remaining)
        || (now.quests[4]==OROME_QUEST_ACTIVE && now.orome_killed>previous.orome_killed)
        || (now.quests[3]==NIENA_QUEST_ACTIVE && now.niena_seen>previous.niena_seen))
        world_observe("quest.progress","quest","Quest progress","An active quest's recorded progress has changed. Review the current objective in Quests.");
    if (now.tulkas_complete && !previous.tulkas_complete)
        world_observe("quest.completed","quest","Tulkas","The target has been defeated. Review the reward state in Quests.");
    if (now.curse>previous.curse)
        world_observe("tale.curse","curse","Revealed Tale curse","A revealed Tale curse has been added or strengthened. Read its current effect in your character details.");
    if (now.blessing>previous.blessing)
        world_observe("tale.blessing","blessing","Tale blessing","A known blessing has been added or strengthened. Read its effect before planning around it.");
    if (now.oath_broken!=previous.oath_broken)
        world_observe("tale.oath_break","oath","Broken oath","Your recorded oath state changed. Review the consequence before acting again.");
    if (now.silmarils>previous.silmarils)
        world_observe("tale.silmaril","silmaril","A Silmaril","You now carry an additional Silmaril. Read the current escape objective and consequences.");
    if (now.truce!=previous.truce)
        world_observe("tale.truce","truce","Throne-room truce",now.truce
            ?"A truce is active. Consider which actions will break it."
            :"The truce has ended. Nearby foes can act normally.");
    previous=now;
    observe_monsters();
}

#include "angband.h"
#include "externs.h"
#include "fs/save-internal.h"
#include "fs/load-internal.h"

#define WRATH_SAVE_MAGIC 0xCA23

void save_write_catastrophe(void)
{
    catastrophe_state s=catastrophe_get_state();
    wr_u16b(WRATH_SAVE_MAGIC);
    wr_byte(s.chance); wr_byte(s.smith_milestones); wr_byte(s.find_milestones);
    wr_byte(s.kind); wr_byte(s.y); wr_byte(s.x); wr_byte(s.ready);
    wr_s32b(s.last_stage); wr_s32b(s.craft_difficulty);
    wr_s32b(s.timer_step);
    wr_u32b(s.random); wr_u32b(s.step);
    for(int i=0;i<CATA_EVENT_MAX;i++)wr_u32b(s.pending[i]);
    wr_u32b(catastrophe_acquired_count());
    for(int i=0;i<catastrophe_acquired_count();i++) {
        guid64 guid=catastrophe_acquired_guid(i);
        wr_u32b(guid.hi);wr_u32b(guid.lo);
    }
    /* RLE keeps an inactive level tiny, and is independent of native padding. */
    int total=p_ptr->cur_map_hgt*p_ptr->cur_map_wid;
    int pos=0;
    while(pos<total) {
        u32b value=catastrophe_cell_step(pos/p_ptr->cur_map_wid,pos%p_ptr->cur_map_wid);
        int length=1;
        while(length<65535 && pos+length<total
            && catastrophe_cell_step((pos+length)/p_ptr->cur_map_wid,
                (pos+length)%p_ptr->cur_map_wid)==value)length++;
        wr_u16b(length);wr_u32b(value);pos+=length;
    }
}

errr load_read_catastrophe(void)
{
    catastrophe_reset_run();
    if(!savefile_version_at_least(0,9,8,23)) {
        catastrophe_legacy_loaded();
        return 0;
    }
    u16b magic=0;
    byte ready=0;
    catastrophe_state s={0};
    rd_u16b(&magic);
    rd_byte(&s.chance);rd_byte(&s.smith_milestones);rd_byte(&s.find_milestones);
    rd_byte(&s.kind);rd_byte(&s.y);rd_byte(&s.x);rd_byte(&ready);s.ready=ready!=0;
    rd_s32b(&s.last_stage);rd_s32b(&s.craft_difficulty);
    rd_s32b(&s.timer_step);
    rd_u32b(&s.random);rd_u32b(&s.step);
    for(int i=0;i<CATA_EVENT_MAX;i++)rd_u32b(&s.pending[i]);
    if(magic!=WRATH_SAVE_MAGIC||ready>1||!catastrophe_restore_state(s))goto invalid;
    u32b count=0;rd_u32b(&count);
    if(count>65535)goto invalid;
    for(u32b i=0;i<count;i++) {
        guid64 guid={0};
        rd_u32b(&guid.hi);rd_u32b(&guid.lo);
        if(!catastrophe_restore_guid(guid))goto invalid;
    }
    int total=p_ptr->cur_map_hgt*p_ptr->cur_map_wid,pos=0;
    while(pos<total) {
        u16b length=0;u32b value=0;
        rd_u16b(&length);rd_u32b(&value);
        if(!length||length>total-pos)goto invalid;
        for(int i=0;i<length;i++,pos++)
            if(!catastrophe_restore_cell(pos/p_ptr->cur_map_wid,pos%p_ptr->cur_map_wid,value))goto invalid;
    }
    if(s.kind&&!catastrophe_cell_step(s.y,s.x))goto invalid;
    if(!load_only_checksums_remain())goto invalid;
    return 0;
invalid:
    catastrophe_reset_run();
    note("Invalid Morgoth's Wrath state.");
    return -1;
}

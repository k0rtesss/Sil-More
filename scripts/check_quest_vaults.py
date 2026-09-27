#!/usr/bin/env python3
"""Compile the production quest-vault token map and eligibility gate in isolation."""
from pathlib import Path
import os
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'scripts/output/quest-vaults'
def function(text, name):
    start = text.index(name)
    start = text.rfind('\n', 0, start) + 1
    opening = text.index('{', start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}')
        pos += 1
    return text[start:pos]
def main():
    source = (ROOT / 'src/level-generation/level-generation-vault-special.c').read_text(encoding='utf-8')
    table = source[source.index('static const struct { int vault;'):source.index('static int quest_vault_permitted_race;')]
    monsters = (ROOT / 'lib/edit/monster.txt').read_text(encoding='utf-8')
    guids = {}
    for block in re.split(r'(?=^N:)', monsters, flags=re.M):
        serial = re.match(r'N:(\d+):', block)
        guid = re.search(r'^Q:([0-9a-fA-F]+)', block, re.M)
        if serial and guid: guids[int(guid[1],16)] = int(serial[1])
    query = function(source, 'quest_vault_token_race(')
    available = function(source, 'quest_vault_tokens_available(')
    gate = function((ROOT / 'src/level-generation/level-generation-rooms-vaults.c').read_text(encoding='utf-8'), 'vault_is_valid_for_depth(')
    code = r'''
#include "angband.h"
#include "quest/quest-runtime.h"
#include <assert.h>
static maxima limits = {.r_max=700, .v_max=600};
maxima *z_info = &limits;
static monster_race races[700];
monster_race *r_info = races;
static vault_type vaults[600];
vault_type *v_info = vaults;
char *v_text = ".";
static bool enabled;
bool quest_enabled(int id) { (void)id; return enabled; }
int quest_followup_vault(int vault) { return vault >= 464 && vault <= 467 ? vault - 457 : 0; }
bool quest_followup_vault_allowed(int vault, int depth) { (void)vault; return enabled && depth >= 5 && depth <= 19; }
bool vault_template_has_aule(vault_type *v) { return v == &vaults[10]; }
bool vault_template_has_mandos(vault_type *v) { return v == &vaults[11]; }
bool vault_template_has_duruin(vault_type *v) { return v == &vaults[12]; }
bool parse_u64b_hex(cptr text, u64b *guid) { *guid = strtoull(text, NULL, 16); return true; }
s16b monster_lookup_guid(u64b guid) { switch(guid) {
'''
    for guid, serial in guids.items(): code += f'case 0x{guid:x}ULL: return {serial};\n'
    code += 'default: return 0; }}\n' + table + query + '\n' + available + '\n' + gate
    code += r'''
int main(void) {
    for(int i=0;i<700;++i) { races[i].max_num=1; races[i].flags1=RF1_UNIQUE; }
    for(size_t i=0;i<N_ELEMENTS(quest_tokens);++i) {
        int race=quest_vault_token_race(quest_tokens[i].vault,quest_tokens[i].token);
        assert(race>0);
        assert(quest_vault_token_race(520,quest_tokens[i].token)==0);
        assert(!vault_is_valid_for_depth(&vaults[quest_tokens[i].vault], 10));
        enabled=true;
        assert(vault_is_valid_for_depth(&vaults[quest_tokens[i].vault],10));
        races[race].max_num=0;
        assert(!vault_is_valid_for_depth(&vaults[quest_tokens[i].vault],10));
        races[race].max_num=1; races[race].cur_num=1;
        assert(!vault_is_valid_for_depth(&vaults[quest_tokens[i].vault],10));
        races[race].cur_num=0;
        assert(!vault_is_valid_for_depth(&vaults[quest_tokens[i].vault],20));
        enabled=false;
    }
    assert(vault_is_valid_for_depth(&vaults[520],10));
    assert(!vault_is_valid_for_depth(&vaults[10],10));
    assert(!vault_is_valid_for_depth(&vaults[11],10));
    assert(!vault_is_valid_for_depth(&vaults[12],10));
    enabled=true;
    assert(vault_is_valid_for_depth(&vaults[10],10));
    puts("Quest vaults: production token GUIDs, scoped meanings, disabled gates and unavailable uniques: PASS");
}
'''
    OUT.mkdir(parents=True,exist_ok=True)
    cfile=OUT/'check.c'; cfile.write_text(code,encoding='utf-8')
    exe=OUT/'check.exe'
    env=os.environ.copy();env['PATH']='C:/msys64/mingw64/bin;C:/msys64/usr/bin;'+env.get('PATH','')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','@CMakeFiles/sil-more.dir/includes_C.rsp',str(cfile),'-o',str(exe)],cwd=ROOT/'build-standard',env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=15)
if __name__=='__main__': main()

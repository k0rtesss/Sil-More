#!/usr/bin/env python3
"""Focused calculator regressions; no combinatorial full catalogue generation."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import calc_artefact_difficulty as calc

ROOT = Path(__file__).resolve().parents[1]


class DifficultyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.objects = calc.parse_object_file(ROOT / 'lib/edit/object.txt')
        cls.kinds = {obj['k_idx']: obj for obj in cls.objects}
        cls.egos = {ego['idx']: ego for ego in calc.parse_special_file(ROOT / 'lib/edit/special.txt')}
        cls.artefacts = {art['idx']: art for art in calc.parse_artefact_file(ROOT / 'lib/edit/artefact.txt')[0]}
        calc.populate_objects_dict(cls.objects)
        calc.parse_ability_file(ROOT / 'lib/edit/ability.txt')

    def test_insta_art_base_kinds_and_first_duplicate(self):
        self.assertEqual(calc.OBJECTS_BY_TYPE[40, 10]['k_idx'], 3)
        self.assertEqual({self.kinds[i]['k_idx'] for i in (19, 104, 108, 109)}, {19, 104, 108, 109})
        self.assertEqual(calc.generate_normal_variants([self.kinds[104]]), [])
        self.assertEqual(calc.generate_special_variants(self.egos[11], [self.kinds[104]]), [])
        self.assertEqual(calc.generate_dual_ego_variants([self.egos[11], self.egos[161]], [self.kinds[104]]), [])

    def test_fixed_artefacts_with_previously_wrong_bases(self):
        for idx, expected in {3: 29, 8: 65, 137: 50, 138: 37, 170: 21, 179: 35}.items():
            with self.subTest(idx=idx):
                self.assertEqual(calc.calculate_difficulty(dict(self.artefacts[idx], type='artefact')), expected)

    def test_normal_explicit_negative_bonuses(self):
        variant, = calc.generate_normal_variants([self.kinds[140]])
        self.assertEqual(variant['stat_bonus_values'], dict(STR=0, DEX=0, CON=1, GRA=-2))
        self.assertEqual(variant['skill_bonus_values']['WILL'], -2)
        self.assertEqual(calc.calculate_difficulty(variant), -16)

    def test_duplicate_kind_variant_uses_own_flags(self):
        variant, = calc.generate_normal_variants([self.kinds[142]])
        self.assertEqual(variant['stat_bonus_values']['GRA'], -1)
        self.assertIn('LIGHT_CURSE', variant['flags'])
        self.assertEqual(calc.calculate_difficulty(variant), 18)

    def test_single_ego_explicit_bonus_is_not_total_pval(self):
        variants = calc.generate_special_variants(self.egos[162], [self.kinds[23]])
        self.assertEqual({v['pval'] for v in variants}, {1, 2, 3})
        self.assertEqual({v['skill_bonus_values']['WILL'] for v in variants}, {-2})
        self.assertEqual({v['skill_bonus_values']['STEALTH'] for v in variants}, {1, 2, 3})

    def test_dual_ego_bonuses_add_and_keep_intrinsic_flags(self):
        variants = calc.generate_dual_ego_variants([self.egos[11], self.egos[161]], [self.kinds[30]])
        self.assertTrue(variants)
        for variant in variants:
            self.assertEqual(variant['skill_bonus_values']['STEALTH'], -4)
            self.assertEqual(variant['skill_bonus_values']['WILL'], -2)
            self.assertTrue(calc.component_flags(self.kinds[30]) <= set(variant['flags']))

    def test_independent_component_pvals_and_overrides(self):
        base = dict(flags=['STR', 'WILL'], pval=1, max_pval=2, bonus_overrides={'WILL': -2})
        ego = dict(flags=['STEALTH'], max_pval=2, bonus_overrides={'GRA': -2})
        variants = calc.component_bonus_variants(base, ego)
        self.assertEqual(len(variants), 4)
        self.assertEqual({(v['pval'], v['stat_bonus_values']['STR'], v['skill_bonus_values']['STEALTH'])
                          for v in variants}, {(2, 1, 1), (3, 1, 2), (3, 2, 1), (4, 2, 2)})
        self.assertEqual({v['skill_bonus_values']['WILL'] for v in variants}, {-2})
        self.assertEqual({v['stat_bonus_values']['GRA'] for v in variants}, {-2})

    def test_ego_explicit_ranges_sum_before_enumeration(self):
        base = dict(flags=[], pval=0, max_pval=0)
        first = dict(flags=[], max_pval=0, bonus_overrides={'STEALTH': (-3, -1)})
        second = dict(flags=[], max_pval=0, bonus_overrides={'STEALTH': (1, 2)})
        variants = calc.component_bonus_variants(base, first, second)
        self.assertEqual([v['skill_bonus_values']['STEALTH'] for v in variants], [-2, -1, 0, 1])

    def test_artefact_arrays_replace_base_arrays(self):
        art = dict(self.artefacts[3], type='artefact')
        original = calc.calculate_difficulty(art)
        with patch.dict(calc.OBJECTS_BY_TYPE, {(40, 10): dict(calc.OBJECTS_BY_TYPE[40, 10], flags=['STR'])}):
            self.assertEqual(calc.calculate_difficulty(dict(art)), original)

    def test_negative_stat_progression_and_c_truncation(self):
        self.assertEqual(calc.dif_mod_calc(2, 10), 22)
        self.assertEqual(calc.dif_mod_calc(-2, 3), -5)
        self.assertEqual(calc.c_trunc_div(-19, 10), -1)

    def test_negative_weapon_attack_uses_weapon_progression(self):
        variant = dict(type='normal', tval=23, sval=999, flags=[],
                       att=-2, evn=0, ds=0, pd=0, ps=0, pval=0)
        self.assertEqual(calc.calculate_difficulty(variant), -5)

    def test_negative_long_corslet_protection_uses_base_one(self):
        variant = dict(type='normal', tval=37, sval=6, flags=[],
                       att=0, evn=0, ds=0, pd=1, ps=1, pval=0,
                       base_pd=1, base_ps=2, weight=calc.get_base_weight(37, 6))
        self.assertEqual(calc.calculate_difficulty(variant), -1)

    def test_negative_tunnelling_uses_signed_progression(self):
        variant = dict(type='normal', tval=20, sval=1, flags=['TUNNEL'],
                       att=0, evn=0, ds=0, pd=0, ps=0, pval=1,
                       base_pval=2, weight=calc.get_base_weight(20, 1))
        self.assertEqual(calc.calculate_difficulty(variant), -4)

    def test_missing_artefact_base_is_an_error(self):
        art = dict(self.artefacts[3], type='artefact', sval=999)
        with self.assertRaisesRegex(ValueError, 'No base kind'):
            calc.calculate_difficulty(art)

    def test_comment_update_preserves_other_bytes_and_is_idempotent(self):
        art = self.artefacts[8]
        content = '# unrelated\r\n# Smithing difficulty: 1\r\n\r\nN:8:of Daeron\r\nI:33:11:3\r\n'
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'artefact.txt'
            target.write_bytes(content.encode())
            with redirect_stdout(StringIO()):
                self.assertEqual(calc.sync_difficulty_comments(target, [art], True), 1)
                self.assertEqual(calc.sync_difficulty_comments(target, [art]), 0)
            self.assertEqual(target.read_bytes(), content.replace('difficulty: 1', 'difficulty: 65').encode())

    def test_comment_cli_skips_all_variant_generation(self):
        with patch.object(calc, 'generate_normal_variants', side_effect=AssertionError('normal generation')), \
             patch.object(calc, 'generate_special_variants', side_effect=AssertionError('ego generation')), \
             patch.object(calc, 'generate_dual_ego_variants', side_effect=AssertionError('dual generation')), \
             redirect_stdout(StringIO()):
            self.assertEqual(calc.main(['--check-comments']), 0)

    def test_artefacts_only_csv_skips_all_variant_generation(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(calc, 'generate_normal_variants', side_effect=AssertionError('normal generation')), \
             patch.object(calc, 'generate_special_variants', side_effect=AssertionError('ego generation')), \
             patch.object(calc, 'generate_dual_ego_variants', side_effect=AssertionError('dual generation')), \
             redirect_stdout(StringIO()):
            target = Path(directory) / 'artefacts.csv'
            calc.main(['--artefacts-only', '--csv', str(target)])
            self.assertEqual(len(target.read_text(encoding='utf-8-sig').splitlines()), 131)


if __name__ == '__main__':
    unittest.main()

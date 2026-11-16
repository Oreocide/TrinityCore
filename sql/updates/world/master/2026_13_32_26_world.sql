-- Brew cooldown cuts (Brewmaster Monk): Keg Smash 121253, Brewmaster Tiger Palm 100780, Press the Advantage 418359
-- each DELETE is keyed on its own spell and name: Keg Smash keeps spell_monk_keg_smash_shuffle, Tiger Palm keeps spell_gen_trigger_exclude_caster_aura_spell
DELETE FROM `spell_script_names` WHERE (`spell_id`=121253 AND `ScriptName`='spell_monk_keg_smash_brews') OR (`spell_id`=100780 AND `ScriptName`='spell_monk_tiger_palm_brews') OR (`spell_id`=418359 AND `ScriptName`='spell_monk_press_the_advantage');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(121253,'spell_monk_keg_smash_brews'),
(100780,'spell_monk_tiger_palm_brews'),
(418359,'spell_monk_press_the_advantage');

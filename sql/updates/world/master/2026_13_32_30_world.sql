-- DUMMY-trigger proc filters, part 2: these talents procced on far more spells than their tooltips name
-- Shadow Embrace 32388, Burn to Ashes 387153, Light of the Titans 378405, Templar Strikes 406646, Commander of the Dead 390259 and
-- Lifespark 443177 get the class masks of the spells they name; Lively Totems 445034 and Primordial Storm 1218047 are filtered in their
-- scripts; Menacing Presence 441181 gets a script; Bind in Darkness 440031's own proc is off: its buff comes with Rime
-- (spell_dk_rime_bind_in_darkness on 59057)
DELETE FROM `spell_proc` WHERE `SpellId` IN (32388,387153,378405,406646,390259,443177,445034,1218047);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(32388,0,5,0x800001,0x0,0x0,0x0,0x51000,0x0,0x0,0x2,0x0,0x0,2,0,100,0,0),
(387153,0,5,0x80,0x2000,0x0,0x10000000,0x0,0x4,0x0,0x0,0x0,0x0,0,0,100,0,0),
(378405,0,10,0x0,0x0,0x4000,0x0,0x4000,0x0,0x0,0x2,0x0,0x0,2,0,100,0,0),
(406646,0,10,0x0,0x0,0x40,0x0,0x10,0x0,0x0,0x2,0x0,0x0,0,0,100,0,0),
(390259,0,15,0x0,0x0,0x20,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0,0,100,0,0),
(443177,0,224,0x100,0x0,0x0,0x0,0x204000,0x0,0x0,0x2,0x0,0x0,2,0,100,0,0),
(445034,0,0,0x0,0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,6,0,100,0,0),
(1218047,0,0,0x0,0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,6,0,100,0,0);
DELETE FROM `spell_script_names` WHERE (`spell_id`=440031 AND `ScriptName`='spell_dk_bind_in_darkness')
 OR (`spell_id`=59057 AND `ScriptName`='spell_dk_rime_bind_in_darkness')
 OR (`spell_id`=387153 AND `ScriptName`='spell_warl_burn_to_ashes')
 OR (`spell_id`=32388 AND `ScriptName`='spell_warl_shadow_embrace')
 OR (`spell_id`=441181 AND `ScriptName`='spell_evo_menacing_presence')
 OR (`spell_id`=1218047 AND `ScriptName`='spell_sha_primordial_storm_talent')
 OR (`spell_id`=445034 AND `ScriptName`='spell_sha_lively_totems');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(440031,'spell_dk_bind_in_darkness'),
(59057,'spell_dk_rime_bind_in_darkness'),
(387153,'spell_warl_burn_to_ashes'),
(32388,'spell_warl_shadow_embrace'),
(441181,'spell_evo_menacing_presence'),
(1218047,'spell_sha_primordial_storm_talent'),
(445034,'spell_sha_lively_totems');

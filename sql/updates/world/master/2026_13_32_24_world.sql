-- 81229 Runic Empowerment: each Runic Power spent may grant a Rune
-- the aura procs on a successful cast (CAST phase set by the loader); the script keeps casts that spent Runic Power
DELETE FROM `spell_proc` WHERE `SpellId`=81229;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(81229,0,0,0x0,0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0,0,100,0,0);

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_dk_runic_empowerment';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(81229,'spell_dk_runic_empowerment');

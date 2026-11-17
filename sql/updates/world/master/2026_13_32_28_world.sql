-- Counterstrike 383800 (Brewmaster Monk): spent only by Tiger Palm 100780 (word 0 0x400) or a Spinning Crane Kick tick 107270 (0x100);
-- the script keeps the effect 1 DUMMY from casting Spinning Crane Kick 101546
DELETE FROM `spell_proc` WHERE `SpellId`=383800;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(383800,0,53,0x500,0x0,0x0,0x0,0x10,0x0,0x0,0x2,0x0,0x0,0,0,100,0,0);
DELETE FROM `spell_script_names` WHERE `spell_id`=383800 AND `ScriptName`='spell_monk_counterstrike';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(383800,'spell_monk_counterstrike');

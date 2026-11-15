-- 1226140 Grove Guardians: Swiftmend / Wild Growth summon a Treant that casts Nourish
-- the talent procs on a successful cast (CAST phase set by the loader); the script keeps Swiftmend and Wild Growth
DELETE FROM `spell_proc` WHERE `SpellId`=1226140;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1226140,0,0,0x0,0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0,0,100,0,0);

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_dru_grove_guardians','spell_dru_grove_guardian_nourish');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1226140,'spell_dru_grove_guardians'),
(422090,'spell_dru_grove_guardian_nourish');

-- 54983 Treant (summoned only by 102693)
UPDATE `creature_template` SET `ScriptName`='npc_dru_grove_guardian' WHERE `entry`=54983;

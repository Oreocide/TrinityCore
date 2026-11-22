-- Empyrean Legacy buff 387178 (Retribution): "Your next Single target Holy Power ability automatically triggers Divine Storm with 25%
-- increased effectiveness." With no spell_proc row the generated entry (family mask 0, DB2 flags 0x4010) let the Avenging Wrath cast that
-- applies the buff use up its only charge right away. Only Templar's Verdict, Final Verdict and Justicar's Vengeance (family 10, mask word 2
-- 0x2000 | 0x1000000) use it now. Templar's Verdict is DUMMY-only (its damage is a triggered spell), so there is no spell type filter.
DELETE FROM `spell_proc` WHERE `SpellId`=387178;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(387178,0,10,0x0,0x0,0x1002000,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0,0,0,0);

DELETE FROM `spell_script_names` WHERE `spell_id`=387178 AND `ScriptName`='spell_pal_empyrean_legacy';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(387178,'spell_pal_empyrean_legacy');

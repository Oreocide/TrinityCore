-- 255652 Light's Reckoning: erupt on death (the aura's DUMMY effect casts 256896)
DELETE FROM `spell_proc` WHERE `SpellId`=255652;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(255652,0,0,0x0,0x0,0x0,0x0,0x01000000,0x0,0x0,0x0,0x0,0x0,0,0,100,0,0);

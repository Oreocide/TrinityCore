-- DUMMY-trigger proc filters: these talents name their effect in a DUMMY aura, so with no row the core cast it on every proc
-- Mindbender 1230339 (Shadow Priest): only the Voidform cast 228260 (word 1 0x10), cast successful
-- Demonic Calling 205145 (Demonology Warlock): Shadow Bolt / Demonbolt, 20 % chance
-- Chaotic Inferno 1244788 (Destruction Warlock): only Chaos Bolt (word 1 0x2000), cast successful, 25 % chance
-- Divine Revelations 387808 (Holy Paladin): Judgment (word 0 0x80000); the script needs Infusion of Light 54149
DELETE FROM `spell_proc` WHERE `SpellId` IN (1230339,205145,1244788,387808);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1230339,0,6,0x0,0x10,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0,0,100,0,0),
(205145,0,5,0x1,0x0,0x40,0x0,0x10000,0x0,0x0,0x2,0x0,0x0,6,0,20,0,0),
(1244788,0,5,0x0,0x2000,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,2,0,25,0,0),
(387808,0,10,0x80000,0x0,0x0,0x0,0x14010,0x0,0x0,0x2,0x0,0x0,2,0,100,0,0);
DELETE FROM `spell_script_names` WHERE (`spell_id`=1230339 AND `ScriptName`='spell_pri_mindbender_voidform') OR (`spell_id`=387808 AND `ScriptName`='spell_pal_divine_revelations');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1230339,'spell_pri_mindbender_voidform'),
(387808,'spell_pal_divine_revelations');

-- 53651 Light's Beacon: the proc needs a phase for its DB2 flags (DEAL_HELPFUL_SPELL); with SpellPhaseMask 0 it never procs
UPDATE `spell_proc` SET `SpellPhaseMask`=0x2 WHERE `SpellId`=53651;

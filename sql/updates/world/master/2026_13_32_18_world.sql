-- 1245979 Valiant Crusade: out-of-combat marker 1246042, Divine Steed grants Shield of the Righteous for its duration + 4 s
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_pal_valiant_crusade','spell_pal_divine_steed_valiant_crusade');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1245979,'spell_pal_valiant_crusade'),
(221883,'spell_pal_divine_steed_valiant_crusade'),
(221885,'spell_pal_divine_steed_valiant_crusade'),
(221886,'spell_pal_divine_steed_valiant_crusade'),
(221887,'spell_pal_divine_steed_valiant_crusade'),
(276111,'spell_pal_divine_steed_valiant_crusade'),
(276112,'spell_pal_divine_steed_valiant_crusade'),
(294133,'spell_pal_divine_steed_valiant_crusade'),
(363608,'spell_pal_divine_steed_valiant_crusade');

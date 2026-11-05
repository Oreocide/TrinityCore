-- Housing intro NPC Lyssabel Dawnpetal (233063 / 249850), menu 40502.
-- Retail sends one of several option sets depending on quest 91863 "My First Home" objective progress (sniff 12.1.5.70575 2026-10-23, packets #66221 #66436 #66904 #108171).
-- Objectives: 463615 Greet the steward (credit 249851), 463609 Ask the steward to join you (credit 248857, given by menu 40506 option 135770).
-- Conditions: 47 QUESTSTATE (1 not taken, 2 complete, 8 in progress, 64 rewarded), 48 QUEST_OBJECTIVE_PROGRESS (value3 = progress).
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=15 AND `SourceGroup`=40502 AND `SourceEntry` IN (2,3,4,9,10,11,12,13,17,18,19,20,21);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(15,40502,2,0,0,47,0,91863,65,0,'',0,0,0,'','Lyssabel menu 40502 base option (directions): My First Home not taken or already rewarded'),
(15,40502,3,0,0,47,0,91863,65,0,'',0,0,0,'','Lyssabel menu 40502 base option (other neighborhoods): My First Home not taken or already rewarded'),
(15,40502,4,0,0,47,0,91863,65,0,'',0,0,0,'','Lyssabel menu 40502 base option (found neighborhood): My First Home not taken or already rewarded'),
(15,40502,9,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set ((Quest) look around): My First Home in progress, Greet the steward not done'),
(15,40502,9,0,0,48,0,463615,0,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set ((Quest) look around): My First Home in progress, Greet the steward not done'),
(15,40502,10,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (directions): My First Home in progress, Greet the steward not done'),
(15,40502,10,0,0,48,0,463615,0,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (directions): My First Home in progress, Greet the steward not done'),
(15,40502,11,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (other neighborhoods): My First Home in progress, Greet the steward not done'),
(15,40502,11,0,0,48,0,463615,0,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (other neighborhoods): My First Home in progress, Greet the steward not done'),
(15,40502,12,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (found neighborhood): My First Home in progress, Greet the steward not done'),
(15,40502,12,0,0,48,0,463615,0,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (found neighborhood): My First Home in progress, Greet the steward not done'),
(15,40502,13,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (go back): My First Home in progress, Greet the steward not done'),
(15,40502,13,0,0,48,0,463615,0,0,'',0,0,0,'','Lyssabel menu 40502 first-talk set (go back): My First Home in progress, Greet the steward not done'),
(15,40502,17,0,0,47,0,91863,8,0,'',0,0,0,'','Lyssabel menu 40502 (Quest) look around: Greet the steward done, Ask the steward to join you not done'),
(15,40502,17,0,0,48,0,463615,0,1,'',0,0,0,'','Lyssabel menu 40502 (Quest) look around: Greet the steward done, Ask the steward to join you not done'),
(15,40502,17,0,0,48,0,463609,0,0,'',0,0,0,'','Lyssabel menu 40502 (Quest) look around: Greet the steward done, Ask the steward to join you not done'),
(15,40502,18,0,0,47,0,91863,10,0,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (directions): My First Home in progress or complete, Greet the steward done'),
(15,40502,18,0,0,48,0,463615,0,1,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (directions): My First Home in progress or complete, Greet the steward done'),
(15,40502,19,0,0,47,0,91863,10,0,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (other neighborhoods): My First Home in progress or complete, Greet the steward done'),
(15,40502,19,0,0,48,0,463615,0,1,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (other neighborhoods): My First Home in progress or complete, Greet the steward done'),
(15,40502,20,0,0,47,0,91863,10,0,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (found neighborhood): My First Home in progress or complete, Greet the steward done'),
(15,40502,20,0,0,48,0,463615,0,1,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (found neighborhood): My First Home in progress or complete, Greet the steward done'),
(15,40502,21,0,0,47,0,91863,10,0,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (go back): My First Home in progress or complete, Greet the steward done'),
(15,40502,21,0,0,48,0,463615,0,1,'',0,0,0,'','Lyssabel menu 40502 in-tutorial set (go back): My First Home in progress or complete, Greet the steward done');

-- Credits that retail grants on talking / on the "Let's go!" option (no scripting existed for these creatures).
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry` IN (233063,249850) AND `AIName`='';
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid` IN (233063,249850) AND `id` IN (0,1,2);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(233063,0,0,0,'',64,0,100,0,0,0,0,0,0,'',33,249851,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Hello - Kill Credit Greet the steward (My First Home)'),
(233063,0,1,2,'',62,0,100,0,40506,0,0,0,0,'',33,248857,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Option 0 Selected (menu 40506, Let''s go) - Kill Credit Ask the steward to join you'),
(233063,0,2,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Close Gossip'),
(249850,0,0,0,'',64,0,100,0,0,0,0,0,0,'',33,249851,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Hello - Kill Credit Greet the steward (My First Home)'),
(249850,0,1,2,'',62,0,100,0,40506,0,0,0,0,'',33,248857,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Option 0 Selected (menu 40506, Let''s go) - Kill Credit Ask the steward to join you'),
(249850,0,2,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Close Gossip');

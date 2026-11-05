-- Lyssabel Dawnpetal (233063): "(Quest) Let's go!" (menu 40506 option 0) in retail makes the PLAYER cast
--   1250436 Tutorial Guardian Accepted (KILL_CREDIT 248857, 233708, 233063: quest 91863 objectives 464280/464281/463609) and
--   1250473 Lyssabel Guardian Aura (LINKED_SUMMON -> 1250472 summons creature 249850 as a guardian, + credit 248857)
-- (sniff 12.1.5.70575 2026-10-23_14-37-06, select at packet #66732, spells right after). Replaces the plain credit added 2026-10-25.
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid` IN (233063,249850) AND `id` IN (1,2,3);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(233063,0,1,3,'',62,0,100,0,40506,0,0,0,0,'',134,1250436,2,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Option 0 Selected (menu 40506, Let''s go) - Invoker Cast Tutorial Guardian Accepted'),
(233063,0,3,2,'',61,0,100,0,0,0,0,0,0,'',134,1250473,2,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Invoker Cast Lyssabel Guardian Aura'),
(233063,0,2,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Close Gossip'),
(249850,0,1,3,'',62,0,100,0,40506,0,0,0,0,'',134,1250436,2,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - On Gossip Option 0 Selected (menu 40506, Let''s go) - Invoker Cast Tutorial Guardian Accepted'),
(249850,0,3,2,'',61,0,100,0,0,0,0,0,0,'',134,1250473,2,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Invoker Cast Lyssabel Guardian Aura'),
(249850,0,2,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Lyssabel Dawnpetal - Linked - Close Gossip');
-- 249850 is the summoned guardian, not a static NPC: remove the import's stacked spawn (99001233, same position as 233063).
DELETE FROM `creature_addon` WHERE `guid`=99001233;
DELETE FROM `creature` WHERE `guid`=99001233 AND `id`=249850;

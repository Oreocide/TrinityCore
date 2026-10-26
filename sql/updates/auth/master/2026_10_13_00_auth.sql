DELETE FROM `build_info` WHERE `build`=70340;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70340,12,1,5,NULL);

DELETE FROM `build_auth_key` WHERE `build`=70340 AND `platform`='Win' AND `arch`='x64' AND `type`='WoW';
INSERT INTO `build_auth_key` (`build`,`platform`,`arch`,`type`,`key`) VALUES
(70340,'Win','x64','WoW',0xC438D2744972A7D6885915616C212F52);

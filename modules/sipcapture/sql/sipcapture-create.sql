

DROP TABLE IF EXISTS `sip_capture`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture` (
  `id` int(20) NOT NULL AUTO_INCREMENT,
  `date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `micro_ts` bigint(18) NOT NULL DEFAULT '0',
  `method` varchar(50) NOT NULL DEFAULT '',
  `reply_reason` varchar(100) NOT NULL,
  `ruri` varchar(200) NOT NULL DEFAULT '',
  `ruri_user` varchar(100) NOT NULL DEFAULT '',
  `from_user` varchar(100) NOT NULL DEFAULT '',
  `from_tag` varchar(64) NOT NULL DEFAULT '',
  `to_user` varchar(100) NOT NULL DEFAULT '',
  `to_tag` varchar(64) NOT NULL,
  `pid_user` varchar(100) NOT NULL DEFAULT '',
  `contact_user` varchar(120) NOT NULL,
  `auth_user` varchar(120) NOT NULL,
  `callid` varchar(100) NOT NULL DEFAULT '',
  `callid_aleg` varchar(100) NOT NULL DEFAULT '',
  `via_1` varchar(256) NOT NULL,
  `via_1_branch` varchar(80) NOT NULL,
  `cseq` varchar(25) NOT NULL,
  `diversion` varchar(256) NOT NULL,
  `reason` varchar(200) NOT NULL,
  `content_type` varchar(256) NOT NULL,
  `authorization` varchar(256) NOT NULL,
  `user_agent` varchar(256) NOT NULL,
  `source_ip` varchar(50) NOT NULL DEFAULT '',
  `source_port` int(10) NOT NULL,
  `destination_ip` varchar(50) NOT NULL DEFAULT '',
  `destination_port` int(10) NOT NULL,
  `contact_ip` varchar(60) NOT NULL,
  `contact_port` int(10) NOT NULL,
  `originator_ip` varchar(60) NOT NULL DEFAULT '',
  `originator_port` int(10) NOT NULL,
  `proto` int(5) NOT NULL,
  `family` int(1) DEFAULT NULL,
  `rtp_stat` varchar(256) NOT NULL,
  `type` int(2) NOT NULL,
  `node` varchar(125) NOT NULL,
  `msg` longblob NOT NULL,
  PRIMARY KEY (`id`,`date`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8
PARTITION BY RANGE ( TO_DAYS(`date`) ) (
PARTITION p20110824 VALUES LESS THAN (734739) ENGINE = MyISAM,
PARTITION p20110825 VALUES LESS THAN (734740) ENGINE = MyISAM,
PARTITION p20110826 VALUES LESS THAN (734741) ENGINE = MyISAM,
PARTITION p20110827 VALUES LESS THAN (734742) ENGINE = MyISAM,
PARTITION p20110828 VALUES LESS THAN (734743) ENGINE = MyISAM,
PARTITION p20110829 VALUES LESS THAN (734744) ENGINE = MyISAM,
PARTITION p20110830 VALUES LESS THAN (734745) ENGINE = MyISAM,
PARTITION p20110831 VALUES LESS THAN (734746) ENGINE = MyISAM,
PARTITION p20110901 VALUES LESS THAN (734747) ENGINE = MyISAM,
PARTITION p20110902 VALUES LESS THAN (734748) ENGINE = MyISAM,
PARTITION pmax VALUES LESS THAN (MAXVALUE)
);

/* alter table homer_capture add partition (PARTITION p20110903 VALUES LESS THAN (734749) ENGINE = MyISAM); */
/* if your mysql < 5.5 drop maxvalue partition */
/* alter table sip_capture drop partition pmax; */

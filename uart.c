static U32 uart_info_upgrade_trans_data(pUartInfo_t uinfo, char *name)
{
	int fd, i, j, k, ret, cur, escacpe = 0;
	size_t file_size, persize, statsize = 0, remain_size = 0;
	U16 tp[32];
	Ushort_to_byte_u file_crc_raw, pack_crc, crc_con;
	struct timeval tv1, tv2;

	crc_con.byte2 = 0xffff;
	U8 *shbuf = calloc(1, 288);
	U8 *tmpbuf = calloc(1, 288);

	fd = open(name, O_RDONLY);
	printf("[%s][%d]File path: %s, fd: %d\n", __func__, __LINE__, name, fd);
	if (fd == -1) {
		printf("open file %s error!\n", name);
		return FAILURE;
	}

	remain_size = file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	//check file crc value
	gettimeofday(&tv1, NULL);
	file_crc_raw.byte2 = 0xffff;
	for (i = 0; i < file_size / 248; i++) {
		read(fd, shbuf, 248);
		file_crc_raw.byte2 = CRC16Continue(shbuf, 248, file_crc_raw.byte2);
//		printf("file crc check crc0: %#x, crc1: %#x\n", (int)file_crc_raw.byte[1], (int)file_crc_raw.byte[0]);
//		if ((i + 1) % 6 ==0) printf("\n");
	}

#if 0
	gettimeofday(&tv2, NULL);
	printf("file crc check over, time use:  %dus, crc0: %#x, crc1: %#x, remain: %d, filesize: %d\n",
		((tv2.tv_sec-tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec),
		(int)file_crc_raw.byte[1], (int)file_crc_raw.byte[0], remain_size, file_size);
#endif
	if (file_size % 248) {
		read(fd, shbuf, file_size % 248);
		file_crc_raw.byte2 = CRC16Continue(shbuf, file_size % 248, file_crc_raw.byte2);
	}
	gettimeofday(&tv2, NULL);
	printf("file crc check over, time use:  %dus, crc0: %#x, crc1: %#x, remain: %d, filesize: %d\n",
			((tv2.tv_sec-tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec),
			(int)file_crc_raw.byte[1], (int)file_crc_raw.byte[0], remain_size, file_size);
	shbuf[281] = file_size >> 8;
	shbuf[280] = file_size;
	shbuf[282] = file_crc_raw.byte[0];
	shbuf[283] = file_crc_raw.byte[1];
	memset(shbuf, 0, 280);
	memset(tp, 0, 32);
	lseek(fd, 0, SEEK_SET);
	k = ((file_size % 248) ? (file_size / 248 + 1) : (file_size / 248));
	while(k > 0) {
		j = 0;
//		printf("k===>%d\n", k);
		persize = (k == 1 ? (file_size % 248) : 248);
		memset(shbuf, 0, 280);
		memset(tp, 0, 32);
		memset(tmpbuf, 0, 288);
		read(fd, shbuf, persize);

		pack_crc.byte2 = CRC16Continue(shbuf, persize, 0xffff);
		crc_con.byte2 = CRC16Continue(shbuf, persize, crc_con.byte2);
		printf(">>>>>>>>>crc.1: %#x, crc.0: %#x, crcfile.1: %#x, crcfile.2: %#x\n",
				pack_crc.byte[1], pack_crc.byte[0], crc_con.byte[1], crc_con.byte[0]);
		// escape policy
		for (i = 0; i < 248; i++) {		//escape mechanism
			if (shbuf[i] == 0x7e) {
				tp[j] = i | 0x8000;
				j++;
			} else if (shbuf[i] == 0x7d) {
				tp[j] = i | 0x4000;
				j++;
			}
		}
		if (j != 0) {
			memcpy(tmpbuf, shbuf, 280);
			for (i = 0; i < j; i++) {
				int d;
				cur = (((i == j - 1) ? 248 : (tp[i + 1] & 0xfff)) - (tp[i] & 0xfff) - 1);
				printf("$$$$$$$$$$$$$$$$$ file grows by %d bytes, tp[%d]: %d, tp[i + 1]: %d, cur: %d\n", j, i, tp[i] & 0xfff, tp[i + 1] & 0xfff, cur);
				memcpy(&shbuf[(tp[i] & 0xfff) + i + 2], &tmpbuf[(tp[i] & 0xfff) + 1], cur);
#if 0				//for escape policy debug
				for(d = 0; d < cur; d++) {
					printf("tm%d: %#x    \t", (tp[i] & 0xfff) + 1 + d, tmpbuf[(tp[i] & 0xfff) + 1 + d]);
					printf("sh%d: %#x    \t", (tp[i] & 0xfff) + 2 + i + d, shbuf[(tp[i] & 0xfff) + 2 + i + d]);
					if ((d + 1) % 5 == 0)
						printf("\n");
				}
				printf("\n");
#endif
				if (tp[i] & 0x8000) {
					shbuf[(tp[i] & 0xfff) + i + 1] = 0x01;
				} else if (tp[i] & 0x4000) {
					shbuf[(tp[i] & 0xfff) + i + 1] = 0x02;
				}
				shbuf[(tp[i] & 0xfff) + i] = 0x7d;
			}

		escacpe += j;
		}

		remain_size -= persize;
		k--;
		statsize += persize;

		shbuf[284] = k;
		shbuf[285] = j;
		shbuf[286] = persize;
		shbuf[287] = 0;		//reserved
		ugThrInfo.pData = (char *)shbuf;
		ugThrInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;

		printf("=====_-_-_-_-_-_-_==>>file: %#x, remain: %#x, statistic: %#x, escacpe: %d, per: %f%%\n",
				file_size, remain_size, statsize, escacpe, (double)statsize / file_size * 100);

#if 0
		for (i = 280; i < 288; i++)
			printf("[%s][%d]shbuf[%d]: %#x\n", __func__, __LINE__, i, shbuf[i]);
		printf("[%s][%d]--------------------------------------\n", __func__, __LINE__);
#endif

		sendMsg(ugThrId, &ugThrInfo);
		upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
		ret = recMsg(upgradeId, &upgradeInfo);			//waiting for upgrade task thread
//		printf("[%s][%d]received msg, loop on, upgradeInfo.iDataLen: %d...\n", __func__, __LINE__, upgradeInfo.iDataLen);
		if (upgradeInfo.iDataLen == -1 || ret == -1) {		//error
			upgradeInfo.iDataLen = 0;
			goto fail;
		} else if (upgradeInfo.iDataLen == 9){	//finished
			ugThrInfo.iDataLen = 9;
			ugThrInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
			sendMsg(ugThrId, &ugThrInfo);
			break;
		} else if (upgradeInfo.iDataLen == 0){
			continue;
		} else {
			printf("error msg!\n");
			goto fail;
		}
	}

	printf("finished!\n");
	close(fd);
	sleep(1);
	free(shbuf);
	shbuf = NULL;
	return SUCCESS;
fail:
	close(fd);
	sleep(1);
	free(shbuf);
	shbuf = NULL;
	return FAILURE;
}

static U32 uart_info_upgrade_terminate(void)
{
	on_off = 0;

	return SUCCESS;
}

static void *uart_info_upgrade_task(void *arg)
{
	U8 *buf;
	int i, ret;
	Ushort_to_byte_u file_crc, file_crc_inall;

	pUartInfo_t uinfo = arg;
	file_crc_inall.byte2 = file_crc.byte2 = 0xffff;

	prctl(PR_SET_NAME, (unsigned long)__func__);
	while(1) {
		//check on_off to decide whether terminate the upgrade
		//show the upgrade proccess every 0.5s
		ugThrInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
		recMsg(ugThrId, &ugThrInfo);			//waiting for upgrade data
		if (ugThrInfo.iDataLen == 9) {
			ugThrInfo.iDataLen = 0;
			break;
		}
//		printf("[%s][%d], msg received from  file...\n", __func__, __LINE__);
		buf = (U8 *)ugThrInfo.pData;
		if (buf[284] == 0) {						//last transmission
			printf("last, packet size~persize~buf[286]: %d, buf[284]~k: %d, buf[285]~j: %d ===========\n", buf[286], buf[284], buf[285]);
			uinfo->data_send[1] = buf[286] + buf[285]; 		//single packet size
			memcpy(&uinfo->data_send[6], &buf[0], uinfo->data_send[1]);
			uinfo->data_size = uinfo->data_send[1] + 6;
			uinfo->data_send[0] = on_off;				//terminate upgrading?
			uinfo->data_send[2] = buf[280];
			uinfo->data_send[3] = buf[281];				//high?
			uinfo->data_send[4] = buf[282];
			uinfo->data_send[5] = buf[283];
			uinfo->funcID = FuncID_System_UpGrade;

			uinfo->data_send[287] = 0x89;
			ret = uart_info_send_func(uinfo, FuncID_System_UpGrade, 0x30, uart_upgrade_sig_handler);
			if (ret != 0) {
				printf("no ack, failed!\n");
				upgradeInfo.iDataLen = -1;		//end trans
				upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
				sendMsg(upgradeId, &upgradeInfo);
				goto end;
			}
			if (i == (buf[999]) || (i == (buf[999] - 1) && buf[997] == 0)) {
				upgradeInfo.iDataLen = 0x9;		//end trans
				upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
				sendMsg(upgradeId, &upgradeInfo);
				break;
			}
			upgradeInfo.iDataLen = 0;		//continue
			upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
			sendMsg(upgradeId, &upgradeInfo);
			
		} else {
			printf("*****packet size~persize~buf[286]: %d, buf[284]~k: %d, buf[285]~j: %d ===========\n", buf[286], buf[284], buf[285]);
			uinfo->data_send[1] = buf[286] + buf[285]; 		//single packet size
			memcpy(&uinfo->data_send[6], buf, uinfo->data_send[1]);
			uinfo->data_size = uinfo->data_send[1] + 6;
			uinfo->data_send[0] = on_off;				//terminate upgrading?
			uinfo->data_send[2] = buf[280];
			uinfo->data_send[3] = buf[281];				//high?
			uinfo->data_send[4] = buf[282];
			uinfo->data_send[5] = buf[283];
			uinfo->funcID = FuncID_System_UpGrade;
			
			uinfo->data_send[287] = 0x89;
			ret = uart_info_send_func(uinfo, FuncID_System_UpGrade, 0x30, uart_upgrade_sig_handler);
			if (ret != 0) {
				printf("no ack, failed!\n");
				upgradeInfo.iDataLen = -1;		//end trans
				upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
				sendMsg(upgradeId, &upgradeInfo);
				goto end;
			}
			upgradeInfo.iDataLen = 0;		//continue
			upgradeInfo.lMsgType = MSG_TYPE_UPGRADE_ISR;
			sendMsg(upgradeId, &upgradeInfo);
		}
	}

	printf("thread end here!\n");
end:
	pthread_exit(NULL);
}

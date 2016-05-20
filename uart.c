{
	U8 shbuf[288], tmpbuf[288];

	memcpy(shbuf, tmpbuf, 280);
	for (i = 0; i < j; i++) {
		cur = (((i == j - 1) ? 248 : (tp[i] & 0xfff + 1)) - (tp[i] & 0xfff) - 1);
		memcpy(&shbuf[(tp[i] & 0xfff) + i + 2], &tmpbuf[(tp[i] & 0xfff) + 1], cur);
	}
}


audacity workflow: 
	convert to 16bit and mono
	export audio
	save as type uncompressed other audio
	header RAW
	encoding signed 16-bit PCM
	rename to filename.bin

main sound func (https://github.com/smealum/ctrulib/blob/master/libctru/source/services/csnd.c)
	example
	csndPlaySound(8, SOUND_FORMAT_16BIT | SOUND_REPEAT, 44100, 1, 0, buffer, buffer, size);
	csndPlaySound(channel, flags, khz, volume, pan, buffer, buffer, size);
	
	channel is used for L/R and playing multiple files at once
	volume is usually set at 1
	pan+ is R and pan- is L, 9 and -9 appear to be sufficient for stereo
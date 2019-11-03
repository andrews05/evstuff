#!/usr/bin/python
#
# snd2aif.py
# Convert 'snd ' resources directly to AIFF files with no transcoding
# (c) 2017 Andrew Simmonds
#
# References
# aifc.py: https://svn.python.org/projects/python/trunk/Lib/aifc.py
# Sound Manager: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Sound/Sound-44.html

import sys, aifc, struct
from os.path import basename
sys.tracebacklimit = 0
if len(sys.argv) < 2:
	print "Usage: "+basename(sys.argv[0])+" <output aiff file>"
	sys.exit(1)

from Carbon.Sound import * # Sound constants

# aifc has limited support for compression types and will throw an exception. We don't need to convert the data so just hack it to ignore the exception.
aifc.Aifc_write._init_compression = lambda self: None

af = aifc.open(sys.argv[1], 'w')
data = sys.stdin.read()

# Parse resource header
format, = struct.unpack_from('<H', data, 0)
if format == firstSoundFormat:
	numDatas, dataType = struct.unpack_from('<HH', data, 2)
	if numDatas != 1 or dataType != sampledSynth: raise Exception("Unsupported sound format.")
	cmdOffset = 6 + numDatas*6
elif format == secondSoundFormat:
	cmdOffset = 6
else:
	raise Exception("Unsupported sound format.")
command, soundOffset = struct.unpack_from('<H2xL', data, cmdOffset)
if command != dataOffsetFlag+bufferCmd: raise Exception("Unsupported sound format.")

# Parse sound header
nchannels, framerate, encode = struct.unpack_from('<4xLL8xB', data, soundOffset)
if encode == stdSH:
	comptype = k8BitOffsetBinaryFormat
	sampwidth = 8
	nframes = nchannels
	nchannels = 1
	dataOffset = soundOffset + 22
else:
	nframes, = struct.unpack_from('<L', data, soundOffset+22)
	if encode == extSH:
		sampwidth, = struct.unpack_from('<H', data, soundOffset+48)
		if sampwidth == 8: comptype = k8BitOffsetBinaryFormat
		else: comptype = k16BitLittleEndianFormat
	elif encode == cmpSH:
		comptype, compressionID, sampwidth = struct.unpack_from('<4s12xH4xH', data, soundOffset+40)
		if compressionID == threeToOne:
			comptype = kMACE3Compression
		elif compressionID == sixToOne:
			comptype = kMACE6Compression
		else:
			comptype = comptype[::-1]
	dataOffset = soundOffset + 64

if comptype != k16BitBigEndianFormat: af.aifc()
af.setparams((nchannels, sampwidth/8, float(framerate)/0x10000, nframes, 'NONE', ''))
af._comptype = comptype # Set comptype manually to avoid an exception
af.writeframesraw(buffer(data, dataOffset))
af._nframeswritten = nframes # Reset nframes
af.close()

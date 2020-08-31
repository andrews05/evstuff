#!/usr/bin/python
#
# aif2snd.py
# Convert AIFF files directly to 'snd ' resources with no transcoding
# (c) 2017 Andrew Simmonds
#
# References
# aifc.py: https://svn.python.org/projects/python/trunk/Lib/aifc.py
# Sound Manager: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Sound/Sound-44.html

import sys, aifc, struct
from os.path import basename
sys.tracebacklimit = 0
if len(sys.argv) < 2:
	print "Usage: "+basename(sys.argv[0])+" <input aiff file>"
	sys.exit(1)

from Carbon.Sound import * # Sound constants

# aifc has limited support for compression types and will throw an exception. We don't need to convert the data so just hack it to ignore the exception.
def readComm(self, chunk):
	try:
		self.readComm(chunk)
		if self._comptype == kSoundNotCompressed: self._comptype = k16BitBigEndianFormat # 'NONE' = 'twos'
	except aifc.Error as e:
		if self._sampwidth == 0: self._sampwidth = 2 # Some 16-bit compressed formats don't set the sample width - assume 2
aifc.Aifc_read.readComm = aifc.Aifc_read._read_comm_chunk
aifc.Aifc_read._read_comm_chunk = readComm


af = aifc.open(sys.argv[1])

# Determine header type
comptype = af.getcomptype()
nchannels = af.getnchannels()
if comptype == k8BitOffsetBinaryFormat and nchannels == 1:
	encode = stdSH # Standard header
	nchannels = af.getnframes() # Standard header stores nframes in place of nchannels
elif comptype == k8BitOffsetBinaryFormat or comptype == k16BitLittleEndianFormat:
	encode = extSH # Extended header
else:
	encode = cmpSH # Compressed header

# Create type 1 snd resource header
data = struct.pack('<HHHLHHHL',
                   firstSoundFormat, #format
                   1, #number of data types
                   sampledSynth, #data type
                   initStereo, #initialization options, unused
                   1, #number of sound commands
                   dataOffsetFlag+bufferCmd, #sound command
                   0, #param1, ignored
                   20) #param2, offset to sound header
# Add sound header
data += struct.pack('<LLL8xBB',
                    0, #samplePtr, 0 = after header
	                nchannels, #numChannels (or length if stdSH)
                    af.getframerate()<<16, #sampleRate
                    encode, #encode
                    kMiddleC) #baseFrequency, unused
if encode != stdSH:
	data += struct.pack('<L6xHH4x',
	                    af.getnframes(), #numFrames
	                    af.getframerate(), 0x400E) #sampleRate (10-byte float), unused
if encode == extSH:
	data += struct.pack('<8xH14x', af.getsampwidth()*8) #sampleSize
elif encode == cmpSH:
	data += struct.pack('<4s12xhH2xH',
	                    comptype[::-1], #format, reverse bytes
	                    fixedCompression, #compressionID, only fixedCompression currently supported (excludes MACE)
	                    0, #packetSize, 0 = auto
	                    af.getsampwidth()*8) #sampleSize
# Add sound data
af._ssnd_chunk.seek(8) # Seek past header fields
data += af._ssnd_chunk.read() # Read entire chunk directly rather than calling readframes()

sys.stdout.write(data)

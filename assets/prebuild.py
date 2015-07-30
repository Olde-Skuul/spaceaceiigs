#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
import subprocess
import burger

#
# Copy movies and audio from a folder
# Convert .wav files to 4 bit audio
#

def convertdata(soundexename,videoexename,srcfolder,destfolder):
	filelist = os.listdir(srcfolder)
	error = 0
	for item in filelist:
		
		#
		# If a sound file, convert to 4 bits per sample
		if item.lower().endswith('.wav'):
			src = os.path.join(srcfolder,item)
			dest = os.path.join(destfolder,item[:-4])
			
			# Only if newer
			if burger.isthesourcenewer(src,dest)==True:
				cmd = soundexename + ' -s "' + src + '" "' + dest + '"'
				error = subprocess.call(cmd,cwd=srcfolder,shell=True)
		elif item.lower().endswith('.gif'):
			
			# Copy any other file as is
			src = os.path.join(srcfolder,item)
			dest = os.path.join(destfolder,item[:-4])

			# Only if newer
			if burger.isthesourcenewer(src,dest)==True:
				cmd = videoexename + ' -v "' + src + '" "' + dest + '"'
				error = subprocess.call(cmd,cwd=srcfolder,shell=True)
		
		# Abort on error
		if error!=0:
			break
			
	return error
	
#
# Copy the data files for Space Ace for the Apple IIgs
#

def main(workingDir):
	
	#
	# Make sure the tool is built
	#
	
	toolfolder = os.path.dirname(workingDir)
	destfolder = os.path.join(toolfolder,'bin')
	toolfolder = os.path.join(toolfolder,'tools','bin')
	soundexename = burger.gettoolpath(toolfolder,'packsound',True)
	videoexename = burger.gettoolpath(toolfolder,'packvideo',True)
			
	#
	# Prepare for the output
	#
	
	burger.createfolderifneeded(destfolder)

	#
	# Copy the data files
	#
	
	srcfolder = os.path.join(workingDir,'movie')
	error = convertdata(soundexename,videoexename,srcfolder,destfolder)
	
	if error!=0:
		return error
		
	srcfolder = os.path.join(workingDir,'death')
	error = convertdata(soundexename,videoexename,srcfolder,destfolder)
	
	return error

# 
# If called as a function and not a class,
# call my main
#

if __name__ == "__main__":
	sys.exit(main(os.path.dirname(os.path.abspath(__file__))))

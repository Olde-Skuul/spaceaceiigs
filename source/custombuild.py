#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import subprocess
import shutil
import burger
	
#
# Compile the code and data for Space Ace IIgs
#

def main(workingDir):
	
	#
	# Create the output folder if needed
	#
	
	toolfolder = os.path.dirname(workingDir)
	destfolder = os.path.join(toolfolder,'bin')
	burger.createfolderifneeded(destfolder)

	#
	# Update the map files
	#

	filelist = [
		['icon','spaceace.icon#ca0000'],	# Icon file
		['buildall','SpaceAce#b3db03'],		# Application file
		['spaceacerez','SpaceAce#b3db03r']	# Resource file
	]

	error = 0
	
	for item in filelist:
		src = os.path.join(workingDir,item[0] + '.a65')
		dest = os.path.join(destfolder,item[1])
		if burger.isthesourcenewer(src,dest)==True:
			cmd = 'a65816 . ' + item[0] + '.a65'
			error = subprocess.call(cmd,cwd=workingDir,shell=True)
			if error!=0:
				return error

	#
	# Do some cleanup
	#
	
	burger.deletefileifpresent(os.path.join(workingDir,'_FileInformation.txt'))
	return error

# 
# If called as a function and not a class,
# call my main
#

if __name__ == "__main__":
	sys.exit(main(os.path.dirname(os.path.abspath(__file__))))



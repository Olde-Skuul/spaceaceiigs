/***************************************

	Tool to pre-process video data for Space Ace IIgs

	Copyright (c) 1995-2015 by Rebecca Ann Heineman <becky@burgerbecky.com>

	It is released under an MIT Open Source license. Please see LICENSE
	for license details. Yes, you can use it in a
	commercial title without paying anything, just give me a credit.
	Please? It's not like I'm asking you for money!

***************************************/

#include "packvideo.h"

/***************************************

	Convert the IIgs palette to RGBAWord8_t

***************************************/

static void BURGER_API ConvertPalette(RGBAWord8_t *pOutput,const Word8 *pInput)
{
	Word uIndex = 0;
	do {
		pOutput->m_uRed = Renderer::RGB4ToRGB8Table[pInput[1]&0xFU];
		pOutput->m_uGreen = Renderer::RGB4ToRGB8Table[pInput[0]>>4U];
		pOutput->m_uBlue = Renderer::RGB4ToRGB8Table[pInput[0]&0xFU];
		pOutput->m_uAlpha = 0xFF;
		pInput+=2;
		++pOutput;
	} while (++uIndex<16);
}

/***************************************

	Convert the RGBAWord8_t palette to IIgs

***************************************/

static void BURGER_API ConvertPalette(Word8 *pOutput,const RGBAWord8_t *pInput)
{
	Word uIndex = 0;
	do {
		Word uTemp = pInput->m_uGreen&0xF0U;
		uTemp |= (pInput->m_uBlue>>4U);
		pOutput[0] = static_cast<Word8>(uTemp);
		pOutput[1] = static_cast<Word8>(pInput->m_uRed>>4U);
		++pInput;
		pOutput+=2;
	} while (++uIndex<16);
}

/***************************************

	Test if the IIgs palette has changed

***************************************/

static Word BURGER_API ComparePalette(const Word8 *pInput1,const Word8 *pInput2)
{
	return MemoryCompare(pInput1,pInput2,32);
}

/***************************************

	Convert bitmap to IIgs format

	8 bits per pixel converted to 4 bits per pixel

***************************************/

static void BURGER_API ConvertPixelsToIIgs(Word8 *pOutput,const Image *pInput)
{
	WordPtr uStride = pInput->GetStride()-320;
	const Word8 *pPixels = pInput->GetImage();
	WordPtr j=200;
	do {
		WordPtr i=320/2;
		do {
			Word uTemp = pPixels[0]<<4U;
			uTemp |= (pPixels[1]&0xFU);
			pOutput[0] = static_cast<Word8>(uTemp);
			pPixels+=2;
			++pOutput;
		} while (--i);
		pPixels+=uStride;
	} while (--j);
}

/***************************************

	Compress a IIgs keyframe

	The compression only packs runs of a minimum of 3 matching
	bytes to reduce mode switching during decompression

***************************************/

static void BURGER_API CompressKeyFrame(OutputMemoryStream *pOutput,const Word8 *pInput)
{
	// Number of bytes to process
	WordPtr uInputLength = 320*200/2;

	do {
		// If two bytes or less, just store and exit
		if (uInputLength<3) {
			pOutput->Append(static_cast<Word8>(uInputLength));
			pOutput->Append(pInput,uInputLength);
			break;
		}

		// Check for repeater of at LEAST three bytes
		Word uMatchTest = pInput[0];

		// Is there a run?
		if ((pInput[1] == uMatchTest) && (pInput[2] == uMatchTest)) {

			// Maximum length of a matched run
			WordPtr uMaximumRun = 127;
			if (uInputLength<127) {
				uMaximumRun = uInputLength;		// 1-127
			}
			WordPtr uRun = 3-1;
			while (++uRun<uMaximumRun) {
				// Find end of repeater
				if (pInput[uRun]!=uMatchTest) {
					break;
				}
			}
			// Encode 128-255 for 0-127 run
			pOutput->Append(static_cast<Word8>(0x80|uRun));
			pOutput->Append(static_cast<Word8>(uMatchTest));
			uInputLength-=uRun;
			pInput += uRun;
		} else {
			// Raw run, minimum size of 2 bytes
			WordPtr uMaximumRun = 127;
			if (uInputLength<127) {
				uMaximumRun = uInputLength;
			}
			// Preload the next byte
			uMatchTest = pInput[1];
			WordPtr uRun = 2-1;
			while (++uRun<uMaximumRun) {	
				// Scan for next repeater
				if (pInput[uRun]==uMatchTest && (pInput[uRun+1]==uMatchTest)) {
					// Remove from the run
					--uRun;
					break;
				}
				// Get the next byte
				uMatchTest = pInput[uRun];
			}
			// Perform a raw data transfer
			// Run 1-128
			// Encode 0-127
			pOutput->Append(static_cast<Word8>(uRun));
			pOutput->Append(pInput,uRun);
			uInputLength-=uRun;
			pInput += uRun;
		}
	} while (uInputLength);

	// Mark the end of compressed data
	pOutput->Append(static_cast<Word8>(0));
}

/***************************************

	Compress a IIgs animation frame

***************************************/

static void BURGER_API CompressAnimFrame(OutputMemoryStream *pOutput,const Word8 *pPreviousFrame,const Word8 *pCurrentFrame)
{
	// Number of bytes to process
	WordPtr uInputLength = 320*200/2;
	do {
		// Check if there were any differences between the frames to create a skip token
		WordPtr uMaximumRun = 127;		// Skip token maximum value
		if (uInputLength<uMaximumRun) {
			uMaximumRun = uInputLength;
		}

		// Test from the previous frame to the current frame
		WordPtr uRun = 0;
		do {
			if (pPreviousFrame[uRun]!=pCurrentFrame[uRun]) {
				break;
			}
		} while (++uRun<uMaximumRun);

		// If the run is at least 2 bytes or end of the data, use it as is

		if ((uRun==uInputLength) || (uRun>=3)) {

			// Output a "skip" data token
			pOutput->Append(static_cast<Word8>(uRun));
			pPreviousFrame+=uRun;
			pCurrentFrame+=uRun;
			uInputLength-=uRun;

		} else {

			// Maximum length of a matched run
			uMaximumRun = 255;
			if (uInputLength<255) {
				uMaximumRun = uInputLength;		// 1-255
			}

			Word uMatchTest = pCurrentFrame[0];
			uRun = 1;
			while (uRun<uMaximumRun) {
				// Find end of repeater
				if (pCurrentFrame[uRun]!=uMatchTest) {
					break;
				}
				++uRun;
			}

			// Is there a run of 4 or greater?
			if (uRun>=4) {
				// Encode the run length
				pOutput->Append(static_cast<Word8>(0));
				pOutput->Append(static_cast<Word8>(uRun));
				pOutput->Append(static_cast<Word8>(uMatchTest));
				uInputLength -= uRun;
				pCurrentFrame += uRun;
				pPreviousFrame += uRun;

			} else {

				// Raw run
				uMaximumRun = 127;
				if (uInputLength<127) {
					uMaximumRun = uInputLength;
				}
				uRun = 0;
				while (++uRun<uMaximumRun) {	
					// Scan for next repeater
					if (pCurrentFrame[uRun]==uMatchTest && (pCurrentFrame[uRun+1]==uMatchTest) && (pCurrentFrame[uRun+2]==uMatchTest)) {
						// Remove from the run
						--uRun;
						break;
					}
					if ((pCurrentFrame[uRun]==pPreviousFrame[uRun]) &&
						(pCurrentFrame[uRun+1]==pPreviousFrame[uRun+1]) &&
						(pCurrentFrame[uRun+2]==pPreviousFrame[uRun+2])) {
						break;
					}
					// Get the next byte
					uMatchTest = pCurrentFrame[uRun];
				}

				// Handle some data optimizations

				// If it's only a single byte run and it's the same as the previous
				// frame? Just skip
				if ((uRun==1) && (pCurrentFrame[0]==pPreviousFrame[0])) {
					pOutput->Append(static_cast<Word8>(1));
					--uInputLength;
					++pCurrentFrame;
					++pPreviousFrame;

				// If it's only a single byte run and it's the same as the previous
				// frame? Just skip
				} else if ((uRun==2) && 
					(pCurrentFrame[0]==pPreviousFrame[0]) &&
					(pCurrentFrame[1]==pPreviousFrame[1])) {
					pOutput->Append(static_cast<Word8>(2));
					uInputLength-=2;
					pCurrentFrame+=2;
					pPreviousFrame+=2;

				} else {
					// Perform a raw data transfer
					// Run 1-128
					pOutput->Append(static_cast<Word8>(0x80|uRun));
					pOutput->Append(pCurrentFrame,uRun);
					uInputLength-=uRun;
					pCurrentFrame += uRun;
					pPreviousFrame+=uRun;
				}
			}
		}
	} while (uInputLength>=2);

	// Simple check if there's only one byte left

	if (uInputLength==1) {
		if (pCurrentFrame[0]==pPreviousFrame[0]) {
			pOutput->Append(static_cast<Word8>(1));
		} else {
			pOutput->Append(static_cast<Word8>(0x81));
			pOutput->Append(pCurrentFrame[0]);
		}
	}
}


/***************************************

	Process a video file into space ace format

***************************************/

static Word ExtractVideo(OutputMemoryStream *pOutput,const Word8 *pInput,WordPtr uInputLength)
{
	InputMemoryStream InputMem(pInput,uInputLength,TRUE);
	Image MyImage;
	FileGIF Giffy;
	Word8 IIgsPalette[32];
	Word8 NewIIgsPalette[32];
	Word uResult = 10;
	if (!Giffy.Load(&MyImage,&InputMem)) {

		if ((MyImage.GetWidth()!=320) || (MyImage.GetHeight()!=200)) {
			printf("Input file is not 320 x 200");
		} else {
			// Initialize the IIgs palette to invalid values
			MemoryFill(IIgsPalette,255,sizeof(IIgsPalette));
			Word8 *pCurrentFrame = static_cast<Word8 *>(Alloc(320*200/2));
			Word8 *pPreviousFrame = static_cast<Word8 *>(Alloc(320*200/2));
			int i = 1;
			do {

				// Process a frame

				// Save space for the chunk size
				WordPtr uOutputMark = pOutput->GetSize();
				pOutput->Append(static_cast<Word16>(0));

				// Convert the palette to IIgs format
				ConvertPalette(NewIIgsPalette,Giffy.GetPalette());

				// Set the default chunk type

				Word8 uTypeFlag = 0x01;

				// Is there a palette update?
				if (ComparePalette(NewIIgsPalette,IIgsPalette)) {
					MemoryCopy(IIgsPalette,NewIIgsPalette,sizeof(IIgsPalette));
					uTypeFlag |= 0x80U;
				}

				// Initial frame?
				if (i==1) {
					uTypeFlag |= 0x60;
				}

				// Send the data type byte
				pOutput->Append(static_cast<Word8>(uTypeFlag));

				if (uTypeFlag&0x80U) {
					pOutput->Append(IIgsPalette,32);
				}

				ConvertPixelsToIIgs(pCurrentFrame,&MyImage);

				if (uTypeFlag&0x40) {
					CompressKeyFrame(pOutput,pCurrentFrame);
				} else {
					CompressAnimFrame(pOutput,pPreviousFrame,pCurrentFrame);
				}
				MemoryCopy(pPreviousFrame,pCurrentFrame,320*200/2);

				// Update the chunk size
				Word16 uChuckShort;
				LittleEndian::Store(&uChuckShort,static_cast<Word16>(pOutput->GetSize()-uOutputMark));
				pOutput->Overwrite(&uChuckShort,2,uOutputMark);
				++i;
			} while (!Giffy.LoadNextFrame(&MyImage,&InputMem));
			Free(pCurrentFrame);
			Free(pPreviousFrame);
			// Append an "End of data" marker
			pOutput->Append(static_cast<Word16>(0xFF00U));
			uResult = 0;
		}
	} else {
		printf("Gif input file error!\n");
	}
	return uResult;
}

/***************************************

	Convert a Space Ace file to an animated GIF file

***************************************/

static char Name[] = "filexxx.gif";

static Word EncapsulateToGIF(OutputMemoryStream *pOutput,const Word8 *pInput,WordPtr uInputLength)
{
	// Too small?
	if (uInputLength<2) {
		return 10;
	}

	FileGIF GIF;
	Image MyImage;

	// Create an initial image
	MyImage.Init(320,200,Image::PIXELTYPE8BIT);
	MyImage.ClearBitmap();
	MemoryClear(GIF.GetPalette(),sizeof(GIF.GetPalette()[0])*256);

	//
	// Decompress a chunk
	//
	Word uFrame = 0;
	for (;;) {
		Word uChunkSize = LittleEndian::LoadAny(reinterpret_cast<const Word16 *>(pInput));
		if (uChunkSize>=0xFF00) {
//			printf("End of data, frames = %u\n",uFrame);
			break;
		}
		++uFrame;
		if (uChunkSize>uInputLength) {
			printf("Premature end of data\n");
			return 10;
		}
		if (uChunkSize<2) {
			printf("Chunk size too small\n");
			return 10;
		}
//		printf("Chunk is %u bytes\n",uChunkSize);
		const Word8 *pWork = pInput+2;
		uInputLength -= uChunkSize;
		pInput+= uChunkSize;
		uChunkSize-=2;

		// Get the palette token

		if (uChunkSize) {
			Word uType = pWork[0];
			++pWork;
			--uChunkSize;
//			printf("Token = 0x%02X\n",uType);

			if (uType&0x80) {

				// Clear out the palette
				MemoryClear(GIF.GetPalette(),sizeof(GIF.GetPalette()[0])*256);
				ConvertPalette(GIF.GetPalette(),pWork);
				pWork+=32;
				uChunkSize-=32;
			}

			// Full image or animation frame?
			if (uType&0x40) {
				Word uTemp;
				Word8 *pDest = MyImage.GetImage();

				for (;;) {
					uTemp = pWork[0];
					++pWork;
					if (!uTemp) {
						break;
					}
					if (uTemp&0x80) {
						uTemp&=0x7f;
						if (uTemp) {
							// Run length compressed loop
							Word uSecond = pWork[0];
							++pWork;
							Word uFirst = uSecond>>4U;
							uSecond&=0xF;
							do {
								pDest[0] = static_cast<Word8>(uFirst);
								pDest[1] = static_cast<Word8>(uSecond);
								pDest+=2;
							} while (--uTemp);
						}
					} else {
						// Uncompressed loop
						do {
							Word uColor = pWork[0];
							++pWork;
							pDest[0] = static_cast<Word8>(uColor>>4U);
							pDest[1] = static_cast<Word8>(uColor&0xF);
							pDest+=2;
						} while (--uTemp);
					}
				}
			} else {

				Word uTemp;
				Word8 *pDest = MyImage.GetImage();
				Word8 *pEnd = pDest+(320*200);
				do {
					uTemp = pWork[0];
					++pWork;
					if (!uTemp) {
						uTemp = pWork[0];
						++pWork;
						if (uTemp) {
							// Run length compressed loop
							Word uSecond = pWork[0];
							++pWork;
							Word uFirst = uSecond>>4U;
							uSecond&=0xF;
							do {
								pDest[0] = static_cast<Word8>(uFirst);
								pDest[1] = static_cast<Word8>(uSecond);
								pDest+=2;
							} while (--uTemp);
						}
					} else if (uTemp&0x80) {
						uTemp&=0x7f;
						if (uTemp) {
							// Uncompressed loop
							do {
								Word uColor = pWork[0];
								++pWork;
								pDest[0] = static_cast<Word8>(uColor>>4U);
								pDest[1] = static_cast<Word8>(uColor&0xF);
								pDest+=2;
							} while (--uTemp);
						}
					} else {
						pDest+=(uTemp*2);
					}
				} while (pDest<pEnd);
			}
		}
		if (uFrame==1) {
			GIF.AnimationSaveStart(pOutput,&MyImage);
		}
		GIF.AnimationSaveFrame(pOutput,&MyImage,(100U/8U));
	}
	GIF.AnimationSaveFinish(pOutput);
	return 0;
}

/***************************************

	Main dispatcher

***************************************/

int BURGER_ANSIAPI main(int argc,const char **argv)
{
	ConsoleApp MyApp(argc,argv);
	CommandParameterBooleanTrue DoVideo("Process Video","v");
	CommandParameterBooleanTrue ConvertToGIF("Convert to GIF","g");
	const CommandParameter *MyParms[] = {
		&DoVideo,
		&ConvertToGIF
	};
	argc = MyApp.GetArgc();
	argv = MyApp.GetArgv();
	argc = CommandParameter::Process(argc,argv,MyParms,sizeof(MyParms)/sizeof(MyParms[0]),
		"Usage: packvideo InputFile OutputFile\n\n"
		"Preprocess video data for Space Ace IIgs.\nCopyright by Rebecca Ann Heineman\n",3);
	if (argc<0) {
		Globals::SetErrorCode(10);
	} else {
		MyApp.SetArgc(argc);

		Filename InputName;
		InputName.SetFromNative(argv[1]);

		WordPtr uInputLength;
		Word8 *pInput = static_cast<Word8 *>(FileManager::LoadFile(&InputName,&uInputLength));
		if (!pInput) {
			printf("Can't open %s!\n",argv[1]);
			Globals::SetErrorCode(10);
		} else {

			// Convert gif to data
			if (DoVideo.GetValue()) {
				OutputMemoryStream Output;
				if (ExtractVideo(&Output,pInput,uInputLength)) {
					printf("Can't convert %s!\n",argv[1]);
					Globals::SetErrorCode(10);
				} else {
					Filename OutputName;
					OutputName.SetFromNative(argv[2]);
					if (Output.SaveFile(&OutputName)) {
						printf("Can't save %s!\n",argv[2]);
						Globals::SetErrorCode(10);
					}
				}
				
			// Convert raw video to GIF
			} else if (ConvertToGIF.GetValue()) {
				OutputMemoryStream Output;
				if (EncapsulateToGIF(&Output,pInput,uInputLength)) {
					printf("Can't convert %s!\n",argv[1]);
					Globals::SetErrorCode(10);
				} else {
					Filename OutputName;
					OutputName.SetFromNative(argv[2]);
					if (Output.SaveFile(&OutputName)) {
						printf("Can't save %s!\n",argv[2]);
						Globals::SetErrorCode(10);
					}
				}
			} else {
				printf("No conversion selected for %s!\n",argv[1]);
				Globals::SetErrorCode(10);
			}
			Free(pInput);
		}
	}
	return Globals::GetErrorCode();
}

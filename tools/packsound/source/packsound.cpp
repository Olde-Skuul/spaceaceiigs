/***************************************

	Tool to process data for Space Ace IIgs

	Copyright (c) 1995-2015 by Rebecca Ann Heineman <becky@burgerbecky.com>

	It is released under an MIT Open Source license. Please see LICENSE
	for license details. Yes, you can use it in a
	commercial title without paying anything, just give me a credit.
	Please? It's not like I'm asking you for money!

***************************************/

#include "packsound.h"

#define DOC_28MHZ 28636360.0f			// Master Ensoniq clock rate
#define DOC_RATE (DOC_28MHZ/32.0f)		// Ensoniq clock rate
#define SCAN_RATE (DOC_RATE/34.0f)		// All oscillators are enabled

struct SpaceAceAudioFile_t {
	Word16 m_uDOCRate;			// Value to put into the Ensoniq DOC for sample rate
	Word16 m_uBytesPerTick;		// Number of bytes consumed per 1/60th of a second tick
	Word8 m_Data[1];			// Raw audio data
};

static const Word8 g_Lookup[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};

/***************************************

	Process a sound file into 4 bits per sample

***************************************/

static Word ExtractSound(OutputMemoryStream *pOutput,const Word8 *pInput,WordPtr uInputLength)
{
	if (uInputLength<4) {
		printf("Input is too small\n");
		return 1;
	}
	if (MemoryCompare("RIFF",pInput,4)) {
		printf("Not a sound file\n");
		return 1;
	}
	Word32 uFileLength = LittleEndian::Load(reinterpret_cast<const Word32 *>(pInput+4));
	if (uFileLength!=uInputLength) {
		printf("Sound file length mismatch\n");
		return 1;
	}

	// Calculate the DOC rate
	int iSampleRate = LittleEndian::Load(reinterpret_cast<const Word32 *>(pInput+24));
	// Convert from a IIgs step rate to a samples per second rate
	int iDOCRate = static_cast<int>(((static_cast<float>(iSampleRate)*512.0f)/SCAN_RATE)+0.5f);

	// Output the Space Ace audio header
	pOutput->Append(static_cast<Word16>(iDOCRate));
	pOutput->Append(static_cast<Word16>((iSampleRate+59)/60));

	// WGet the number of samples
	Word uSoundLength = LittleEndian::Load(reinterpret_cast<const Word32 *>(pInput+40));
	uSoundLength>>=1U;
	if (uSoundLength) {
		const Word8 *pTemp = pInput+44;
		do {
			// Convert to 4 bits per sample audio
			pOutput->Append(static_cast<Word8>((pTemp[0]&0xF0U)+(pTemp[1]>>4U)));
			pTemp+=2;
		} while (--uSoundLength);
	}
	return 0;
}

/***************************************

	Convert a Space Ace file to a WAV file

***************************************/

static Word EncapsulateToWAV(OutputMemoryStream *pOutput,const Word8 *pInput,WordPtr uInputLength)
{
	// Too small?
	if (uInputLength<4) {
		return 10;
	}
	const Word8 *pWork = pInput+4;
	WordPtr uCounter = uInputLength-4;

	int iSampleRate = LittleEndian::Load(&reinterpret_cast<const SpaceAceAudioFile_t *>(pInput)->m_uDOCRate);
	// Convert from a IIgs step rate to a samples per second rate
	iSampleRate = static_cast<int>((static_cast<float>(iSampleRate)/512.0f)*SCAN_RATE);

	// Trim excess data if needed

	if (uCounter) {
		do {
			if (pWork[uCounter-1]==0x88) {
				break;
			}
		} while (--uCounter);
	}

	pOutput->Append("RIFF");
	pOutput->Append(static_cast<Word32>((uCounter*2)+44));
	pOutput->Append("WAVEfmt ");
	pOutput->Append(static_cast<Word32>(16));				// Chunk size (Minimum)
	pOutput->Append(static_cast<Word16>(1));				// Format code (PCM)
	pOutput->Append(static_cast<Word16>(1));				// Mono
	pOutput->Append(static_cast<Word32>(iSampleRate));		// Samples per second
	pOutput->Append(static_cast<Word32>(iSampleRate));		// Bytes per second (Same as samples)
	pOutput->Append(static_cast<Word16>(1));				// Byte aligned
	pOutput->Append(static_cast<Word16>(8));				// Bits per sample
	pOutput->Append("data");
	pOutput->Append(static_cast<Word32>(uCounter*2));

	// No diff
	do {
		pOutput->Append(g_Lookup[(pWork[0]>>4)&0xF]);
		pOutput->Append(g_Lookup[pWork[0]&0xF]);
		++pWork;
	} while (--uCounter);

	return 0;
}

/***************************************

	Main dispatcher

***************************************/

int BURGER_ANSIAPI main(int argc,const char **argv)
{
	ConsoleApp MyApp(argc,argv);
	CommandParameterBooleanTrue DoSound("Process Sound","s");
	CommandParameterBooleanTrue DoWave("Convert to Wave","w");
	const CommandParameter *MyParms[] = {
		&DoSound,
		&DoWave
	};

	argc = MyApp.GetArgc();
	argv = MyApp.GetArgv();
	argc = CommandParameter::Process(argc,argv,MyParms,sizeof(MyParms)/sizeof(MyParms[0]),
		"Usage: packsound InputFile OutputFile\n\n"
		"Preprocess data for Space Ace IIgs.\nCopyright by Rebecca Ann Heineman\n",3);
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

			// Convert wave to data
			if (DoSound.GetValue()) {
				OutputMemoryStream Output;
				if (ExtractSound(&Output,pInput,uInputLength)) {
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

			// Convert raw audio to WAV
			} else if (DoWave.GetValue()) {
				OutputMemoryStream Output;
				if (EncapsulateToWAV(&Output,pInput,uInputLength)) {
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

#include "Base_Sound.h"
#include <math.h>

/*
	Music Beats = quarter note if 4/4. That means we can transform it
	to full whole notes (1/1).
*/
#define WHOLE_NOTES_PER_MINUTE (MUSIC_BPM / 4)

#define GetMusicFrames(LSampleRate) ((size_t) \
(ceilf(((((float)WHOLE_NOTES_COUNT) / \
((float)WHOLE_NOTES_PER_MINUTE)) * 60.f) \
* ((float)(LSampleRate))))) 

#define ALIGN_SIZE(Size, AlSize)        ((Size + (AlSize-1)) & (~(AlSize-1)))
#define ALIGN_SIZE_64K(Size)            ALIGN_SIZE(Size, 65536)

#define maxmin(a, minimum, maximum)  min(max(a, minimum), maximum)

/*
	Windows 10 only
*/
const TIME_INTERVAL Ring09_Intervals[] =
{
	{ 65671, 76777 },		// Piano, F4
	{ 110677, 120487 }		// Piano, E3
};

const TIME_INTERVAL Ring02_Intervals[] =
{
	{ 19028, 24499 },		// Arp, C5
	{ 25342, 30774 },		// Arp, A4
	{ 25342, 36299 },		// Arp, A4-F4-E4
	{ 36030, 47322 },		// Arp, A3-F4-E4
	{ 25342, 47322 }		// Arp, A4-F4-E4 + delay 
};

typedef struct  
{
	int FileId;
	WCHAR szName[32];
} SOUNDID_PATH;

const SOUNDID_PATH SoundsPaths[] = 
{
	{ 0, L"NULL" },
	{ 1, L"Ring09" },
	{ 2, L"Ring02" }
};

WAVEFORMATEX waveFormat;
HANDLE hFileToPlay = NULL;
float* BaseBuffer = NULL;
float fMasterVolume = 1.f;
DWORD dwHeaderSize = 0;
size_t BufferPosition = 0;
size_t ProcessedFrames = 0;
size_t FramesCount = 0;

boolean
ProcessSoundWorker(
	SOUNDDEVICE_INFO* pInfo
)
{
#if 1
	DWORD dwTemp = 0;
	LARGE_INTEGER larg;
	memset(&larg, 0, sizeof(LARGE_INTEGER));

	hFileToPlay = CreateFileW(L"I:\\Test_Alistra.raw", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (!hFileToPlay || hFileToPlay == INVALID_HANDLE_VALUE) return false;

	GetFileSizeEx(hFileToPlay, &larg);
	if (larg.QuadPart < 8)
	{
		CloseHandle(hFileToPlay);
		hFileToPlay = NULL;
		return false;
	}

	BaseBuffer = HeapAlloc(GetProcessHeap(), 0, (size_t)larg.QuadPart);
	ReadFile(hFileToPlay, BaseBuffer, (DWORD)larg.QuadPart, &dwTemp, 0);

	FramesCount = (size_t)(larg.QuadPart / sizeof(float));

	CloseHandle(hFileToPlay);	
	hFileToPlay = NULL;

#else
	__try
	{
		FramesCount = GetMusicFrames(pInfo->Fmt.SampleRate) * pInfo->Fmt.Channels;
		BaseBuffer = VirtualAlloc(NULL, ALIGN_SIZE_64K(FramesCount * sizeof(float)), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!BaseBuffer) return false;

		//while (ProcessedFrames < FramesCount)
		{
			/*
				TODO: Process function
			*/
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		/*
			We can't process our data, so we quit from thread
		*/
		return false;
	}
#endif	

	waveFormat.wFormatTag = (pInfo->Fmt.IsFloat ? 3 : 1);
	waveFormat.wBitsPerSample = pInfo->Fmt.Bits;

	return true;
}

float
GetSoundWorkerProcess()
{
	float ret = (((float)ProcessedFrames) / ((float)(FramesCount)));
	return ret > 0.95f ? 1.0f : ret;
}


void 
SoundWorker(
	float* FileData,
	size_t DataSize, 
	int Channels
)
{
	/*
		M$ Frame Size = Single Frame Size * Channels Count

		That means...

		4410 Frames Buffer == 8820 Frames Buffer by normal system of counting frames
	*/
	size_t sizeToRead = DataSize * Channels;

	if (waveFormat.wFormatTag == 3)
	{
		if (BufferPosition + sizeToRead < FramesCount)
		{
			for (size_t i = 0; i < sizeToRead; i++)
			{
				BaseBuffer[BufferPosition + i] *= fMasterVolume;
			}

			memcpy(FileData, &BaseBuffer[BufferPosition], sizeToRead * sizeof(float));
		}
		else
		{
			memset(FileData, 0, sizeToRead * sizeof(float));
		}
	}
	else
	{
		short* pShortData = (short*)FileData;

		switch (waveFormat.wBitsPerSample)
		{
		case 16:
			for (size_t i = 0; i < sizeToRead; i++)
			{
				pShortData[i] = maxmin(((short)(BaseBuffer[BufferPosition + i] * 32768.0f)), -32768, 32767);
			}
			break;
		default:
			break;
		}
	}

	BufferPosition += sizeToRead;
}

boolean
IsMusicEnd()
{
	return (BufferPosition >= FramesCount);
} 

void
DestroySoundWorker()
{
	if (BaseBuffer && FramesCount) VirtualFree(BaseBuffer, FramesCount * sizeof(float), MEM_RELEASE);
}

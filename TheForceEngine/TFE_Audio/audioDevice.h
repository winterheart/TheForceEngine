#pragma once
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#define MUTEX_INITIALIZE(A) InitializeCriticalSection(A)
#define MUTEX_DESTROY(A)    DeleteCriticalSection(A)
#define MUTEX_LOCK(A)       EnterCriticalSection(A)
#define MUTEX_TRYLOCK(A)    TryEnterCriticalSection(A)
#define MUTEX_UNLOCK(A)     LeaveCriticalSection(A)

typedef CRITICAL_SECTION Mutex;
#else

#include <pthread.h>

#define MUTEX_INITIALIZE(A) pthread_mutex_init(A, NULL)
#define MUTEX_DESTROY(A)    pthread_mutex_destroy(A)
#define MUTEX_LOCK(A)       pthread_mutex_lock(A)
#define MUTEX_TRYLOCK(A)    pthread_mutex_trylock(A)
#define MUTEX_UNLOCK(A)     pthread_mutex_unlock(A)

typedef pthread_mutex_t Mutex;
#endif

#ifdef _WIN32
#undef min
#undef max
#endif

static const u32 AUDIO_STATUS_INPUT_OVERFLOW   = 0x1;  // Input data was discarded because of an overflow condition at the driver.
static const u32 AUDIO_STATUS_OUTPUT_UNDERFLOW = 0x2;  // The output buffer ran low, likely causing a gap in the output sound.

typedef s32(*StreamCallback)(void* outputBuffer, void* inputBuffer, u32 nFrames, f64 streamTime, u32 status, void* userData);

namespace TFE_AudioDevice
{
	bool init(u32 audioFrameSize = 256u);
	void destroy();

	bool startOutput(StreamCallback callback, void* userData = 0, u32 channels = 2, u32 sampleRate = 44100);
	void stopOutput();
};

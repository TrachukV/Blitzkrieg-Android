#pragma once
// WAV, decoded from memory.
//
// The engine hands FSOUND_Sample_Load a block it has already read out of the
// game archives, and those blocks are WAV. This is what turns them into the
// interleaved 16-bit frames the mixer plays.
//
// It lives apart from the audio device for one reason: it is the only part of
// the sound path with logic that can be wrong in a way a device would not
// report -- a misread chunk is silence or noise, and a trusted length field is
// a read past the end of the buffer. Being its own unit means the host test in
// android/tests/wave_parser_test.cpp exercises exactly the code that ships,
// rather than a copy of it that drifts.
//
// Nothing here touches Android, so it compiles on the host unchanged.
#include <stddef.h>
#include <stdint.h>

#include <vector>

struct SWaveData
{
    std::vector<int16_t> samples;    // interleaved, one entry per channel per frame
    int nChannels;
    int nRate;

    SWaveData() : nChannels( 1 ), nRate( 44100 ) {}

    int Frames() const
    {
        return nChannels > 0 ? (int)( samples.size() / nChannels ) : 0;
    }
};

// Reads a RIFF/WAVE block. Returns false for anything it cannot represent --
// a non-PCM encoding, more than two channels, a depth other than 8, 16 or 24 --
// and never reads beyond nLength whatever the header claims.
bool ParseWave( const unsigned char *pData, size_t nLength, SWaveData *pOut );

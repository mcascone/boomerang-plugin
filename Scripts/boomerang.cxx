/** \file
 *   boomerang.
 *   Record and loop/overdub
 */

string name="Boomerang+";

enum InputParamsIndexes
{
    kRecordParam=0,
    kPlayParam,
    kOnceParam,
    kDirectionParam,
    kStackParam,
    kThruMuteParam,
    kSpeedParam,
    kMixParam
};

array<string> inputParametersNames={"Record","Play","Once","Direction","Stack","Thru Mute","Speed","Mix"};
array<double> inputParametersDefault={0,0,0,0,0,1,1,.5};
array<double> inputParametersMax={1,1,1,1,1,1,1,1};
array<int> inputParametersSteps={2,2,2,2,2,2,2,101};
array<string> inputParametersEnums={"Off;On","Off;On","Off;On","Off;On","Off;On","Off;On","Full;Half",""};
array<double> inputParameters(inputParametersNames.length);

enum LEDState
{
    kIdle=0,
    kRecording,
    kPlaying,
    kPlayingOnce,
    kReversed,
    kOutOfMemory
};

array<string> outputParametersNames={"Play","Rec","PlayHead","RecordHead","Loop Len"};
array<double> outputParameters(outputParametersNames.length);
array<double> outputParametersMin={0,0};
array<double> outputParametersMax={1,1};
array<string> outputParametersEnums={"Stopped;Playing","Stopped;Recording"};

/* Internal Variables.
 *
 */
array<array<double>> buffers(audioInputsCount);
int allocatedLength=int(sampleRate*60); // 60 seconds max recording
bool recording=false;
bool playing=false;
bool reversing=false;
bool stacking=false;
bool onceMode=false;
bool outOfMemory=false;
int currentPlayingIndex=0;
int currentRecordingIndex=0;
int loopDuration=0;
bool thruMute=false;
bool speedHalf=false;
double mix=0.5;

const int fadeTime=int(.001*sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);
const double triggerThreshold=.005;

LEDState currentLEDState = kIdle;

void initialize()
{
    for(uint channel=0;channel<audioInputsCount;channel++)
    {
        buffers[channel].resize(allocatedLength);
    }
    loopDuration=0;
    resetLEDs();
}

int getTailSize()
{
    return -1;
}

void resetLEDs()
{
    currentLEDState = kIdle;
}

void setLEDState(LEDState state)
{
    currentLEDState = state;
}

void startRecording()
{
    if(recording)
        return;

    if(outOfMemory)
    {
        initialize();
        outOfMemory = false;
    }

    recording = true;
    playing = false;
    reversing = false;
    stacking = false;
    currentRecordingIndex = 0;

    setLEDState(kRecording);
}

void stopRecording()
{
    if(!recording)
        return;

    recording = false;
    playing = true;
    loopDuration = currentRecordingIndex;
    currentPlayingIndex = 0;

    setLEDState(kPlaying);
}

void startPlayback()
{
    if(outOfMemory)
        return;

    playing = true;
    currentPlayingIndex = 0;
    setLEDState(kPlaying);
}

void stopPlayback()
{
    playing = false;
    setLEDState(kIdle);
}

void startOncePlayback()
{
    if(outOfMemory)
        return;

    onceMode = true;
    playing = true;
    currentPlayingIndex = 0;
    setLEDState(kPlayingOnce);
}

void startReversePlayback()
{
    if(!playing)
        return;

    reversing = !reversing;
    setLEDState(reversing ? kReversed : kPlaying);
}

void handleOutOfMemory()
{
    recording = false;
    playing = false;
    outOfMemory = true;
    setLEDState(kOutOfMemory);
}

void processBlock(BlockData& data)
{
    if(recording)
    {
        for(uint i=0; i<data.samplesToProcess; i++)
        {
            for(uint channel=0; channel<audioInputsCount; channel++)
            {
                buffers[channel][currentRecordingIndex] = data.samples[channel][i];
            }
            currentRecordingIndex++;
            if(currentRecordingIndex >= allocatedLength)
            {
                handleOutOfMemory();
                break;
            }
        }
    }

    if(playing)
    {
        for(uint i=0; i<data.samplesToProcess; i++)
        {
            for(uint channel=0; channel<audioOutputsCount; channel++)
            {
                if(reversing)
                    data.samples[channel][i] = buffers[channel][loopDuration - currentPlayingIndex - 1];
                else
                    data.samples[channel][i] = buffers[channel][currentPlayingIndex];
            }
            currentPlayingIndex++;
            if(currentPlayingIndex >= loopDuration)
            {
                if(onceMode)
                {
                    stopPlayback();
                    onceMode = false;
                }
                else
                {
                    currentPlayingIndex = 0;
                }
            }
        }
    }

    outputParameters[0] = playing ? 1 : 0;
    outputParameters[1] = recording ? 1 : 0;
    outputParameters[2] = double(currentPlayingIndex) / sampleRate;
    outputParameters[3] = double(currentRecordingIndex) / sampleRate;
    outputParameters[4] = double(loopDuration) / sampleRate;
}

void updateInputParametersForBlock(const TransportInfo@ info)
{
    if(inputParameters[kRecordParam] == 1)
    {
        if(recording)
        {
            stopRecording();
        }
        else
        {
            startRecording();
        }
    }

    if(inputParameters[kPlayParam] == 1)
    {
        if(playing)
        {
            stopPlayback();
        }
        else
        {
            startPlayback();
        }
    }

    if(inputParameters[kOnceParam] == 1)
    {
        startOncePlayback();
    }

    if(inputParameters[kDirectionParam] == 1)
    {
        startReversePlayback();
    }

    if(inputParameters[kStackParam] == 1)
    {
        stacking = true;
    }
    else
    {
        stacking = false;
    }

    thruMute = inputParameters[kThruMuteParam] == 1;
    speedHalf = inputParameters[kSpeedParam] == 1;
    mix = inputParameters[kMixParam];
}

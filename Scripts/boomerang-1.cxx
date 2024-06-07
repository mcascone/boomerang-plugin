/** \file
 *   looper.
 *   Record and loop/overdub
 */

// DONE: remove rec trigger
// DONE: set default rec mode to boomerang mode
// DONE: remove snap
// TODO: figure out how to put the leds on top of the buttons (maybe need the custom gui)
// TODO: link leds to switches
// TODO: add stack mode
// TODO: add ONCE mode
// TODO: Flash record LED when loop cycles around

/*  Effect Description
 *
 */
string name="Boomerang Phrase Sampler";
string description="An attempt to emulate the Boomerang+ Looper pedal";

/* Parameters Description.
 */
enum InputParamsIndexes
{
    kOutputLevelParam=0,
    kThruMuteParam,
    kRecordParam,
    kPlayParam,
    kResetParam,
    kReverse,
    kStack
};

enum RecordMode
{
    kRecLoopOver=0, ///< standard looper: records over existing material and keep original loop length
    kRecExtend,     ///< records over existing material and extends original loop length when reaching end of loop
    kRecAppend,     ///< append to existing loop (without recording original loop content after loop duration has been reached) <-- stack mode?
    kRecOverWrite,  ///< overwrite loop content with new material, and extend original loop until recording ends
    kRecPunch,      ///< overwrite loop content with new material, but keeps original loop length
    kRecClear       ///< clear original loop when recording starts  <-- pretty sure this is the boomerang mode when press record (5)
};


array<string> inputParametersNames={"Output Level", "Thru Mute", "Record", "Play (Stop)", "Once", "Direction", "Stack (Speed)"};
array<double> inputParameters(inputParametersNames.length);
array<double> inputParametersDefault={
    5, // OutputLevel
    0, // Thru Mute
    0, // Record
    0, // Play/Stop
    0, // Once
    0, // Reverse
    0, // Stack
};
array<double> inputParametersMax={
    10,  // Output Level, if not entered, defaults to percentage
    1,   // Thru Mute
    1,   // Record
    1,   // Play/Stop
    1,   // Once
    1,   // Reverse
    1,   // Stack
};

// these are the number of available steps/modes for each parameter - 1-based
array<int> inputParametersSteps={
    -1, // OutputLevel // -1 means continuous
    2,  // Thru Mute
    2,  // Record
    2,  // Play
    2,  // Once
    2,  // Reverse
    2,  // Stack
};

// these are the labels under each input control
array<string> inputParametersEnums={
    "",            // Output Level
    ";THRU MUTE",  // Thru Mute
    ";Recording",  // Record
    ";Playing",    // Play
    ";Once",       // Once
    "Fwd;Rev",     // Reverse
    ";STACK"       // Stack
};

enum OutputParamsIndexes
{
    kThruMuteLed=0,
    kRecordLed,  // 1
    kPlayLed,    // 2
    kOnceLed,    // 3
    kReverseLed, // 4
    kStackLed,   // 5
    kSpeedLed    // 6
};

array<string> outputParametersNames={"Thru Mute","Record","Play","Once","Reverse","Stack", "1/2 Speed"};
array<double> outputParameters(outputParametersNames.length);
array<double> outputParametersMin={0,0,0,0,0,0,0};
array<double> outputParametersMax={1,1,1,1,1,1,1};
array<string> outputParametersEnums={";",";",";",";",";",";",";"};



/* Internal Variables.
 *
 */
array<array<double>> buffers(audioInputsCount);

int allocatedLength=int(sampleRate*60); // 60 seconds max recording
bool recording=false;
bool recordingArmed=false;

double OutputLevel=0;
double playbackGain=0;
double recordGain=0;
double playbackGainInc=0;
double recordGainInc=0;

int currentPlayingIndex=0;
int currentRecordingIndex=0;

int loopDuration=0;
bool eraseValueMem=false;

const int fadeTime=int(.001*sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);

RecordMode recordingMode=kRecClear; // Hardcode to boomerang mode

// bool triggered=false;
bool Reverse=false;
bool ReverseArmed=false;
bool playArmed=false;
bool playing=false;

bool bufferFilled=false;

/* Initialization
 *
 */
void initialize()
{
    // create buffer
    for(uint channel=0;channel<audioInputsCount;channel++)
    {
        buffers[channel].resize(allocatedLength);
    };
    loopDuration=0;
}

int getTailSize()
{
    // infinite tail (sample player)
    return -1;
}

//sync utils
// double quarterNotesToSamples(double position,double bpm)
// {
//     return position*60.0*sampleRate/bpm;
// }

// double samplesToQuarterNotes(double samples,double bpm)
// {
//     return samples*bpm/(60.0*sampleRate);
// }

void startRecording()
{
    // clear recording if required
    // should be hardcoded to boomerang mode
    if(recordingMode==kRecClear)
    {
        loopDuration=0;
        currentPlayingIndex=0;
    }
    // else if(recordingMode==kRecOverWrite)
    // {
    //     // truncate loop duration to current position (+ crossfade time)
    //     int newLoopDuration=currentPlayingIndex+int(fadeTime+1);
    //     if(newLoopDuration<loopDuration)
    //     {
    //         loopDuration=newLoopDuration;
    //     }
    // }
    // else if(recordingMode==kRecPunch)
    // {
    //     // decrease playback gain to start fade out
    //     playbackGainInc=-xfadeInc;
    // }
    // TODO: implement append mode
    
    // actually start recording
    recording=true;
    currentRecordingIndex=currentPlayingIndex;
    // pre fade to avoid clicks
    recordGain=0;
    recordGainInc=xfadeInc;
}

void stopRecording()
{
    recording=false;
    const int recordedCount=currentRecordingIndex;
    // post fade to avoid clicks
    recordGainInc=-xfadeInc;
    
    // recorded beyond previous loop -> update duration, start playback at 0
    if(recordedCount>loopDuration)
    {
        loopDuration=recordedCount;
        currentPlayingIndex=0;
    }
    
    if(recordingMode==kRecPunch)
    {
        // increase playback gain to start fade in
        playbackGainInc=xfadeInc;
    }
}

void startPlayback()
{
    // start playback
    playing=true;
    currentPlayingIndex=0;
    playbackGain=0;
    playbackGainInc=xfadeInc;
}

void stopPlayback()
{
    playing=false;
    playbackGainInc=-xfadeInc;
}

bool isPlaying()
{
    // while recording and overwriting content without blending, do not play
    return (playing || (playbackGainInc!=0)) && !(recording && ((recordingMode==kRecOverWrite || recordingMode==kRecAppend) && currentRecordingIndex>loopDuration));
}

void startReverse()
{
    if(Reverse==false && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=true;
}

void stopReverse()
{
    if(Reverse==true && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=false;
}

void processBlock(BlockData& data)
{
    // // indexes used by auto record trigger or snapped play/stop
    int startRecordingSample=-1; // sample number in buffer when recording should be started
    int stopRecordingSample=-1; // sample number in buffer when recording should be stopped
    int startPlayingSample=-1; // sample number in buffer when playback should be started	
    int stopPlayingSample=-1; // sample number in buffer when playback should be stopped
    int startReverseSample=-1; // sample number in buffer when Reverse should be started
    int stopReverseSample=-1; // sample number in buffer when Reverse should be stopped

    // 
    if(!recording && loopDuration==0 && recordingArmed==true)
    {
        for(uint channel=0;channel<audioInputsCount && startRecordingSample==-1;channel++)
        {
            array<double>@ samplesBuffer=@data.samples[channel];
            for(uint i=0;i<data.samplesToProcess && startRecordingSample==-1;i++)
            {
                startRecordingSample=i;
            }
        }
        // not found? next buffer
        if(startRecordingSample==-1)
            startRecordingSample=data.samplesToProcess;
    }

    // if in snap mode and the host is actually playing: check our position
    // if(snap!=kSnapNone && @data.transport!=null && data.transport.isPlaying)
    // {
    //     // start position is either buffer start, or startRecordingSample if we are detecting automatically
    //     double currentPos=data.transport.positionInQuarterNotes;
    //     if(startRecordingSample>=0)
    //         currentPos+=samplesToQuarterNotes(startRecordingSample,data.transport.bpm);
        
    //     double expectedPos=0;
        
    //     // snap to next quarter note
    //     if(snap==kSnapQuarter)
    //     {
    //         expectedPos=floor(currentPos);
    //         if(expectedPos!=currentPos)
    //             expectedPos++;
    //     }
    //     // expected position is next down beat (except if buffer is exactly on beat
    //     else if(snap==kSnapMeasure)
    //     {
    //         expectedPos=data.transport.currentMeasureDownBeat;
    //         if(expectedPos!=0)
    //             expectedPos+=(data.transport.timeSigTop)/(data.transport.timeSigBottom)*4.0;
    //         while(expectedPos<currentPos)
    //             expectedPos+=(data.transport.timeSigTop)/(data.transport.timeSigBottom)*4.0;
    //     }
    //     double expectedSamplePosition=quarterNotesToSamples(expectedPos,data.transport.bpm);
    //     int nextSample=int(floor(expectedSamplePosition+.5))-data.transport.positionInSamples;
        
    //     // manage recording
    //     if(!recording && (loopDuration==0 || recordingMode==kRecClear) && recordingArmed==true)
    //     {
    //         startRecordingSample=nextSample;
    //     }
    //     else if (recording && recordingArmed==false)
    //     {
    //         stopRecordingSample=nextSample;
    //     }
        
    //     // manage playing
    //     if(!playing && playArmed==true)
    //     {
    //         startPlayingSample=nextSample;
    //     }
    //     else if (playing && playArmed==false)
    //     {
    //         stopPlayingSample=nextSample;
    //     }
        
    //     // manage reversing
    //     if(!Reverse && ReverseArmed==true)
    //     {
    //         startReverseSample=nextSample;
    //     }
    //     else if (playing && ReverseArmed==false)
    //     {
    //         stopReverseSample=nextSample;
    //     }
    // }
    
    // smooth OutputLevel update: use begin and end values
    // since the actual gain is exponential, we can use the ratio between begin and end values
    // as an incremental multiplier for the actual gain
    double OutputLevelInc=(data.endParamValues[kOutputLevelParam]-data.beginParamValues[kOutputLevelParam])/data.samplesToProcess;
    
    // actual audio processing happens here
    for(uint i=0;i<data.samplesToProcess;i++)
    {
        // manage recording state
        bool doStartRecording=(int(i)==startRecordingSample);
        bool doStopRecording=(int(i)==stopRecordingSample);
        
        if(doStartRecording) {
            startRecording();
        }
        else if(doStopRecording) {
            stopRecording();
        }
        
        // manage playback state
        bool startPlaying=(int(i)==startPlayingSample);
        bool stopPlaying=(int(i)==stopPlayingSample);
        
        if(startPlaying)
        {
            startPlayback();
        }
        else if(stopPlaying)
        {
            stopPlayback();
        }

        // manage Reverse state
        bool startReversal=(int(i)==startReverseSample);
        bool stopReversal=(int(i)==stopReverseSample);
        
        if(startReversal) {
            startReverse();
        }
        else if(stopReversal) {
            stopReverse();
        }
        
        const bool currentlyPlaying=isPlaying();
        
        // process audio for each channel--------------------------------------------------
        for(uint channel=0;channel<audioInputsCount;channel++)
        {
            array<double>@ channelBuffer=@buffers[channel];
            array<double>@ samplesBuffer=@data.samples[channel];
            double input=samplesBuffer[i];
    
            double playback=0;
            if(currentlyPlaying)
            {
                // read loop content
                int index=currentPlayingIndex;
                if(Reverse && loopDuration>0)
                    index=loopDuration-1-index;
                playback=channelBuffer[index]*playbackGain;
            }
            
            // update buffer when recording
            if(recording || (recordGainInc!=0))
            {
                channelBuffer[currentRecordingIndex]=playback+recordGain*input;
            }
            
            // copy to output with OutputLevel
            samplesBuffer[i]=input+OutputLevel*playback;
        }
        // end process audio for each channel--------------------------------------------------
        
        
        // update playback index while playing
        if(currentlyPlaying)
        {
            // update index
            currentPlayingIndex++;
            if(currentPlayingIndex>=loopDuration)
                currentPlayingIndex=0;
            
            // playback xfade
            if(loopDuration>0)
            {
                if(!(recording && recordingMode==kRecPunch)) // when recording in punch mode, playback gain is controlled by record status
                {
                    if(currentPlayingIndex==(loopDuration-fadeTime))
                        playbackGainInc=-xfadeInc;
                    else if(currentPlayingIndex<fadeTime)
                        playbackGainInc=xfadeInc;
                }
            }
            else
                playbackGain=0;
        }
        
        // update recording index, if recording
        if(recording || (recordGainInc!=0))
        {
            currentRecordingIndex++;
            // looping over existing => check boundaries
            if((recordingMode==kRecLoopOver||recordingMode==kRecPunch) && loopDuration>0 && currentRecordingIndex>=loopDuration)
            {
                currentRecordingIndex=0;
            }
            if(currentRecordingIndex>=allocatedLength) // stop recording if reached the end of the buffer
            {
                stopRecording();
                recordGainInc=0; // avoid post buffer recording
                // TODO: turn on all LEDs if this happens
                // wait for Record or Play to be pressed
                bufferFilled=true;
            }
        }
        
        // update playback gain
        if(playbackGainInc!=0)
        {
            playbackGain+=playbackGainInc;
            if(playbackGain>1)
            {
                playbackGain=1;
                playbackGainInc=0;
            }
            else if (playbackGain<0)
            {
                playbackGain=0;
                playbackGainInc=0;
            }
        }
        
        // update record gain
        if(recordGainInc!=0)
        {
            recordGain+=recordGainInc;
            if(recordGain>1)
            {
                recordGain=1;
                recordGainInc=0;
            }
            else if (recordGain<0)
            {
                recordGain=0;
                recordGainInc=0;
            }
        }
        OutputLevel+=OutputLevelInc;
    }
}

// This is called when an input param is changed
void updateInputParametersForBlock(const TransportInfo@ info)
{
    
    // Reverse--------------------------------------------------------------------------
    ReverseArmed=inputParameters[kReverse]>.5;
    if(ReverseArmed) {
        startReverse();
    }
    else {
        stopReverse();
    }

    // playing--------------------------------------------------------------------------
    bool wasPlaying=playing;
    bool playWasArmed=playArmed;
    playArmed=inputParameters[kPlayParam]>.5;
    
    // start playing right now
    if(!wasPlaying && playArmed) { 
        startPlayback(); 
    }

    // stop playing right now
    if(wasPlaying && playWasArmed && !playArmed) { 
        stopPlayback(); 
    }

    // recording------------------------------------------------------------------------
    bool wasRecording=recording;
    bool wasArmed=recordingArmed;
    recordingArmed=inputParameters[kRecordParam]>.5;
    
    // continue recording if already recording
    if(recordingArmed && loopDuration !=0) {
        recording=true;
    }

    // stop recording
    if(wasRecording) {
        stopRecording();
        startPlayback();
    }
    
    // start recording at current playback index
    if(!wasRecording && recording)
    {
        startRecording();
    }
    
    // erase content and restart recording to 0
    // TODO: This should happen when record is pressed while playing
    bool eraseVal=true;
    if(eraseVal != eraseValueMem)
    {
        eraseValueMem=eraseVal;
        loopDuration=0;
        currentRecordingIndex=0;
        currentPlayingIndex=0;
        if(!recordingArmed) {
            recording=false;
        }
    }
    
    // OutputLevel
    OutputLevel=inputParameters[kOutputLevelParam];
}

void computeOutputData()
{
    // playback status
    if(isPlaying() && loopDuration!=0) {
        outputParameters[kPlayLed]=1;
    }
    else
        outputParameters[kPlayLed]=0;

    // recording status
    if(recording)
        outputParameters[kRecordLed]=1;
    else
        outputParameters[kRecordLed]=0;

    // Reverse status
    if(Reverse)
        outputParameters[kReverseLed]=1;
    else
        outputParameters[kReverseLed]=0;

}




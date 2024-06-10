/** \file
 *   looper.
 *   Record and loop/overdub
 */

// DONE: remove rec trigger
// DONE: set default rec mode to boomerang mode
// DONE: remove snap
// TODO: figure out how to put the leds on top of the buttons (maybe need the custom gui)
// DONE: link leds to switches
// TODO: add stack mode
// TODO: add ONCE mode (wip)
// TODO: Flash record LED when loop cycles around
// TODO: add 1/2 speed mode
// TODO: figure out how to flip Play switch when record stops
// TODO: create function to determine toggle state, for consistency and DRY


// NOTES:
// - "armed" means "the button is pressed or in Enable state"


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
    kOnceParam,
    kReverseParam,
    kStackParam,
    kHalfSpeedParam

};

enum RecordMode
{
    kRecLoopOver=0, ///< standard looper: records over existing material and keep original loop length
    kRecExtend,     ///< records over existing material and extends original loop length when reaching end of loop
    kRecAppend,     ///< append to existing loop (without recording original loop content after loop duration has been reached) <-- stack mode?
    kRecOverWrite,  ///< overwrite loop content with new material, and extend original loop until recording ends
    kRecPunch,      ///< overwrite loop content with new material, but keeps original loop length
    kRecClear,       ///< clear original loop when recording starts  <-- pretty sure this is the boomerang mode when press record (5)
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
    0  // 1/2 Speed
};
array<double> inputParametersMax={
    10,  // Output Level, if not entered, defaults to percentage
    1,   // Thru Mute
    1,   // Record
    1,   // Play/Stop
    1,   // Once
    1,   // Reverse
    1,   // Stack
    1    // 1/2 Speed
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
    2   // 1/2 Speed
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
    ";1/2 Speed"   // 1/2 Speed
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
 * These appear to be global variables that are used to store the state of the plugin
 */
array<array<double>> buffers(audioInputsCount);

const int MAX_LOOP_DURATION_SECONDS=60; // 60 seconds max recording

int allocatedLength=int(sampleRate * MAX_LOOP_DURATION_SECONDS); // 60 seconds max recording

bool recording=false;         // currently recording
bool recordingArmed=false;    // record button is pressed

double OutputLevel=0;         // output level

double playbackGain=0;        // playback gain
double playbackGainInc=0;     // playback gain increment

double recordGain=0;          // record gain
double recordGainInc=0;       // record gain increment

// playback gain reduction of 2.5 Db when stacking
// TODO: CHECK THIS MATH
const double STACK_GAIN_REDUCTION = 1.0/1.77827941003892; // 2.5 Db gain reduction

int currentPlayingIndex=0;    // current playback index
int currentRecordingIndex=0;  // current recording index

int loopDuration=0;           // loop duration in ?? it doesn't seem to matter

bool eraseValueMem=false;     // erase value memory  ???

const int fadeTime=int(.001 * sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);  // fade increment

RecordMode recordingMode=kRecClear; // Hardcode to boomerang mode. Clear loop when record is pressed. May end up not being necessary.

///// These are checked in the processBlock function - called for each block of samples
bool Reverse=false;       // reverse mode
bool ReverseArmed=false;  // reverse (direction) button is clicked (toggle)

bool playing=false;       // currently playing state
bool playArmed=false;     // play button is clicked (toggle)

bool onceMode=false;      // once mode
bool onceArmed=false;     // once button is clicked (momentary)

bool stackMode=false;     // stack mode
bool stackArmed=false;    // stack button is clicked (momentary)

bool halfSpeedMode=false;  // half speed mode
bool halfSpeedArmed=false; // half speed button is pressed (toggle)

bool bufferFilled=false;   // OOM
bool loopCycled=false;     // loop has looped around

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

void startRecording()
{
    // reset
    loopDuration=0;
    currentPlayingIndex=0;
    currentRecordingIndex=0;

    // actually start recording
    recording=true;

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
    
    // // recorded beyond previous loop -> update duration, start playback at 0
    // if(recordedCount>loopDuration)
    // Updated: just always do this
    loopDuration          = recordedCount;
    currentPlayingIndex   = 0;
}

void startPlayback()
{
    playing=true;
    // currentPlayingIndex=0;  // assume any restarting is done before this is called
    playbackGain=0;
    playbackGainInc=xfadeInc; // ???
}

void stopPlayback()
{
    playing=false;
    playbackGainInc=-xfadeInc; // ???
}

bool isPlaying()
{
    // true if playing or if playback gain is not 0
    return (playing || (playbackGainInc !=0 ));
}

void enableReverse()
{
    if(Reverse==false && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=true;
}

void disableReverse()
{
    if(Reverse==true && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=false;
}

// This is called for each block of samples
void processBlock(BlockData& data) {

    // // indexes used by auto record trigger or snapped play/stop
    // i don't think we need any of this but it's coupled to a lot of important stuff below
    int startRecordingSample=-1; // sample number in buffer when recording should be started
    int stopRecordingSample=-1; // sample number in buffer when recording should be stopped
    int startPlayingSample=-1; // sample number in buffer when playback should be started	
    int stopPlayingSample=-1;  // sample number in buffer when playback should be stopped
    int enableReverseSample=-1; // sample number in buffer when Reverse should be started
    int disableReverseSample=-1; // sample number in buffer when Reverse should be stopped

    // if not recording and recording is pressed -> start recording
    if(!recording && recordingArmed)
    {
        // for each channel, while the there is no startRecordingSample, find the first sample that is not 0
        for(uint channel=0; channel<audioInputsCount && startRecordingSample == -1; channel++)
        {
            // create sample buffer
            array<double>@ samplesBuffer=@data.samples[channel];

            // for each sample in the buffer, if the sample is not 0, set the startRecordingSample to the index of the sample
            for(uint i=0;i<data.samplesToProcess && startRecordingSample==-1;i++)
            {
                startRecordingSample=i;
            }
        }
        // not found? next buffer
        if(startRecordingSample==-1)
            startRecordingSample=data.samplesToProcess;
    }

    // smooth OutputLevel update: use begin and end values
    // this smoothes the transition between the begin and end values of the OutputLevel parameter,
    //  between the beginning and end of the block
    // since the actual gain is exponential, we can use the ratio between begin and end values
    // as an incremental multiplier for the actual gain
    double OutputLevelInc=(data.endParamValues[kOutputLevelParam] - data.beginParamValues[kOutputLevelParam])/data.samplesToProcess;
    
    // actual audio processing happens here
    // i don't think we need all this stop/start stuff since we're not using any snap/sync
    // for each sample in the buffer
    for(uint i=0; i<data.samplesToProcess; i++)
    {
        // manage recording state
        // start recording if startRecordingSample is the current sample
        bool doStartRecording=(int(i)==startRecordingSample);

        // stop recording if stopRecordingSample is the current sample
        bool doStopRecording=(int(i)==stopRecordingSample);
        
        if(doStartRecording) {
            startRecording();
        }
        else if(doStopRecording) {
            stopRecording();
        }
        
        // manage playback state
        // start playing if startPlayingSample is the current sample
        bool startPlaying=(int(i)==startPlayingSample);

        // stop playing if stopPlayingSample is the current sample
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
        bool startReversal=(int(i)==enableReverseSample);
        bool stopReversal=(int(i)==disableReverseSample);
        
        if(startReversal) {
            enableReverse();
        }
        else if(stopReversal) {
            disableReverse();
        }
        
        const bool currentlyPlaying=isPlaying();
        
        // process audio for each channel--------------------------------------------------
        // in STACK mode here is where we would reduce the existing audio by 2.5Db?
        for(uint channel=0; channel < audioInputsCount; channel++)
        {
            array<double>@ channelBuffer=@buffers[channel];
            array<double>@ samplesBuffer=@data.samples[channel];
            double input=samplesBuffer[i];
    
            double playback=0;
            if(currentlyPlaying)
            {
                // read loop content
                int index = currentPlayingIndex;
                if(Reverse && loopDuration > 0) {
                    index = loopDuration - 1 - index;
                }
                playback  = channelBuffer[index] * playbackGain;
            }
            
            // update buffer when recording
            if(recording || (recordGainInc != 0))
            {
                channelBuffer[currentRecordingIndex] = playback + recordGain * input;
            }
            
            // copy to output with OutputLevel
            samplesBuffer[i] = input + OutputLevel * playback;
        }
        // end process audio for each channel--------------------------------------------------
        
        
        // update playback index while playing
        if(currentlyPlaying)
        {
            // update index
            currentPlayingIndex++;
            if(currentPlayingIndex>=loopDuration)
                currentPlayingIndex=0;
                // is this where we blink the record LED at the beginning of the loop
                // i think we set a flag here and check it in the computeOutputData function

            
            // playback xfade
            if(loopDuration>0)
            {
                if(!recording) // ie playing
                {
                    if(currentPlayingIndex == (loopDuration - fadeTime))
                        playbackGainInc =- xfadeInc;
                    else if(currentPlayingIndex<fadeTime)
                        playbackGainInc = xfadeInc;
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
            if(loopDuration > 0 && currentRecordingIndex >= loopDuration)
            {
                currentRecordingIndex=0; // return to beginning of loop
            }
            if(currentRecordingIndex >= allocatedLength) // stop recording if reached the end of the buffer
            {
                stopRecording();
                recordGainInc=0;    // avoid post buffer recording
                allLedsOn();        // turn on all LEDs if this happens
                bufferFilled=true;  // down the line this flag will be checked to wait for Record or Play to be pressed
            }
        }
        
        // update playback gain
        if(playbackGainInc!=0)
        {
            playbackGain += playbackGainInc;
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


// This is called when an input param is changed - ie, a button/toggle was not clicked but changed
// BUT they will ALL be checked on ANY change, so we still have to keep a state of each status before we check it. If it different from the state, we we do something.
// This invalidates my concept of a momentary switch, but we can still use it as a toggle
// Another concept to internalize is that only one button can be pressed at a time, so we don't have to account for multiple button presses
void updateInputParametersForBlock(const TransportInfo@ info)
{
    // Reverse--------------------------------------------------------------------------
    ReverseArmed = inputParameters[kReverseParamParam]>.5;
    if(ReverseArmed) {
        enableReverse();
    }
    else {
        disableReverse();
    }

    // playing--------------------------------------------------------------------------
    bool wasPlaying=playing;                    // if we were playing when we entered this block
    bool playWasArmed=playArmed;                // if we were play armed when we entered this block
    playArmed=inputParameters[kPlayParam]>.5;   // if we are play armed now
    
    // start playing right now
    if(!wasPlaying && playArmed) { 
        startPlayback();
        // TODO: find way to flip UI toggle to ON
    }

    // stop playing right now
    // i don't think playWasArmed is necessary
    if(wasPlaying && playWasArmed && !playArmed) { 
        stopPlayback();
        // TODO: find way to flip UI toggle to OFF
    }

    // ONCE
    // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded, but the loop will playback only once.
    // Pressing ONCE during playback tells the Boomerang Phrase Sampler to finish playing the loop and then stop.
    // If the Boomerang Phrase Sampler is idle, pressing ONCE will playback your recorded loop one time.
    // After pressing this button, the ONCE LED will be turned on letting you know this is the last time through your loop.
    // There is an interesting twist in the way the ONCE button works. Pressing it while the ONCE LED is on will always immediately restart playback. Repeated presses produces a stutter effect sort of like record scratching.
    // TODO: new option: if playing in once mode, find a way to disable it so the loop repeats
    bool wasInOnceMode=onceMode;                    // if we were in once mode when we entered this block
    bool wasOnceArmed=onceArmed;                    // if we were once armed when we entered this block
    onceArmed = inputParameters[kOnceParam] >= .5;  // toggle on/off

    // pressing once...
    if(onceArmed) {
        // Pressing ONCE during playback (when not in Once Mode) tells the Boomerang Phrase Sampler to finish playing the loop and then stop. 
        if(playing && !onceMode) {
            onceMode=true;
        }
        // Pressing it while the ONCE LED is on (playing in once mode) will always immediately restart playback. 
        // theoretically, this should be used the next time a block is processed?
        // do we have to go to the sample level for instant response?
        // do have to / should we instead call startPlayback()?
        else if (playing && onceMode) {
            currentPlayingIndex=0;
        }
        // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded,
        // but the loop will playback only once. 
        else if (recording) {
            stopRecording();
            onceMode=true;
            startPlayback();
        }
        else if (!playing && !recording) {
            // If the Boomerang Phrase Sampler is idle, pressing ONCE will playback your recorded loop one time. 
            onceMode=true;
            startPlayback();    
        }
    } // there is no 'else' since this is a momentary switch and runs on any touch (ie any midi change)
    
    // recording------------------------------------------------------------------------
    // When it is pressed, recording begins and the RECORD LED lights up brightly. 
    // A second press ends the recording and the Boomerang Phrase Sampler begins playing back; the PLAY LED lights up brightly to indicate the change.
    // During playback the RECORD button can be pressed again and a new recording will begin. Recording erases any previously stored sounds. 
    // During playback the RECORD LED will blink briefly at the beginning of the loop each time it comes around.
    // bool wasRecording=recording;                      // if we were recording when we entered this block
    // bool wasArmed=recordingArmed;                     // if we were record armed when we entered this block
    recordingArmed=inputParameters[kRecordParam] >= 0;  // on any change, toggle - emulate a momentary switch
    
    if(recordingArmed) {
        // if idle, or playing, start a new recording
        if((!recording && !playing) || playing) {
            loopDuration=0;
            currentRecordingIndex=0;
            currentPlayingIndex=0;
            recording=true;
            startRecording();
        }
        // if recording, stop recording and play back
        else if(recording) {
            stopRecording();
            startPlayback();
            recording=false;
        }
    }

    // OutputLevel
    bool outputChanged = inputParameters[kOutputLevelParam] != OutputLevel;
    if(outputChanged) {
        OutputLevel = inputParameters[kOutputLevelParam];
    }
}

void computeOutputData()
{
    // playback status
    if(isPlaying() && loopDuration != 0) {
        outputParameters[kPlayLed] = 1;
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

    // Once status
    if(onceMode)
        outputParameters[kOnceLed]=1;
    else
        outputParameters[kOnceLed]=0;
}

void allLedsOn()
{
    outputParameters[kThruMuteLed]=1;
    outputParameters[kRecordLed]=1;
    outputParameters[kPlayLed]=1;
    outputParameters[kOnceLed]=1;
    outputParameters[kReverseLed]=1;
    outputParameters[kStackLed]=1;
    outputParameters[kSpeedLed]=1;
}
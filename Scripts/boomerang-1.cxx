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
// DONE: add ONCE mode
// TODO: Flash record LED when loop cycles around (wip - intermittent)
// TODO: add 1/2 speed mode
// TODO: figure out how to flip UI Play switch when record stops
// DONE: create function to determine toggle state, for consistency and DRY
// DONE: fix: record light goes on when playing
// DONE: fix: doesn't record
// DONE: thrumute (wip)
//
// FUTURE FEATURE IDEAS
// - record/play stop options
// - write loop to file
// - load loop from file

// NOTES:
// - "armed" means "the button is pressed or in Enable state"


/*  Effect Description
 *
 */
string name="Boomerang+ Phrase Sampler";
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
    kSpeedParam
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

enum ParamsStatus
{
    kParamOff=0,
    kParamOn
};


array<string> inputParametersNames={"Output Level", "Thru Mute", "Record", "Play (Stop)", "Once", "Direction", "Stack", "Speed"};
array<double> inputParameters(inputParametersNames.length);
array<double> inputParametersDefault={
    5, // OutputLevel
    kParamOff, // Thru Mute
    kParamOff, // Record
    kParamOff, // Play/Stop
    kParamOff, // Once
    kParamOff, // Direction
    kParamOff, // Stack
    kParamOff  // Speed
};
array<double> inputParametersMax={
    10,  // Output Level, if not entered, defaults to percentage
    kParamOn,   // Thru Mute
    kParamOn,   // Record
    kParamOn,   // Play/Stop
    kParamOn,   // Once
    kParamOn,   // Direction
    kParamOn,   // Stack
    kParamOn    // Speed
};

// these are the number of available steps/modes for each parameter - 1-based
array<int> inputParametersSteps={
    -1, // OutputLevel // -1 means continuous
    2,  // Thru Mute
    2,  // Record
    2,  // Play
    2,  // Once
    2,  // Direction
    2,  // Stack
    2   // Speed
};

// these are the labels under each input control
array<string> inputParametersEnums={
    "",            // Output Level
    ";THRU MUTE",  // Thru Mute
    ";Recording",  // Record
    ";Playing",    // Play
    ";",       // Once
    "Fwd;Rev",     // Direction
    ";STACK",       // Stack
    ";1/2 Speed"   // Speed
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
bool onceArmed=false;     // once button is clicked (toggle+)

bool stackMode=false;     // stack mode
bool stackArmed=false;    // stack button is clicked (momentary)

bool halfSpeedMode=false;  // half speed mode
bool halfSpeedArmed=false; // half speed button is pressed (toggle)

bool bufferFilled=false;   // OOM
bool loopCycled=false;     // loop has looped around

// The THRU MUTE foot switch, on the upper-left front panel, turns the through signal on or off and can be changed at any time
bool thruMute=false;       // Thru Mute (toggle)

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
    loopDuration           = 0;
    currentPlayingIndex    = 0;
    currentRecordingIndex  = 0;

    // pre fade to avoid clicks
    recordGain=0;
    recordGainInc=xfadeInc;

    // actually start recording
    recording=true;
}

void stopRecording()
{
    recording               = false;
    const int recordedCount = currentRecordingIndex;
    loopDuration            = recordedCount;
    // currentPlayingIndex     = 0; // assume any restarting is done before this is called

    // post fade to avoid clicks
    recordGainInc = -xfadeInc;
}

void startPlayback()
{
    playing               = true;
    // currentPlayingIndex = 0;  // assume any restarting is done before this is called
    playbackGain          = 0;
    playbackGainInc       = xfadeInc; // very short playback gain ramp to avoid clicks?
}

void stopPlayback()
{
    playing         = false;
    playbackGainInc = -xfadeInc; // see above
}

bool isPlaying()
{
    // true if playing or if playback gain is not 0
    return (playing || playbackGainInc !=0);
}

bool isRecording()
{
    return (recording || recordGainInc !=0);
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
    // if recording and recording is pressed -> stop recording, start playback
    if(!recording && recordingArmed)  // only called when recording starts; if recording, this isn't called
    {
        // for each channel, while the there is no startRecordingSample, find the first sample that is not 0
        for(uint channel=0; channel<audioInputsCount && startRecordingSample == -1; channel++)
        {
            // create sample buffer
            array<double>@ samplesBuffer=@data.samples[channel];

            // for each sample in the buffer, if the sample is not null, set the startRecordingSample to the index of the sample
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
    // The OUTPUT LEVEL roller on the front panel of the Boomerang Phrase Sampler controls the playback volume but has no effect on the through signal.
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
        const bool currentlyRecording=isRecording();
        
        // process audio for each channel--------------------------------------------------
        // in STACK mode here is where we would reduce the existing audio by 2.5Db?
        for(uint channel=0; channel < audioInputsCount; channel++)
        {
            array<double>@ channelBuffer=@buffers[channel];       //  output buffer
            array<double>@ samplesBuffer=@data.samples[channel];  //  input buffer
            double input=samplesBuffer[i];
    
            double playback=0;
            if(currentlyPlaying)
            {
                // read loop content
                int playIndex = currentPlayingIndex;

                // literally where reverse is applied
                if(Reverse && loopDuration > 0) {
                    playIndex = loopDuration - 1 - playIndex;
                }

                // playback is the current incoming sample from the record buffer, multiplied by the playback gain
                // playback gain is their internal way of fading in/out to avoid clicks
                // so this is the recorded data being played into the output buffer
                playback  = channelBuffer[playIndex] * playbackGain;
            }
            
            // update buffer when recording
            if(currentlyRecording)
            {           
                // record input
                channelBuffer[currentRecordingIndex] = playback + (recordGain * input);
            }
            
            // copy to output with OutputLevel
            // if thru mute is on, don't include the input
            // in stack mode, reduce the playback by 2.5Db
            //  already defined at top of file, copied here for reference
            //  const double STACK_GAIN_REDUCTION = 1.0/1.77827941003892; // 2.5 Db gain reduction
            // else, add the input to the output buffer (with OutputLevel applied)
            if(thruMute)
                samplesBuffer[i] = OutputLevel * playback;
            else if(stackMode)
                samplesBuffer[i] = input + (OutputLevel * playback * STACK_GAIN_REDUCTION);
            else
                samplesBuffer[i] = input + (OutputLevel * playback);
        }
        // end process audio for each channel--------------------------------------------------
        
        
        // update playback index while playing
        if(currentlyPlaying)
        {
            // update index
            currentPlayingIndex++;
            if(currentPlayingIndex>=loopDuration) {
                // in once mode, stop playback after one cycle
                if(onceMode) {
                    print("stopping playback in once mode");
                    onceMode=false;
                    stopPlayback();
                    currentPlayingIndex=0; // reset playback index
                }
                else {
                    loopCycled=true;         // set flag to blink record LED
                    currentPlayingIndex=0;   // loop around to the beginning
                }
            }
            
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


// This is called when an input param is changed - ie, a button/toggle was not previously clicked but changed
// BUT they will ALL be checked on ANY change, so we still have to keep a state of each status before we check it. If it is different from the state, we do something.
// This invalidates my concept of a momentary switch, but we can still use it as a toggle
// Another concept to internalize is that only one button can be pressed at a time, so we don't have to account for multiple button presses
// If bufferFilled, the BPS will stop recording and wait for Record or Play to be pressed.
void updateInputParametersForBlock(const TransportInfo@ info)

    print("--------------\nParam Changed\n--------------");
    print("Loop Duration:" + loopDuration);
    print("Current Playing Index:" + currentPlayingIndex);
    print("Current Recording Index:" + currentRecordingIndex);

    // Reverse--------------------------------------------------------------------------
    // If the unit is playing back, pressing this button will immediately reverse the direction through your loop, resulting in reversed audio output. 
    // DIRECTION can be pressed any number of times during playback with a resulting instantaneous reversal of playback direction with each press.
    bool wasReverse = Reverse;                                  // if we were in reverse when we entered this block
    ReverseArmed    = isArmed(inputParameters[kReverseParam]);  // if the reverse toggle is now on
    
    print("Reverse Mode: " + Reverse);
    print("ReverseArmed: " + ReverseArmed);

    if(switchChanged(wasReverse, ReverseArmed)) {
        print("Reverse button was pressed");
        // if we were in reverse and the toggle is now off, disable reverse
        if(wasReverse && !ReverseArmed) {
            disableReverse();   // sets `Reverse` false
            print("--> disabling reverse");
        }
        // if we were not in reverse and the toggle is now on, enable reverse
        else if(!wasReverse && ReverseArmed) {
            enableReverse();    // sets `Reverse` true
            print("--> enabling reverse");
        }
    }

    // PLAY/STOP --------------------------------------------------------------------------
    // If the Rang is recording, pressing PLAY/STOP halts the recording and the unit becomes idle; your music is recorded and ready for playback.
    // If the Rang is playing back, pressing PLAY/STOP halts playback and the unit becomes idle.
    // If idle, pressing PLAY starts playback of whatever was last recorded, in a continuously looping manner.
    // If bufferFilled, clear that state and start playback.
    // During playback the PLAY LED will be on and the RECORD LED will blink at the beginning of each pass through the loop.
    bool wasPlaying = playing;                         // if we were playing when we entered this block
    bool playWasArmed = playArmed;                     // if we were play armed when we entered this block
    playArmed = isArmed(inputParameters[kPlayParam]);  // if we are play armed now
    
    print("wasPlaying: " + wasPlaying);
    print("playWasArmed: " + playWasArmed);
    print("playArmed: " + playArmed);

    // if play button state changed
    if(switchChanged(playWasArmed, playArmed)) {
        print("Play button was pressed");
        // If idle, pressing PLAY starts playback of whatever was last recorded, in a continuously looping manner.
        if(!wasPlaying && playArmed) {
            // onceMode = false;                       // if we were in once mode, disable it. TODO: is this right? maybe future feature

            if(bufferFilled) {
                print("--> clearing buffer filled state");
                bufferFilled=false;
            }

            if(recording) {
                print("--> stopping recording");
                stopRecording();     // sets `recording` false
            }

            print("--> starting playback at 0");
            currentPlayingIndex = 0;
            startPlayback();                        // sets `playing` true
            // TODO: find way to flip UI play toggle to ON (separate from LED)
        }

        // if playing, play was on, and now it is toggled off, stop playing right now
        // i don't think playWasArmed is necessary
        if(wasPlaying && playWasArmed && !playArmed) { 
            print("--> stopping playback");
            stopPlayback();   // sets `playing` false
            // TODO: find way to flip UI play toggle to OFF (separate from LED)
        }
    }

    // ONCE
    // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded, but the loop will playback only once.
    // Pressing ONCE during playback tells the BPS to finish playing the loop and then stop.
    // If the BPS is idle, pressing ONCE will playback your recorded loop one time.
    // After pressing this button, the ONCE LED will be turned on letting you know this is the last time through your loop.
    // There is an interesting twist in the way the ONCE button works. Pressing it while the ONCE LED is on will always immediately restart playback. 
    //   Repeated presses produces a stutter effect sort of like record scratching.
    // Currently this switch is a toggle, but acts as a momentary switch: any change triggers the ONCE logic.
    // TODO: new option: if playing in once mode, find a way to disable it so the loop repeats
    // bool wasOnceMode  = onceMode;                               // if we were in once mode when we entered this block
    bool wasOnceArmed = onceArmed;                              // get once toggle state before we entered this block
    onceArmed         = isArmed(inputParameters[kOnceParam]);   // set onceArmed to current toggle state

    print("onceMode: " + onceMode);
    print("wasOnceArmed: " + wasOnceArmed);
    print("onceArmed: " + onceArmed);

    if(switchChanged(wasOnceArmed, onceArmed)) {
        print("--- Once pressed ---");
        if(playing && !onceMode) {
            // Pressing ONCE during playback, when not in Once Mode, tells the Boomerang to finish playing the loop and then stop.
            print("--> setting once mode true");
            onceMode=true;
        }
        // Pressing it while the ONCE LED is on (playing in once mode) will always immediately restart playback. 
        // theoretically, this should be used the next time a block is processed?
        // do we have to go to the sample level for instant response?
        // do have to / should we instead call stop/startPlayback()?
        else if (playing && onceMode) {
            print("--> setting playback index to 0");
            currentPlayingIndex=0;   // immediately restart playback at the beginning of the loop
        }
        // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded,
        // but the loop will playback only once. 
        else if (recording) {
            print("--> stopping recording");
            stopRecording(); // sets recording false
            print("--> setting playback index to 0");
            currentPlayingIndex=0;
            print("--> setting once mode true");
            onceMode=true;
            print("--> starting playback");
            startPlayback();  // sets playing true
        }
        else if (!playing && !recording) {
            // If the Boomerang Phrase Sampler is idle, pressing ONCE will playback your recorded loop one time.
            print("--> setting playback index to 0");
            currentPlayingIndex=0;
            print("--> setting once mode true");
            onceMode=true;
            print("--> starting playback");
            startPlayback();
        }
    }
    
    // RECORD ------------------------------------------------------------------------
    // When it is pressed, recording begins and the RECORD LED lights up brightly. 
    // A second press ends the recording and the BPS begins playing back; the PLAY LED lights up brightly to indicate the change.
    // During playback the RECORD button can be pressed again and a new recording will begin. Recording erases any previously stored sounds. 
    // During playback the RECORD LED will blink briefly at the beginning of the loop each time it comes around.
    bool wasRecordingArmed = recordingArmed;                           // get recording toggle state before we entered this block
    recordingArmed         = isArmed(inputParameters[kRecordParam]);   // set recordingArmed to current toggle state

    print("wasRecordingArmed: " + wasRecordingArmed);
    print("recordingArmed: " + recordingArmed);
    print("recording: " + recording);
    
    if(switchChanged(wasRecordingArmed, recordingArmed)) {
        if((!recording && !playing) || playing) {
            print("--> if playing or idle, stop playing, start new recording");
            if(playing) 
                stopPlayback();
            
            if(bufferFilled) {
                print("--> clearing buffer filled state");
                bufferFilled=false;
                // do we need to do allLedsOff() here?
                // no, they'll get reset 1-by-1 in computeOutputData()
            }
            
            print("--> starting recording");
            startRecording();
        }
        else if(recording) {
            // A second press ends the recording and the BPS begins playing back; the PLAY LED lights up brightly to indicate the change.
            print("if recording, stop recording and play back");
            print("--> stopping recording");
            stopRecording();
            
            // set loop duration to the length of the recording
            print("setting loopduration to current recording index");
            loopDuration = currentRecordingIndex; 
            
            // set playback to the beginning of the loop
            print("--> setting playback index to 0");
            currentPlayingIndex = 0;              
            
            print("--> starting playback");
            startPlayback();
        }
    }

    // Stack Mode
    // The STACK foot switch, on the upper-right front panel, turns the stack mode on or off and can be changed at any time.
    // When the stack mode is on, the playback volume is reduced by 2.5 dB.
    // Stack is definitely momentary - it's only on while the button is pressed
    // bool wasStackMode = stackMode;                                // if we were in stack mode when we entered this block
    bool stackWasArmed = stackArmed;                               // get stack toggle state before we entered this block
    stackArmed         = isArmed(inputParameters[kStackParam]);    // if the stack toggle is now on

    print("wasStackMode: " + wasStackMode);
    print("stackArmed: " + stackArmed);

    if(switchChanged(stackWasArmed, stackArmed)) {
        print("--- Stack ---");
        // if we were in stack mode and the toggle is now off, disable stack mode
        if(stackMode && !stackArmed) {
            stackMode=false;
            print("--> disabling stack mode");
        }
        // if we were not in stack mode and the toggle is now on, enable stack mode
        else if(!wasStackMode && stackArmed) {
            stackMode=true;
            print("--> enabling stack mode");
        }
    }

    // half speed
    // The 1/2 SPEED foot switch, on the lower-right front panel, turns the half-speed mode on or off and can be changed at any time.
    // When the half-speed mode is on, the playback speed is halved.
    // my thinking is, since we can't actually change the sample rate, we'll have to use another technique to slow down the playback
    // roughly, copy every sample and play it back right after the original (play it twice)
    // or instead of copying every sample - which would double the memory allocation - we could just play the same sample twice
    // using a toggle to represent state
    // ie check toggle, (if toggle on, play sample and toggle off; if not, toggle on), play sample
    // if(speedToggle) {
    //     playSample(); // all this will be taken out of here and will be actually played in the playback section
    //                   // so really all this part has to do is flippo the togglo
                         // so     speedToggle = !speedToggle;
    //     speedToggle = false;
    // }
    // else {
    //     speedToggle = true;
    // }
    //     playSample();
    // this is now for the actual switching - just a standard toggle
    // but it only changes the speed when idle
    // and it's actually the same physical button as the stack mode
    // so if playing, it will only change the stack mode
    // if idle, it will toggle the speed mode
    wasHalfSpeedMode = halfSpeedMode;                                // if we were in half speed mode when we entered this block
    bool halfSpeedWasArmed = halfSpeedArmed;                         // get half speed toggle state before we entered this block

    // OutputLevel
    // The OUTPUT LEVEL roller on the front panel of the BPS controls the playback volume but has no effect on the through signal.
    double newOutputLevel = inputParameters[kOutputLevelParam];
    bool outputChanged = newOutputLevel != OutputLevel;
    if(outputChanged) {
        print("--> OutputLevel changed to " + newOutputLevel);
        OutputLevel = newOutputLevel;
    }

    // Thru Mute
    // The THRU MUTE foot switch, on the upper-left front panel, turns the through signal on or off and can be changed at any time
    // this currently functions as a TOGGLE
    bool wasThruMute  = thruMute;
    thruMute          = isArmed(inputParameters[kThruMuteParam]);

    if(switchChanged(wasThruMute, thruMute)) {
        print("Thru Mute button was pressed");

        string thruMuteState = thruMute ? "on" : "off";
        print("Thru Mute is now " + thruMuteState);
    }
}

void computeOutputData()
{
    // TODO: check this first
    if(bufferFilled) {
        allLedsOn();
    }
    else {
        // play LED status
        if(isPlaying() && loopDuration != 0)
            outputParameters[kPlayLed] = kParamOn;
        else
            outputParameters[kPlayLed] = kParamOff;

        // record LED status
        if(recording)
            outputParameters[kRecordLed]=kParamOn;
        else
            outputParameters[kRecordLed]=kParamOff;
        
        // if playing and loop has cycled, flash the record LED
        if(playing && loopCycled) {
            outputParameters[kRecordLed]=kParamOn;
            loopCycled=false;
        }

        // Reverse status
        if(Reverse)
            outputParameters[kReverseLed]=kParamOn;
        else
            outputParameters[kReverseLed]=kParamOff;

        // Once status
        if(onceMode)
            outputParameters[kOnceLed]=kParamOn;
        else
            outputParameters[kOnceLed]=kParamOff;

        // Thru Mute status
        if(thruMute)
            outputParameters[kThruMuteLed]=kParamOn;
        else
            outputParameters[kThruMuteLed]=kParamOff;

        // Stack status
        if(stackMode)
            outputParameters[kStackLed]=kParamOn;
        else
            outputParameters[kStackLed]=kParamOff;

        // 1/2 Speed status
        if(halfSpeedMode)
            outputParameters[kSpeedLed]=kParamOn;
        else
            outputParameters[kSpeedLed]=kParamOff;
    }
}

void allLedsOn()
{
    outputParameters[kThruMuteLed]=kParamOn;
    outputParameters[kRecordLed]=kParamOn;
    outputParameters[kPlayLed]=kParamOn;
    outputParameters[kOnceLed]=kParamOn;
    outputParameters[kReverseLed]=kParamOn;
    outputParameters[kStackLed]=kParamOn;
    outputParameters[kSpeedLed]=kParamOn;
}

void allLedsOff()
{
    outputParameters[kThruMuteLed]=kParamOff;
    outputParameters[kRecordLed]=kParamOff;
    outputParameters[kPlayLed]=kParamOff;
    outputParameters[kOnceLed]=kParamOff;
    outputParameters[kReverseLed]=kParamOff;
    outputParameters[kStackLed]=kParamOff;
    outputParameters[kSpeedLed]=kParamOff;
}

bool isArmed(double param) {
    return param >= .5;
}

bool switchChanged(bool oldState, bool newState) {
    return oldState != newState;
}

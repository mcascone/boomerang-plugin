/** \file
 *   looper.
 *   Record and loop/overdub
 */

// DONE: remove rec trigger
// DONE: set default rec mode to boomerang mode
// DONE: remove snap
// TODO: figure out how to put the leds on top of the buttons (maybe need the custom gui)
// DONE: link leds to switches
// DONE: add stack mode
// DONE: add ONCE mode
// DONE: Flash record LED when loop cycles around
// DONE: add 1/2 speed mode
// TODO: figure out how to flip UI switches
// DONE: create function to determine toggle state, for consistency and DRY
// DONE: fix: record light goes on when playing
// DONE: fix: doesn't record
// DONE: thrumute
// DONE: implement MIDI control
// TODO: tests
// TODO: bug: thru mute intermittently unmutes/remutes (#3)
// TODO: bug: pressing PLAY while recording doesn't stop recording (#4)
// DONE: bug: Stack doesn't work (issue #2)
// TODO: Record LED stops flashing during playback (i think due to outputParams not being processed) (#6)
// TODO: bug: Once when Idle doesn't set ONCE mode (#9)
//
// FUTURE FEATURE IDEAS
// - record/play stop options
// - write loop to file
// - load loop from file
// - change speed during playback (currently only works when idle)
// loop length + marker display

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
    // kSpeedParam
};

enum ParamsStatus
{
    kParamOff=0,
    kParamOn
};


array<string> inputParametersNames={"Output Level", "Thru Mute", "Record", "Play (Stop)", "Once", "Direction", "Stack (Speed)" };
array<double> inputParameters(inputParametersNames.length);
array<double> inputParametersDefault={
    .5, // OutputLevel
    kParamOff, // Thru Mute
    kParamOff, // Record
    kParamOff, // Play/Stop
    kParamOff, // Once
    kParamOff, // Direction
    kParamOff, // Stack
    // kParamOff  // Speed
};
array<double> inputParametersMax={
    1,  // Output Level, if not entered, defaults to percentage
    kParamOn,   // Thru Mute
    kParamOn,   // Record
    kParamOn,   // Play/Stop
    kParamOn,   // Once
    kParamOn,   // Direction
    kParamOn,   // Stack
    // kParamOn    // Speed
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
    // 2   // Speed
};

// these are the labels under each input control
array<string> inputParametersEnums={
    "",            // Output Level
    ";THRU MUTE",  // Thru Mute
    ";Recording",  // Record
    ";Playing",    // Play
    ";",       // Once
    "Fwd;Rev",     // Direction
    ";",       // Stack
    // ";1/2 Speed"   // Speed
};


/// OUTPUTS ///

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

array<string> outputParametersNames={"Thru Mute","Record","Play","Once","Reverse","Stack", "Slow"};
array<double> outputParameters(outputParametersNames.length);
array<double> outputParametersMin={0,0,0,0,0,0,0};
array<double> outputParametersMax={1,1,1,1,1,1,1};
array<string> outputParametersEnums={";",";",";",";",";",";",";"};

/// STRINGS ///
// /// An array of strings to be used as input for the script.
// /// Will be displayed in the user interface of the plug-in 
// array<string> inputStrings(3);
// /// Names to be displayed in the plug-in for the input strings. 
// array<string> inputStringsNames={"11","22","33"};

// /// An array of strings to be used as output for the script.
// /// Will be displayed in the user interface of the plug-in
// array<string> outputStrings(1);
// /// Names to be displayed in the plug-in for the input strings. 
// array<string> outputStringsNames={"Max Loop Len"};
// /// Maximum length for the ouput strings (output strings must be pre-allocated 
// /// to avoid audio dropouts).
// array<int> outputStringsMaxLengths={128};


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
// using reverse dB formula: gain=10^(gaindB/20)
// gain=pow(10,inputParameters[0]/20);
// const double STACK_GAIN_REDUCTION = 1.0/1.77827941003892; // 2.5 Db gain reduction <- math is wrong
const double STACK_GAIN_REDUCTION = 0.749894209; // 2.5 Db gain reduction

int currentPlayingIndex=0;    // current playback index
int currentRecordingIndex=0;  // current recording index

int loopDuration=0;           // loop duration in ?? it doesn't seem to matter

const int fadeTime=int(.001 * sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);  // fade increment

// These are checked in the processBlock function - called for each block of samples
bool Reverse=false;       // reverse mode
bool ReverseArmed=false;  // reverse (direction) button is clicked (toggle)

bool playing=false;       // currently playing state
bool playArmed=false;     // play button is clicked (toggle)

bool onceMode=false;      // once mode
bool onceArmed=false;     // once button is clicked (toggle/momentary)

bool stackMode=false;     // stack mode
bool stackArmed=false;    // stack button is clicked (momentary)

bool halfSpeedMode=false;  // half speed mode
bool halfToggle=false;     // half speed toggle
bool speedModeState=false; // speed mode counter
// bool halfSpeedArmed=false; // half speed button is pressed (toggle)

bool bufferFilled=false;   // OOM
bool loopCycled=false;     // loop has looped

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
    print("---initialized---");
}

int getTailSize()
{
    // infinite tail (sample player)
    return -1;
}

void startRecording()
{
    print("--> Start Recording");
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
    print("--> Stop Recording");
    recording               = false;
    const int recordedCount = currentRecordingIndex;
    loopDuration            = recordedCount;
    // currentPlayingIndex     = 0; // assume any restarting is done elsewhere

    // post fade to avoid clicks
    recordGainInc = -xfadeInc;
}

void startPlayback()
{
    print("--> Start Playing");
    playing               = true;
    currentPlayingIndex   = 0;
    playbackGain          = 0;
    playbackGainInc       = xfadeInc; // very short playback gain ramp to avoid clicks?
}

void stopPlayback()
{
    print("--> Stop Playing");
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
    print("--> enabling reverse");
    if(Reverse==false && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=true;
}

void disableReverse()
{
    print("--> disabling reverse");
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
        
        const bool currentlyPlaying   = isPlaying();
        const bool currentlyRecording = isRecording();
        
        // process audio for each channel--------------------------------------------------
        // in STACK mode here is where we reduce the existing audio by 2.5Db
        for(uint channel=0; channel < audioInputsCount; channel++)
        {
            // If i understand correctly, this is the looper's buffer.
            // It contains the recorded audio data... or WILL contain it?
            array<double>@ channelBuffer = @buffers[channel];       //  looper's buffer

            // @data is a pointer the incoming audio from the DAW/system.
            // so samplesBuffer is an array of pointers to the incoming sample for the current channel (i).
            // It will be handed to the plugin's buffer, which will do whatever processing is necessary, if any,
            // and then be replaced back into the samplesBuffer, which will be sent back to the DAW.
            // DATA FLOW: DAW -> @data -> samplesBuffer -> channelBuffer -> Plugin Processing -> channelBuffer -> samplesBuffer -> DAW
            array<double>@ samplesBuffer = @data.samples[channel];  //  input buffer

            // take the current sample from the input buffer
            double input = samplesBuffer[i];

            // this is what will eventually be put into the sampleBuffer to be sent back to the DAW,
            // after all processing is done, whether playing or recording.
            double playback=0;  

            if(currentlyPlaying) {
                // read loop content
                // this assignment is needed cause it changes if reverse
                int playIndex = currentPlayingIndex;

                // literally where reverse is applied
                if(Reverse && loopDuration > 0) {
                    playIndex = loopDuration - 1 - playIndex;
                }

                // playback is the current sample in the recorded buffer, multiplied by the playback gain
                // playback gain is their internal way of fading in/out to avoid clicks
                // so this is the existing loop data being set as 'playback'
                playback  = channelBuffer[playIndex] * playbackGain;

                // if in stack mode, reduce the original loop by 2.5Db, add the input,
                // and put it back into the buffer.
                // 2.5dB reduction already defined at top of file, copied here for reference:
                //  const double STACK_GAIN_REDUCTION = 0.749894209;
                // TODO: this doesn't work: orig loop is reduced, but no new input is added to the loop
                if(stackMode) {
                    playback *= STACK_GAIN_REDUCTION;    // reduce original loop by 2.5Db
                    playback += input;                   // add the input to the loop
                    channelBuffer[playIndex] = playback; // put the new audio into the channelBuffer 
                }
            }
            
            // update buffer when recording
            if(currentlyRecording) {           
                // record input
                channelBuffer[currentRecordingIndex] = playback + (recordGain * input);
            }
            
            // copy to output with OutputLevel
            // This is the step where the samplesBuffer is replaced with new data
            //   where it is then picked back up by the DAW
            // It isn't a separate step... @data is a reference so it is updated in place
            // if thru mute is on, don't include the input
            // else, add the input to the output buffer (with OutputLevel applied)
            // The input is not affected by the output level, only the loop data is.
            if(thruMute)
                samplesBuffer[i] = OutputLevel * playback;
            else
                samplesBuffer[i] = input + (playback * OutputLevel);
        }
        // end process audio for each channel--------------------------------------------------
                
        // update playback index while playing
        if(currentlyPlaying)
        {
            if(halfSpeedMode && halfToggle) {
                // flip halftoggle, don't update index
                // This the workaround for not being able to change the sample rate
                // we're just playing the same sample twice
                halfToggle = !halfToggle; 
            }
            else {
                // update index to next sample
                currentPlayingIndex++;
                halfToggle = !halfToggle;
            }
            
            if(currentPlayingIndex>=loopDuration) {
                // in once mode, stop playback after one cycle
                if(onceMode) {
                    stopPlayback();
                    onceMode = false;
                }
                else {
                    loopCycled = true;      // set flag to blink record LED
                    currentPlayingIndex=0;  // loop around to the beginning
                }
            }
            
            // playback xfade
            if(loopDuration>0)
            {
                if(!recording) // playing or idle
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
        if(isRecording()) {
            currentRecordingIndex++;
            // looping over existing => check boundaries
            if(loopDuration > 0 && currentRecordingIndex >= loopDuration) {
                currentRecordingIndex=0; // return to beginning of loop
            }

            // stop recording if reached the end of the buffer
            // future feature: Just start playing when buffer is full
            if(currentRecordingIndex >= allocatedLength) {
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

        OutputLevel += OutputLevelInc;
    }
}


// This is called when an input param is changed - ie, a button/toggle was not previously clicked but changed
// BUT they will ALL be checked on ANY change, so we still have to keep a state of each status before we check it. If it is different from the state, we do something.
// Momentary switches are implemented as toggles with an on/off counter.
// Another concept to internalize is that only one button can be pressed at a time, so we don't have to account for multiple button presses
// If bufferFilled, the BPS will stop recording and wait for Record or Play to be pressed.
void updateInputParametersForBlock(const TransportInfo@ info) {

    print("-------------- \nParam Changed\n--------------");
    print("Loop Duration: " + loopDuration);
    print("Current Playing Index: " + currentPlayingIndex);
    print("Current Recording Index: " + currentRecordingIndex);

    // Reverse--------------------------------------------------------------------------
    // If the unit is playing back, pressing this button will immediately reverse the direction through your loop, resulting in reversed audio output. 
    // DIRECTION can be pressed any number of times during playback with a resulting instantaneous reversal of playback direction with each press.
    bool wasReverse = Reverse;                                  // if we were in reverse when we entered this block
    ReverseArmed    = isArmed(inputParameters[kReverseParam]);  // if the reverse toggle is now on
    
    print("Reverse Mode: " + Reverse);
    print("ReverseArmed: " + ReverseArmed);

    if(switchChanged(wasReverse, ReverseArmed)) {
        if(ReverseArmed) {
            enableReverse();
        }
        else {
            disableReverse();
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
        // If idle, pressing PLAY starts playback of whatever was last recorded, in a continuously looping manner.
        if(!wasPlaying && playArmed) {
            // onceMode = false;                       // if we were in once mode, disable it. TODO: is this right? maybe future feature

            if(bufferFilled) {
                print("--> clearing buffer filled state");
                bufferFilled=false;
            }

            if(recording) {
                stopRecording();     // sets `recording` false
            }
            else {
                startPlayback();         // sets `playing` true
                // TODO: find way to flip UI play toggle to ON (separate from LED)
            }
        }

        // if playing, play was on, and now it is toggled off, stop playing right now
        // i don't think playWasArmed is necessary
        if(wasPlaying && playWasArmed && !playArmed) {
            stopPlayback();   // sets `playing` false
            
            // stackMode = stackMode ? false : stackMode; // if stack mode is on, turn it off
            
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
    // TODO: new feature: if playing in once mode, find a way to disable it so the loop repeats
    // bool wasOnceMode  = onceMode;                               // if we were in once mode when we entered this block
    bool wasOnceArmed = onceArmed;                              // get once toggle state before we entered this block
    onceArmed         = isArmed(inputParameters[kOnceParam]);   // set onceArmed to current toggle state

    print("onceMode: " + onceMode);
    print("wasOnceArmed: " + wasOnceArmed);
    print("onceArmed: " + onceArmed);

    if(switchChanged(wasOnceArmed, onceArmed)) {
        if(playing && !onceMode) {
            // Pressing ONCE during playback, when not in Once Mode, tells the Boomerang to finish playing the loop and then stop.
            print("--> setting once mode true");
            onceMode=true;
        }
        // Pressing it while the ONCE LED is on (playing in once mode) will always immediately restart playback. 
        // theoretically, this should be used the next time a block is processed?
        // do we have to go to the sample level for instant response? MAYBE
        // do have to / should we instead call stop/startPlayback()? NO
        else if (playing && onceMode) {
            currentPlayingIndex=0;   // immediately restart playback at the beginning of the loop
        }
        // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded,
        // but the loop will playback only once. 
        else if (recording) {
            stopRecording(); // sets recording false
            print("--> setting once mode true");
            onceMode=true;
            startPlayback();  // sets playing true
        }
        else if (!playing && !recording) {
            // If the Boomerang Phrase Sampler is idle, pressing ONCE will playback your recorded loop one time.
            print("--> setting once mode true");
            onceMode=true;
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
            if(playing) 
                stopPlayback();
            
            if(bufferFilled) {
                print("--> clearing buffer filled state");
                bufferFilled=false;
                // do we need to do allLedsOff() here?
                // no, they'll get reset 1-by-1 in computeOutputData()
            }
            
            startRecording();
        }
        else if(recording) {
            // A second press ends the recording and the BPS begins playing back; the PLAY LED lights up brightly to indicate the change.
            stopRecording();
            startPlayback();
        }
    }

    // Stack Mode
    // The STACK foot switch turns the stack mode on or off and can be changed at any time.
    // When the stack mode is on, the playback volume is reduced by 2.5 dB.
    // Stack is definitely momentary - it's only on while the button is pressed
    // This button has two main functions. 
    // 1: If the unit is idle, it toggles the speed setting: full or half speed. Full speed offers twice the bandwidth but reduces the recording time available. Half speed offers double recording time at the expense of bandwidth.
    // Sounds recorded at full speed may be played at half speed by stopping the system and changing the speed before the next playback.
    // Signals recorded at half speed may also be played back at full speed with a resulting chipmunk effect. 
    // Stacking on a bass line is also possible by recording at half speed and adding the bass line at full speed. When the result is played back at half speed the bass line will be dropped an octave.

    // 2: If STACK is pressed during playback, the system will accept additional input and add it to the existing loop so that on the next pass through the loop, both parts will playback together.  Stack Mode ~= overdub
    // This stacking of parts will continue for as long as the STACK button is held down. 
    // This is different than the other buttons, which are operated with a single tap. 
    // There is no hard limit to the number of parts that can be added in this manner.
    // Also note that the stacking feature works during reversed playback so any part can be recorded forward or reversed.
    //
    // Since the stacked signals are being added together, there is a practical upper limit to how large they can grow after repeated stacking. 
    // The system will internally attenuate the original loop by about 2.5dB to help insure no overloading will occur. 
    // Since the system is slightly attenuating the loop signal while the STACK button is pressed, if the STACK button is held down but no new signal is input, the result will be a very smooth fade-out of the recorded loop. This can be cool.
    //
    // If the loop is very short and the STACK button is held down while you continue to play, the effect is essentially the same as that of a conventional delay with a very slow decay setting. The OUTPUT LEVEL roller then becomes the effect/clean mix control. The cool thing is that the delay time is precisely controlled by two presses of the RECORD button, so it will be just what you need at the moment.
    // bool wasStackMode  = stackMode;                                // if we were in stack mode when we entered this block
    bool stackWasArmed = stackArmed;                               // get stack toggle state before we entered this block
    stackArmed         = isArmed(inputParameters[kStackParam]);    // if the stack toggle is now on

    print("stackMode: " + stackMode);
    print("halfSpeedMode: " + halfSpeedMode);
    print("stackArmed: " + stackArmed);

    if(switchChanged(stackWasArmed, stackArmed)) {
        if(isPlaying()) {
            // if we were in stack mode and the toggle is now off, disable stack mode
            if(stackMode && !stackArmed) {
                stackMode=false;
                print("--> disabling stack mode");
            }
            // if we were not in stack mode and the toggle is now on, enable stack mode
            else if(!stackMode && stackArmed) {
                stackMode=true;
                print("--> enabling stack mode");
            }
        }
        else {
            // if the unit is idle, it toggles the speed setting: full or half speed.
            // but the stack/speed button is momentary, and always ends in OFF
            // we'll always get an an on + off
            // so it's more of a counter + toggle
            if(speedModeState) {
                halfSpeedMode = !halfSpeedMode;
                print("--> halfSpeedMode: " + halfSpeedMode);
            }

            // this just flips every time the stack/speed is pressed on+off
            // resulting in the speed mode being toggled every other time.
            speedModeState = !speedModeState;
            print("--> speedModeState: " + speedModeState);
        }
    }

    // half speed
    // There is no half-speed switch. This feature's functionality is coupled to the state of STACK MODE.
    //  turns the half-speed mode on or off and can be changed at any time.
    // When the half-speed mode is on, the playback speed is halved.
    // my thinking is, since we can't actually change the sample rate, we'll have to use another technique to slow down the playback
    // roughly, copy every sample and play it back right after the original (play it twice)
    // or instead of copying every sample - which would double the memory allocation - we could just play the same sample twice
    // using a toggle to represent state
    // might still need to copy the buffer since it'll start to back up as the play is halved, but we'll see
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


    // OutputLevel
    // The OUTPUT LEVEL roller on the front panel of the BPS controls the playback volume but has no effect on the through signal.
    double newOutputLevel = inputParameters[kOutputLevelParam];
    if(newOutputLevel != OutputLevel) {
        print("--> OutputLevel changed to: " + newOutputLevel);
        OutputLevel = newOutputLevel;
    }

    // Thru Mute
    // The THRU MUTE foot switch, on the upper-left front panel, turns the through signal on or off and can be changed at any time
    // this currently functions as a TOGGLE
    bool wasThruMute  = thruMute;
    thruMute          = isArmed(inputParameters[kThruMuteParam]);

    if(switchChanged(wasThruMute, thruMute)) {
        print("--> Thru Mute: " + thruMute);
    }
}

void computeOutputData() {
    // check this first
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
        if(isRecording())
            outputParameters[kRecordLed] = kParamOn;
        else
            outputParameters[kRecordLed] = kParamOff;
        
        // if playing and loop has cycled, flash the record LED
        if(loopCycled) {
            outputParameters[kRecordLed] = kParamOn;
            loopCycled = false;
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


    // outputStrings[0] = MAX_LOOP_DURATION_SECONDS + "s";
    // outputStrings[1] = inputStrings[0];
}

void allLedsOn() {
    for (uint i=0; i<outputParameters.length; i++)
        outputParameters[i]=kParamOn;
}

void allLedsOff() {
    for (uint i=0; i<outputParameters.length; i++)
        outputParameters[i]=kParamOff;
}

bool isArmed(double param) {
    return param >= .5;
}

bool switchChanged(bool oldState, bool newState) {
    return oldState != newState;
}

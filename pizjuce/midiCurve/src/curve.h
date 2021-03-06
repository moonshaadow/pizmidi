#ifndef MidiCurvePLUGINFILTER_H
#define MidiCurvePLUGINFILTER_H

#include "../../common/PizAudioProcessor.h"

#define MAX_ENVELOPE_POINTS    (32)
#define MAX_ENVELOPE_LENGTH    (127.0f)
#define MAX_ENVELOPE_GAIN      (127.0f)
#define MAX_ENVELOPE_DOT_SIZE  (8)
#define midiScaler (0.007874015748031496062992125984252)
#define fmidiScaler (0.007874015748f)

enum parameters {
    kNumPointParams=MAX_ENVELOPE_POINTS*2, //x1,y1,x2,y2...
	kActive=kNumPointParams,//active1,active2,...
	kCC=kActive+MAX_ENVELOPE_POINTS,
	kCCNumber,
	kVelocity,
	kChannelPressure,
	kAftertouch,
	kChannel,
	kPitchBend,
    kNumParams,
	kNumPrograms = 128
};

class MidiCurvePrograms : ValueTree {	
friend class MidiCurve;
public:
	MidiCurvePrograms ();
	~MidiCurvePrograms () {};
	void set(int prog, const var::identifier &name, const var &newValue)
	{
		this->getChild(prog).setProperty(name,newValue,0);
	}
	const var get(int prog, const var::identifier &name)
	{
		return this->getChild(prog).getProperty(name);
	}
	MidiCurvePrograms(ValueTree& tree)
	{
		this->getChild(0).getParent() = tree.createCopy();
	}
};

//==============================================================================
class MidiCurve  : public PizAudioProcessor,
                   public ChangeBroadcaster
{
public:
    //==============================================================================
    MidiCurve();
    ~MidiCurve();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

	void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();

    //==============================================================================
#include "JucePluginCharacteristics.h"
    const String getName() const {return JucePlugin_Name;}
	bool hasEditor() const {return true;}
    bool acceptsMidi() const {
#if JucePlugin_WantsMidiInput 
        return true;
#else   
        return false;
#endif
    }
    bool producesMidi() const {
#if JucePlugin_ProducesMidiOutput
        return true;
#else 
        return false;
#endif
    }

    int getNumParameters()    { return kNumParams; }

    float getParameter (int index);
    void setParameter (int index, float newValue);

    const String getParameterName (int index);
    const String getParameterText (int index);

    const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;


    //==============================================================================

    int getNumPrograms()      { return kNumPrograms; }
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getCurrentProgramStateInformation (MemoryBlock& destData);
    void setCurrentProgramStateInformation (const void* data, int sizeInBytes);
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);


    //==============================================================================
    int lastUIWidth, lastUIHeight;
	class LastMsgToDisplay : public ChangeBroadcaster
	{
	public:
		LastMsgToDisplay() {lastCCOut = lastCCIn = -1;}
		~LastMsgToDisplay() {}
		int lastCCOut, lastCCIn;
	} lastMsg;
	float getPointValue(int n, int y);
	bool isPointActive(int point);
	bool isPointControl(int point);
	int getPrevActivePoint(int currentPoint);
	int getNextActivePoint(int currentPoint);
	void resetPoints(bool copyToProgram=true);
	Path path;

    //==============================================================================
    juce_UseDebuggingNewOperator



private:
    float param[kNumParams];

	class midiPoint {
	public:
		midiPoint(float x, float y, bool active, bool control)
		{
			p.setXY(x,y);
			isControl=control;
			isActive=active;
		}
		midiPoint() 
		{
			p.setXY(0.f,0.f);
			isControl=false;
			isActive=true;
		}
		~midiPoint() {};

		Point<float> p;
		bool isControl;
		bool isActive;
	};

	struct PointComparator {
		int compareElements(midiPoint a, midiPoint b) { return roundFloatToInt(a.p.getX()*127.f) - roundFloatToInt(b.p.getX()*127.f); }
	} pointComparator;

	Array<midiPoint> points;
	void updatePath();

    MidiCurvePrograms *programs;
    int curProgram;
    
    bool init;

	void copySettingsToProgram(int index);
	float findValue(float input);
	double linearInterpolate(double x,double y1,double y2,double x1,double x2);
	int findPBValue(int input);
};



#endif

#include "MidiChords.h"
#include "MidiChordsEditor.h"

//==============================================================================
/**
    This function must be implemented to create a new instance of your
    plugin object.
*/
PizAudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiChords();
}

//==============================================================================
MidiChordsPrograms::MidiChordsPrograms ()
: ValueTree("midiChordsValues")
{
	this->setProperty("lastProgram",0,0);
	this->setProperty("Version",3,0);
	for (int p=0;p<numProgs;p++) 
	{
		ValueTree progv("ProgValues");
		progv.setProperty("progIndex",p,0);

		progv.setProperty("Channel",0,0);
		progv.setProperty("LearnChannel",0,0);
		progv.setProperty("FollowInput",true,0);
		progv.setProperty("Transpose",0,0);
		progv.setProperty("ChordMode",Normal,0);
		progv.setProperty("Root",60,0);
		progv.setProperty("Guess",true,0);
		progv.setProperty("Flats",false,0);
		progv.setProperty("UsePC",false,0);
		progv.setProperty("Velocity",100,0);
		progv.setProperty("InputTranspose",false,0);
		progv.setProperty("ToAllChannels",false,0);

		progv.setProperty("Name","Program "+ String(p+1),0);
		progv.setProperty("lastUIWidth",600,0);
		progv.setProperty("lastUIHeight",400,0);
		progv.setProperty("lastTrigger",60,0);

		for (int t=0;t<128;t++)
		{
			ValueTree noteMatrix("NoteMatrix_T"+String(t));
			//for (int c=0;c<16;c++) {
			//	noteMatrix.setProperty("Ch"+String(c)+"_0-31", 0, 0);
			//	noteMatrix.setProperty("Ch"+String(c)+"_32-63", 0, 0);
			//	noteMatrix.setProperty("Ch"+String(c)+"_64-95", 0, 0);
			//	noteMatrix.setProperty("Ch"+String(c)+"_96-127", 0, 0);
			//}
			progv.addChild(noteMatrix,t,0);
		}
		this->addChild(progv,p,0);
	}

	//DBG(this->createXml()->createDocument(""));
}

//==============================================================================
MidiChords::MidiChords() : programs(0), curProgram(0)
{
	DBG("MidiChords()");

	demo = !readKeyFile();
	fillChordDatabase();
	programs = new MidiChordsPrograms();
	memset(numChordNotes,0,sizeof(numChordNotes));
    if (!loadDefaultFxb())
	{
		channel = 0;
		learnchan = 0;
		follow = true;
		usepc = false;
		transpose = 0;
		mode = Normal;
		root = 60;
		guess = true;
		flats = false;
		inputtranspose = false;
		ccToAllChannels = false;
		previewVel = 100;
		lastUIWidth = 600;
		lastUIHeight = 400;
		for (int i=0;i<128;i++)
			selectChordNote(i,i,true,1);
		selectTrigger(60);
	}
	memset(notePlaying,0,sizeof(notePlaying));
	memset(chordNotePlaying,0,sizeof(chordNotePlaying));
	for (int c=0;c<16;c++)
		for (int i=0;i<128;i++)
			outputNote[c][i]=-1;
	learn = false;
	learning = 0;
	playFromGUI = playingFromGUI = false;
	playButtonTrigger = curTrigger;
	init = true;
	setCurrentProgram(0);
}

MidiChords::~MidiChords()
{
	DBG("~MidiChords()");
	if (programs) deleteAndZero(programs);
}

//==============================================================================
const String MidiChords::getName() const
{
    return JucePlugin_Name;
}

int MidiChords::getNumParameters()
{
    return numParams;
}

void MidiChords::setCurrentProgram (int index)
{
    //save non-parameter info to the old program, except the first time
    if (!init) 
		copySettingsToProgram(curProgram);
    init = false;

    //then set the new program
    curProgram = index;
	programs->setProperty("lastProgram",index,0);
	channel = programs->get(index,"Channel");
	learnchan = programs->get(index,"LearnChannel");
	follow = programs->get(index,"FollowInput");
	transpose = programs->get(index,"Transpose");
	mode = programs->get(index,"ChordMode");
	root = programs->get(index,"Root");
	guess = programs->get(index,"Guess");
	flats = programs->get(index,"Flats");
	usepc = programs->get(index,"UsePC");
	previewVel = programs->get(index,"Velocity");
	inputtranspose = programs->get(index,"InputTranspose");
	ccToAllChannels = programs->get(index,"ToAllChannels");

    lastUIWidth = programs->get(index,"lastUIWidth");
    lastUIHeight = programs->get(index,"lastUIHeight");
	learn = false;
	
	for (int i=0;i<128;i++) {
		progKbState[curProgram][i].reset();
		for (int c=1;c<=16;c++) {
			for (int n=0;n<128;n++)
			{
				if (programs->getChordNote(index,i,c,n)) {
					progKbState[curProgram][i].noteOn(c,n,1.f);
				}
			}
		}
	}
	selectTrigger(programs->get(index,"lastTrigger"));
    sendChangeMessage();  
}

float MidiChords::getParameter (int index)
{
    switch (index)
    {
    case kChannel: return ((float)channel)/16.f;
    case kLearnChannel: return ((float)learnchan)/16.f;
    case kVelocity: return ((float)(previewVel-1))/126.f;
	case kUseProgCh: return usepc ? 1.f : 0.f;
	case kLearnChord: return learn ? 1.f : 0.f; 
	case kFollowInput: return follow ? 1.f : 0.f; 
	case kTranspose: return ((float)transpose+48)/96.f;
	case kMode: return (float)mode/(float)(numModes-1);
	case kRoot: return (float)root/127.f;
	case kGuess: return guess ? 1.f : 0.f;
	case kFlats: return flats ? 1.f : 0.f;
	case kInputTranspose: return inputtranspose ? 1.f : 0.f;
	case kToAllChannels: return ccToAllChannels ? 1.f : 0.f;
	default: return 0.0f;
    }
}

void MidiChords::setParameter (int index, float newValue)
{
    if (index==kChannel)
    {
        channel = roundToInt(newValue*16.f);
        sendChangeMessage();
    }
    else if (index==kLearnChannel)
    {
        learnchan = roundToInt(newValue*16.f);
        sendChangeMessage();
    }
    else if (index==kVelocity)
    {
        previewVel = roundToInt(newValue*126.f)+1;
        sendChangeMessage();
    }
	else if (index==kUseProgCh)
    {
        usepc = newValue>0.f;
        sendChangeMessage();
    }
	else if (index==kLearnChord)
    {
        learn = newValue>0.f;
		learning = 0;
        sendChangeMessage();
    }
	else if (index==kFollowInput)
    {
        follow = newValue>0.f;
        sendChangeMessage();
    }
    else if (index==kTranspose)
    {
        transpose = roundToInt(newValue*96.f)-48;
        sendChangeMessage();
    }
	else if (index==kMode)
	{
		mode = roundToInt(newValue*(numModes-1));
		if (mode==Global)
			selectTrigger(root);
		else
			selectTrigger(curTrigger);
		sendChangeMessage();
    }
	else if (index==kRoot)
	{
		root = roundToInt(newValue*127.f);
		if (mode==Global) {
			selectTrigger(root);
		}
        sendChangeMessage();
    }
    else if (index==kGuess)
    {
        guess = newValue>0.f;
        sendChangeMessage();
    }
    else if (index==kFlats)
    {
        flats = newValue>0.f;
        sendChangeMessage();
    }
    else if (index==kInputTranspose)
    {
        inputtranspose = newValue>0.f;
        sendChangeMessage();
    }
    else if (index==kToAllChannels)
    {
        ccToAllChannels = newValue>0.f;
        sendChangeMessage();
    }
}

const String MidiChords::getParameterName (int index)
{
    if (index == kChannel)
        return "Channel";
    if (index == kLearnChannel)
        return "LearnChannel";
	if (index == kUseProgCh)
		return "UseProgCh";
	if (index == kLearnChord)
		return "Learn";
	if (index == kFollowInput)
		return "FollowInput";
	if (index == kTranspose)
		return "Transpose";
	if (index == kMode)
		return "TriggerMode";
	if (index == kRoot)
		return "RootNote";
	if (index == kGuess)
		return "GuessChord";
	if (index == kFlats)
		return "Flats";
	if (index == kVelocity)
		return "PreviewVelocity";
	if (index == kInputTranspose)
		return "TranspInput";
	if (index == kToAllChannels)
		return "ToAllChannels";
	return String::empty;
}

const String MidiChords::getParameterText (int index)
{
    if (index == kChannel)
		return channel==0 ? "Any" : String(channel);
    if (index == kLearnChannel)
		return learnchan==0 ? "All" : String(learnchan);
    if (index == kUseProgCh)
        return usepc ? "Yes" : "No";    
	if (index == kLearnChord)
        return learn ? "Yes" : "No";
	if (index == kFollowInput)
        return follow ? "Yes" : "No";
    if (index == kTranspose) {
		if (transpose>0) return "+" + String(transpose);
		return String(transpose);
	}
	if (index == kMode) {
		if (mode==Normal) return "Full";
		if (mode==Octave) return "Octave";
		if (mode==Global) return "Global";
	}
	if (index==kRoot)
		return String(root);
    if (index == kGuess)
        return guess ? "Yes" : "No";  
    if (index == kFlats)
        return flats ? "Yes" : "No";  
	if (index == kVelocity)
		return String(previewVel);
    if (index == kInputTranspose)
        return inputtranspose ? "Yes" : "No";  
    if (index == kToAllChannels)
        return ccToAllChannels ? "Yes" : "No";  
	return String::empty;
}

const String MidiChords::getInputChannelName (const int channelIndex) const
{
    return String (channelIndex + 1);
}

const String MidiChords::getOutputChannelName (const int channelIndex) const
{
    return String (channelIndex + 1);
}

bool MidiChords::isInputChannelStereoPair (int index) const
{
    return true;
}

bool MidiChords::isOutputChannelStereoPair (int index) const
{
    return true;
}

//==============================================================================
void MidiChords::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void MidiChords::releaseResources()
{

}

void MidiChords::processBlock (AudioSampleBuffer& buffer,
                                MidiBuffer& midiMessages)
{
	MidiBuffer output;
	if (playFromGUI!=playingFromGUI)
	{
		int ch = jmax(channel,1);
		if(playFromGUI)
		{
			for (int c=1;c<=16;c++)
			{
				for (int n=0;n<128;n++)
				{
					if (progKbState[curProgram][curTrigger].isNoteOn(c,n)) {
						int newNote = n+transpose;
						if (chordNotePlaying[c][newNote]) {
							ignoreNextNoteOff[c-1].add(newNote);
							playingChord[curTrigger].add(ChordNote(c,newNote));
						}
						else if (newNote < 128 && newNote >= 0) 
						{
							output.addEvent(MidiMessage::noteOn(c,newNote,(uint8)previewVel),0);
							playingChord[curTrigger].add(ChordNote(c,newNote));
							chordNotePlaying[c][newNote]=true;
						}
					}
				}
			}
			notePlaying[ch][curTrigger]=true;
			playingFromGUI = true;
			playButtonTrigger = curTrigger;

		}
		else {
			int numNotes = playingChord[playButtonTrigger].size();
			if (numNotes==0) {
				if (outputNote[ch][curTrigger]!=-1) {
					output.addEvent(MidiMessage::noteOff(ch,outputNote[ch][playButtonTrigger]),0);
					outputNote[ch][playButtonTrigger]=-1;
				}
			}
			else {
				for (int i = 0; i<numNotes; i++) {
					const int chordNote = playingChord[playButtonTrigger][i].n;
					const int c = playingChord[playButtonTrigger][i].c;
					if (!ignoreNextNoteOff[c-1].contains(chordNote)) {
						output.addEvent(MidiMessage::noteOff(c,chordNote),0);
						chordNotePlaying[c][chordNote]=false;
					}
					else
						ignoreNextNoteOff[c-1].removeValue(chordNote);
				}
				playingChord[playButtonTrigger].clear();
			}
			notePlaying[ch][playButtonTrigger]=false;
			playingFromGUI = false;
		}
		playingFromGUI = playFromGUI;
	}
	MidiBuffer::Iterator mid_buffer_iter(midiMessages);
	MidiMessage m(0xf0);
	int sample;
	while(mid_buffer_iter.getNextEvent(m,sample)) 
	{
		bool blockOriginalEvent = false;
		if (m.isProgramChange() && usepc) {// && (m.isForChannel(channel) || channel==0)) {
			if (m.getProgramChangeNumber()<getNumPrograms()) 
				setCurrentProgram(m.getProgramChangeNumber());
		}
		if (channel==0 || m.isForChannel(channel))
		{
			if (m.isNoteOn()) {
				blockOriginalEvent = true;
				const int ch = m.getChannel();
				const int tnote = inputtranspose ? (m.getNoteNumber() - transpose) : m.getNoteNumber();
				const uint8 v = m.getVelocity();
				if (!learn)
				{
					bool triggered = false;
					int trigger = tnote;
					if (mode==Octave)
						trigger =  60 + tnote%12;
					else if (mode==Global)
						trigger = root;
					for (int c=1;c<=16;c++)
					{
						for (int n=0;n<128;n++)
						{
							if (progKbState[curProgram][trigger].isNoteOn(c,n)) {
								triggered = true;
								int newNote = n+transpose;
								if (mode==Global)
									newNote += tnote-root;
								else if (mode==Octave)
									newNote +=  12 * (tnote/12 - 60/12);
								if (chordNotePlaying[ch][newNote]) {
									ignoreNextNoteOff[c-1].add(newNote);
									playingChord[tnote].add(ChordNote(c,newNote));
								}
								else if (newNote < 128 && newNote >= 0) 
								{
									output.addEvent(MidiMessage::noteOn(c,newNote,v),sample);
									playingChord[tnote].add(ChordNote(c,newNote));
									chordNotePlaying[c][newNote]=true;
								}
							}
						}
					}
					if (triggered) sendChangeMessage();
					notePlaying[ch][tnote]=true;
					if (follow && mode!=Global)
						selectTrigger(trigger);
				}
			}
			else if (m.isNoteOff()) {
				blockOriginalEvent = true;
				const int ch = m.getChannel();
				const int note = inputtranspose ? (m.getNoteNumber() - transpose) : m.getNoteNumber();
				if (!learn)
				{
					int numNotes = playingChord[note].size();
					if (numNotes==0) {
						if (outputNote[ch][note]!=-1) {
							output.addEvent(MidiMessage::noteOff(ch,outputNote[ch][note]),sample);
							outputNote[ch][note]=-1;
						}
					}
					else {
						for (int i = 0; i<numNotes; i++) {
							const int chordNote = playingChord[note][i].n;
							const int c = playingChord[note][i].c;
							if (!ignoreNextNoteOff[c-1].contains(chordNote)) {
								output.addEvent(MidiMessage::noteOff(c,chordNote),sample);
								chordNotePlaying[c][chordNote]=false;
							}
							else
								ignoreNextNoteOff[c-1].removeValue(chordNote);
						}
						playingChord[note].clear();
					}
					notePlaying[ch][note]=false;
					sendChangeMessage();
				}
			}
			else if (m.getChannel()>0) {
				if (ccToAllChannels)
				{
					for(int ch=1;ch<=16;ch++)
					{
						m.setChannel(ch);
						output.addEvent(m,sample);
						blockOriginalEvent = true;
					}
				}
				else
					blockOriginalEvent = false;
			}
			else
				blockOriginalEvent = false;
		}

		if (learnchan==0 || m.isForChannel(learnchan))
		{
			if (m.isNoteOn()) {
				blockOriginalEvent = true;
				const int ch = m.getChannel();
				const int tnote = m.getNoteNumber();
				const uint8 v = m.getVelocity();
				if (learn)
				{
					if (learning==0) 
					{
						progKbState[curProgram][curTrigger].reset();
						chordKbState.reset();
					}
					learning++;
					selectChordNote(curTrigger,tnote,true,ch);
					int newNote = tnote+transpose;
					if (mode==Global)
						newNote += tnote-root;
					else if (mode==Octave)
						newNote += 12 * (tnote%12);
					if (newNote < 128 && newNote >= 0) {
						output.addEvent(MidiMessage::noteOn(ch,newNote,v),sample);
						outputNote[ch][tnote]=newNote;
					}
				}
			}
			else if (m.isNoteOff()) {
				blockOriginalEvent = true;
				const int ch = m.getChannel();
				const int note = m.getNoteNumber();
				if (learn)
				{
					learning--;
					if (outputNote[ch][note] < 128 && outputNote[ch][note] >= 0) {
						output.addEvent(MidiMessage::noteOff(ch,outputNote[ch][note]),sample);
						outputNote[ch][note]=-1;
					}
					if (learning<1)
						setParameterNotifyingHost(kLearnChord,0.f);
				}
			}
		}
		if (!blockOriginalEvent)
			output.addEvent(m,sample);
	}

	midiMessages.clear();
	midiMessages = output;

    for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
    {
        buffer.clear (i, 0, buffer.getNumSamples());
    }
}

//==============================================================================
AudioProcessorEditor* MidiChords::createEditor()
{
    return new MidiChordsEditor (this);
}

//==============================================================================
void MidiChords::getStateInformation (MemoryBlock& destData)
{
	copySettingsToProgram(curProgram);
	programs->writeToStream(MemoryOutputStream(destData,false));
}

void MidiChords::getCurrentProgramStateInformation (MemoryBlock& destData)
{
	copySettingsToProgram(curProgram);
	programs->getChild(curProgram).writeToStream(MemoryOutputStream(destData,false));
}

void MidiChords::setStateInformation (const void* data, int sizeInBytes)
{
	ValueTree vt = ValueTree::readFromStream(MemoryInputStream(data,sizeInBytes,false));
	if (vt.isValid())
	{
		if ((int)vt.getProperty("Version",0)>=3)
		{
			programs->removeAllChildren(0);
			for (int i=0;i<vt.getNumChildren();i++)
			{
				if ((int)vt.getChild(i).getProperty("Velocity")==0)
					vt.getChild(i).setProperty("Velocity",100,0);
				programs->addChild(vt.getChild(i).createCopy(),i,0);
			}
		}
		else {
			for (int index=0;index<vt.getNumChildren();index++)
			{
				programs->set(index,"Channel",vt.getChild(index).getProperty("Channel"));
				programs->set(index,"LearnChannel",vt.getChild(index).getProperty("LearnChannel"));
				programs->set(index,"UsePC",vt.getChild(index).getProperty("UsePC"));
				programs->set(index,"FollowInput",vt.getChild(index).getProperty("FollowInput"));
				programs->set(index,"Name",vt.getChild(index).getProperty("Name"));
				programs->set(index,"lastUIHeight",vt.getChild(index).getProperty("lastUIHeight"));
				programs->set(index,"lastUIWidth",vt.getChild(index).getProperty("lastUIWidth"));
				programs->set(index,"lastTrigger",vt.getChild(index).getProperty("lastTrigger"));
				programs->set(index,"Transpose",vt.getChild(index).getProperty("Transpose"));
				programs->set(index,"ChordMode",vt.getChild(index).getProperty("ChordMode"));
				programs->set(index,"Root",vt.getChild(index).getProperty("Root"));
				programs->set(index,"Guess",vt.getChild(index).getProperty("Guess"));
				programs->set(index,"Flats",vt.getChild(index).getProperty("Flats"));
				programs->set(index,"Velocity",100);

				for (int trigger=0;trigger<128;trigger++) {
					for (int ch=0;ch<16;ch++) {	
						for (int n=0;n<128;n++) {
							if (n<32) {
								int state = vt.getChild(index).getChild(ch).getProperty("T"+String(trigger)+"_0-31",0);
								if ((state & (1 << n)) != 0)
									programs->setChordNote(index,trigger,ch+1,n,true);
							}
							else if (n<64) {
								int note = n - 32;
								int state = vt.getChild(index).getChild(ch).getProperty("T"+String(trigger)+"_32-63",0);
								if ((state & (1 << note)) != 0)
									programs->setChordNote(index,trigger,ch+1,n,true);
							}
							else if (n<96) {
								int note = n - 64;
								int state = vt.getChild(index).getChild(ch).getProperty("T"+String(trigger)+"_64-95",0);
								if ((state & (1 << note)) != 0)
									programs->setChordNote(index,trigger,ch+1,n,true);
							}
							else if (n<128) {
								int note = n - 96;
								int state = vt.getChild(index).getChild(ch).getProperty("T"+String(trigger)+"_96-127",0);
								if ((state & (1 << note)) != 0)
									programs->setChordNote(index,trigger,ch+1,n,true);
							}
						}
					}
				}
			}
		}
	}
	init=true;
	setCurrentProgram(vt.getProperty("lastProgram",0));
}

void MidiChords::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
	ValueTree vt = ValueTree::readFromStream(MemoryInputStream(data,sizeInBytes,false));
	if ((int)vt.getProperty("Velocity")==0)
		vt.setProperty("Velocity",100,0);
	if(vt.getChildWithName("NoteMatrix_T0").isValid())
	{
		programs->removeChild(programs->getChild(curProgram),0);
		programs->addChild(vt,curProgram,0);
		programs->getChild(curProgram).setProperty("progIndex",curProgram,0);
	}
	else {
		programs->set(curProgram,"Channel",vt.getProperty("Channel"));
		programs->set(curProgram,"LearnChannel",vt.getProperty("LearnChannel"));
		programs->set(curProgram,"UsePC",vt.getProperty("UsePC"));
		programs->set(curProgram,"FollowInput",vt.getProperty("FollowInput"));
		programs->set(curProgram,"Name",vt.getProperty("Name"));
		programs->set(curProgram,"lastUIHeight",vt.getProperty("lastUIHeight"));
		programs->set(curProgram,"lastUIWidth",vt.getProperty("lastUIWidth"));
		programs->set(curProgram,"lastTrigger",vt.getProperty("lastTrigger"));
		programs->set(curProgram,"Transpose",vt.getProperty("Transpose"));
		programs->set(curProgram,"ChordMode",vt.getProperty("ChordMode"));
		programs->set(curProgram,"Root",vt.getProperty("Root"));
		programs->set(curProgram,"Guess",vt.getProperty("Guess"));
		programs->set(curProgram,"Flats",vt.getProperty("Flats"));
		programs->set(curProgram,"Velocity",100);

		programs->getChild(curProgram).setProperty("progIndex",curProgram,0);
		for (int trigger=0;trigger<128;trigger++) {
			programs->clearNoteMatrix(curProgram,trigger);
			for (int ch=0;ch<16;ch++) {	
				for (int n=0;n<128;n++) {
					if (n<32) {
						int state = vt.getChild(curProgram).getChild(ch).getProperty("T"+String(trigger)+"_0-31",0);
						if ((state & (1 << n)) != 0)
							programs->setChordNote(curProgram,trigger,ch+1,n,true);
					}
					else if (n<64) {
						int note = n - 32;
						int state = vt.getChild(curProgram).getChild(ch).getProperty("T"+String(trigger)+"_32-63",0);
						if ((state & (1 << note)) != 0)
							programs->setChordNote(curProgram,trigger,ch+1,n,true);
					}
					else if (n<96) {
						int note = n - 64;
						int state = vt.getChild(curProgram).getChild(ch).getProperty("T"+String(trigger)+"_64-95",0);
						if ((state & (1 << note)) != 0)
							programs->setChordNote(curProgram,trigger,ch+1,n,true);
					}
					else if (n<128) {
						int note = n - 96;
						int state = vt.getChild(curProgram).getChild(ch).getProperty("T"+String(trigger)+"_96-127",0);
						if ((state & (1 << note)) != 0)
							programs->setChordNote(curProgram,trigger,ch+1,n,true);
					}
				}
			}
		}
	}
	init=true;
	setCurrentProgram(curProgram);
}

void MidiChords::selectTrigger(int index)
{
	if (index>=0 && index<128)
	{
		if (mode==Octave)
			index = 60 + index%12;
		triggerKbState.noteOff(1,curTrigger);
		triggerKbState.noteOn(1,index,1.f);
		chordKbState.reset();
		for (int c=1;c<=16;c++) {
			for (int i=0;i<128;i++)
				if (progKbState[curProgram][index].isNoteOn(c,i)) 
					chordKbState.noteOn(c,i,1.f);
		}
		curTrigger = index;
		programs->set(curProgram,"lastTrigger",index);
		sendChangeMessage();
	}
}

void MidiChords::selectChordNote(int index, int note, bool on, int ch)
{
	if (ch==-1) ch=jmax(1,learnchan);
	if (note>=0 && note<128)
	{
		if (on) {
			progKbState[curProgram][index].noteOn(ch,note,1.f);
			if (index==curTrigger) chordKbState.noteOn(ch,note,1.f);
			++numChordNotes[curProgram][index];
		}
		else {
			progKbState[curProgram][index].noteOff(ch,note);
			if (index==curTrigger) chordKbState.noteOff(ch,note);
			--numChordNotes[curProgram][index];
		}
		programs->setChordNote(curProgram,index,ch,note,on);
		sendChangeMessage();
	}
}

void MidiChords::copySettingsToProgram(int index)
{
	programs->set(index,"Channel",channel);
	programs->set(index,"LearnChannel",learnchan);
	programs->set(index,"UsePC",usepc);
	programs->set(index,"FollowInput",follow);
    programs->set(index,"Name",getProgramName(index));
	programs->set(index,"lastUIHeight",lastUIHeight);
    programs->set(index,"lastUIWidth",lastUIWidth);
	programs->set(index,"lastTrigger",curTrigger);
	programs->set(index,"Transpose",transpose);
	programs->set(index,"ChordMode",mode);
	programs->set(index,"Root",root);
	programs->set(index,"Guess",guess);
	programs->set(index,"Flats",flats);
	programs->set(index,"Velocity",previewVel);
	programs->set(index,"InputTranspose",inputtranspose);
	programs->set(index,"ToAllChannels",ccToAllChannels);

	for (int i=0;i<128;i++) {
		programs->clearNoteMatrix(index,i);
		for (int c=1;c<=16;c++) {	
			for (int n=0;n<128;n++) {
				if (progKbState[index][i].isNoteOn(c,n))
					programs->setChordNote(index,i,c,n,true);
			}
		}
	}
}

void MidiChords::clearAllChords()
{
	for (int i=0;i<128;i++) {
		numChordNotes[curProgram][i]=0;
		progKbState[curProgram][i].reset();
		programs->clearNoteMatrix(curProgram,i);
	}
	chordKbState.reset();
	sendChangeMessage();
}

void MidiChords::resetAllChords() 
{
	int ch = jmax(1,learnchan);
	for (int i=0;i<128;i++) {
		numChordNotes[curProgram][i]=0;
		progKbState[curProgram][i].reset();
		progKbState[curProgram][i].noteOn(ch,i,1.f);
		programs->clearNoteMatrix(curProgram,i);
		programs->setChordNote(curProgram,i,ch,i,true);
	}
	chordKbState.reset();
	chordKbState.noteOn(ch,curTrigger,1.f);
	sendChangeMessage();
}

void MidiChords::clearChord(int trigger)
{
	numChordNotes[curProgram][trigger]=0;
	progKbState[curProgram][trigger].reset();
	programs->clearNoteMatrix(curProgram,trigger);
	if (trigger==curTrigger) 
	{
		chordKbState.reset();
		sendChangeMessage();
	}

}

void MidiChords::resetChord(int trigger)
{
	int ch = jmax(1,learnchan);
	numChordNotes[curProgram][trigger]=0;
	progKbState[curProgram][trigger].reset();
	progKbState[curProgram][trigger].noteOn(ch,trigger,1.f);
	programs->clearNoteMatrix(curProgram,trigger);
	programs->setChordNote(curProgram,trigger,ch,trigger,true);
	if (trigger==curTrigger) 
	{
		chordKbState.reset();
		chordKbState.noteOn(ch,curTrigger,1.f);
		sendChangeMessage();
	}
}

void MidiChords::transposeAll(bool up)
{
	chordKbState.reset();
	for (int i=0;i<128;i++)
	{
		bool oldState[16][128];
		for (int c=0;c<16;c++) {
			for (int n=0;n<128;n++)
				oldState[c][n] = progKbState[curProgram][i].isNoteOn(c+1,n);
		}
		progKbState[curProgram][i].reset();
		for (int c=0;c<16;c++) 
		{
			for (int n=0;n<128;n++)
			{
				if (oldState[c][n]) {
					if (up) 
					{
						progKbState[curProgram][i].noteOn(c+1,n+1,1.f);
						programs->setChordNote(curProgram,i,c+1,n+1,true);
						if (i==curTrigger) 
							chordKbState.noteOn(c+1,n+1,128);

					}
					else {
						progKbState[curProgram][i].noteOn(c+1,n-1,1.f);
						programs->setChordNote(curProgram,i,c+1,n-1,true);
						if (i==curTrigger) 
							chordKbState.noteOn(c+1,n-1,128);
					}
				}
				else {
					if (up) 
						programs->setChordNote(curProgram,i,c+1,n+1,false);
					else
						programs->setChordNote(curProgram,i,c+1,n-1,false);
				}
			}
		}
	}
	sendChangeMessage();
}

void MidiChords::transposeChord(int trigger, bool up)
{
	bool oldState[16][128];
	for (int c=0;c<16;c++) {
		for (int n=0;n<128;n++)
			oldState[c][n] = progKbState[curProgram][trigger].isNoteOn(c+1,n);
	}
	progKbState[curProgram][trigger].reset();
	if (trigger==curTrigger) 
		chordKbState.reset();
	for (int c=0;c<16;c++) {
		for (int n=0;n<128;n++)
		{
			if (oldState[c][n]) {
				if (up) 
				{
					progKbState[curProgram][trigger].noteOn(c+1,n+1,1.f);
					programs->setChordNote(curProgram,trigger,c+1,n+1,true);
					if (trigger==curTrigger) 
						chordKbState.noteOn(c+1,n+1,128);
				}
				else {
					progKbState[curProgram][trigger].noteOn(c+1,n-1,1.f);
					programs->setChordNote(curProgram,trigger,c+1,n-1,true);
					if (trigger==curTrigger) 
						chordKbState.noteOn(c+1,n-1,128);
				}
			}
			else {
				if (up) 
					programs->setChordNote(curProgram,trigger,c+1,n+1,false);
				else
					programs->setChordNote(curProgram,trigger,c+1,n-1,false);
			}
		}
	}
	if (trigger==curTrigger) 
	sendChangeMessage();
}

void MidiChords::transposeCurrentChordByOctave(bool up)
{
	bool oldState[16][128];
	for (int c=0;c<16;c++) {
		for (int n=0;n<128;n++)
			oldState[c][n] = progKbState[curProgram][curTrigger].isNoteOn(c+1,n);
	}
	progKbState[curProgram][curTrigger].reset();
	chordKbState.reset();
	for (int c=0;c<16;c++) {
		for (int n=0;n<128;n++)
		{
			if (oldState[c][n]) {
				if (up) 
				{
					progKbState[curProgram][curTrigger].noteOn(c+1,n+12,1.f);
					programs->setChordNote(curProgram,curTrigger,c+1,n+12,true);
					chordKbState.noteOn(c+1,n+12,128);
				}
				else {
					progKbState[curProgram][curTrigger].noteOn(c+1,n-12,1.f);
					programs->setChordNote(curProgram,curTrigger,c+1,n-12,true);
					chordKbState.noteOn(c+1,n-12,128);
				}
			}
			else {
				if (up) 
					programs->setChordNote(curProgram,curTrigger,c+1,n+12,false);
				else
					programs->setChordNote(curProgram,curTrigger,c+1,n-12,false);
			}
		}
	}
	sendChangeMessage();
}


void MidiChords::savePreset(String name)
{
	String contents="Mode:";
	switch(mode)
	{
	case Global: contents+="Global\n"; break;
	case Octave: contents+="Octave\n"; break;
	case Normal: 
	default:
		contents+="Full\n"; break;
	}
	for (int t=0;t<128;t++)
	{
		bool usingTrigger = false;
		for (int n=0;n<128;n++)	{
			for (int c=1;c<=16;c++) {
				if (progKbState[curProgram][t].isNoteOn(c,n)) {
					if (!usingTrigger) {
						contents += String(t) + ":";
						usingTrigger = true;
					}
					contents += String(" ") + String(n-t)+"."+String(c);
				}
			}
		}
		if (usingTrigger)
			contents += "\n";
	}
	File chordFile(getCurrentPath()+File::separatorString
		+"midiChords"+File::separatorString+"mappings"+File::separatorString+name+".chords");

	FileChooser myChooser ("Save preset...",chordFile,"*.chords");

	if (myChooser.browseForFileToSave(true))
	{
		File file (myChooser.getResult());
		if (!file.hasFileExtension("chords")) file = file.withFileExtension("chords");

		file.create();
		file.replaceWithText(contents);
	}
}

bool MidiChords::readKeyFile(File file)
{
	DBG("readKeyFile()");
	bool copy = false;
	if (file.exists())
	{
		copy=true;
	}
	else {
		file = File(getCurrentPath()+File::separatorString+"midiChords"+File::separatorString+"midiChordsKey.txt");
		if (!file.exists())
			file = File(getCurrentPath()+File::separatorString+"midiChordsKey.txt");
	}

	if (file.exists()) {
		XmlElement* xmlKey = decodeEncryptedXml(file.loadFileAsString(),"3,5097efd7266b329e878e65129df0c8fe3ffc188abd24625e3709c65b304f5e2bdfa157e6c3cc6de5c29e36138c29f2516b56c55827aa238135bf1e23b6cab4f7");
		if ( !xmlKey->getStringAttribute("product").compare("midiChords 1.x")
			&& xmlKey->getStringAttribute("username").isNotEmpty()
			&& xmlKey->getStringAttribute("time").isNotEmpty() )
		{
			demo=false;
			//infoString = "Registered to " + xmlKey->getStringAttribute("username");
			if (copy) 
				file.copyFileTo(File(getCurrentPath()+File::separatorString+"midiChords"+File::separatorString+"midiChordsKey.txt"));
			sendChangeMessage();
		}
		else
		{
			//infoString = "Bad midiChordsKey";
			sendChangeMessage();
		}
		if (xmlKey) deleteAndZero(xmlKey);
		return true;
	}
	return false;
}

void MidiChords::readChorderPreset(File file)
{
	XmlDocument xmlDoc(file);
	ScopedPointer<XmlElement> e = xmlDoc.getDocumentElement();
	if (e != 0)
	{
		MemoryBlock data;
		data.loadFromHexString(e->getAllSubText());

		struct ChorderMapping
		{
			uint32 header0;
			uint8 kbSect0[16];
			uint32 header1;
			uint8 kbSect1[16];
			uint32 header2;
			uint8 kbSect2[16];
			uint32 header3;
			uint8 kbSect3[16];
			uint32 header4;
			uint8 kbSect4[16];
			uint32 header5;
			uint8 kbSect5[16];
			uint32 header6;
			uint8 kbSect6[16];
			uint32 header7;
			uint8 kbSect7[16];
		};
		
		int ch = jmax(learnchan,1);
		//chordKbState.reset();
		ChorderMapping map;
		for (int i=0;i<128;i++)
		{
			data.copyTo(&map,i*sizeof(map),sizeof(map));
			progKbState[curProgram][i].reset();
			programs->clearNoteMatrix(curProgram,i);
			for (int m = 0; m<16; m++)
			{
				for (int n=0;n<8;n++) {
					if (map.kbSect0[m] & (1<<n)) {
						progKbState[curProgram][i].noteOn(ch,n + m*8,1.f);
						//chordKbState.noteOn(ch,n+m*8,1.f);
						programs->setChordNote(curProgram,i,ch,n+m*8,true);
					}
				}
			}
		}
		int otherSettings[5]; // 0: ?, 1: mode, 2: Layer switch, 3: Thru, 4: Used Layers 
		data.copyTo(&otherSettings,128*sizeof(map),sizeof(otherSettings));

		if (otherSettings[1]==0)
			mode = Normal;
		else if (otherSettings[1]==1)
			mode = Octave;
		else if (otherSettings[1]==2)
			mode = Global;
		programs->set(curProgram,"ChordMode",mode);
		changeProgramName(curProgram,file.getFileNameWithoutExtension());

		selectTrigger(curTrigger);
	}
}
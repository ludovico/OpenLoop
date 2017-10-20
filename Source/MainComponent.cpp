#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>
//#include "../Source/sol.hpp"
#include "chaiscript\chaiscript.hpp"
#include "chaiscript\chaiscript_stdlib.hpp"

#pragma warning (disable : 4100)

String getMachine() {
	if (File{ "c:/samples" }.exists()) {
		return "laptop";
	} else {
		return "desktop";
	}
}

void savePluginXml(String path) {
	VSTPluginFormat vstPluginFormat;
	OwnedArray<PluginDescription> plugindescs;
	vstPluginFormat.findAllTypesForFile(plugindescs, path);
	for (auto pd : plugindescs) {
		auto xml = pd->createXml();
		File filepath{ path };

		auto newFile = getMachine() == "desktop" ? 
			File{ "C:/code/c++/juce/OpenLoop/Resources/desktop/plugins/" + pd->descriptiveName + ".xml" } :
			File{ "C:/code/c++/juce/OpenLoop/Resources/laptop/plugins/" + pd->descriptiveName + ".xml" };

		xml->writeToFile(newFile, "");
		
		std::cout << "Wrote " << pd->descriptiveName << " to disk" << std::endl;
	}
}

void saveLaptopPlugins() {
	savePluginXml("C:/Program Files/VSTPlugins/Dexed.dll");
	//savePluginXml("C:/Program Files/Steinberg/VSTPlugins/FalconVSTx64.dll"); why?
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Battery 4.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/FM8.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Massive.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/RC 48.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Guitar Rig 5.dll");
}

void saveDesktopPlugins() {
	// U-HE
	savePluginXml("C:/Program Files/VSTPlugIns/ACE(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Bazille(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Diva(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Filterscape(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/FilterscapeQ6(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/FilterscapeVA(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Hive(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/ACE(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/MFM2(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Presswerk(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Protoverb(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Runciter(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Satin(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Zebra(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/ZRev(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-A(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-D(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-F(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-G(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-P(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-Q(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-S(x64).dll");
	savePluginXml("C:/Program Files/VSTPlugIns/Uhbik-T(x64).dll");

	// NI
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Battery 4.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/FM8.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Massive.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/RC 48.dll");
	savePluginXml("C:/Program Files/Native Instruments/VSTPlugins 64 bit/Guitar Rig 5.dll");
}

class PluginWindow : public DocumentWindow {
public:
	PluginWindow()
		:DocumentWindow("no plugin", Colour::fromHSV(0.0, 0.0, 0.0, 1.0), DocumentWindow::minimiseButton) {
		setSize(100, 100);
		setTopLeftPosition(0, 600);
		setVisible(true);
	}

	void clear() {
		clearContentComponent();
		setSize(100, 100);
		setName("no plugin");
	}

	void setEditor(AudioProcessorEditor* editor) {
		clear();
		if (editor != nullptr) {
			setName(editor->getName());
			setContentNonOwned(editor, true);
		}
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

struct MidiInputCallbackImpl : public MidiInputCallback {
	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) {
		handleIncomingMidiMessageFunc(source, message);
	}

	void handlePartialSysexMessage(MidiInput* source, const uint8* messageData, int numBytesSoFar, double timestamp) {
		handlePartialSysexMessageFunc(source, messageData, numBytesSoFar, timestamp);
	}

	std::function<void(MidiInput*, const MidiMessage&)> handleIncomingMidiMessageFunc = [](MidiInput*, const MidiMessage&) {};
	std::function<void(MidiInput*, const uint8*, int, double)> handlePartialSysexMessageFunc = [](MidiInput*, const uint8*, int, double) {};
};

class AudioProcessorPlayerV2 : public AudioIODeviceCallback {
public:
	AudioProcessorPlayerV2() {}

	virtual ~AudioProcessorPlayerV2() {
		setProcessor(nullptr);
	}

	void setProcessor(AudioProcessor* processorToPlay) {
		if (processor != processorToPlay) {
			if (processorToPlay != nullptr && sampleRate > 0 && blockSize > 0) {
				processorToPlay->setPlayConfigDetails(numInputChans, numOutputChans, sampleRate, blockSize);

				bool supportsDouble = processorToPlay->supportsDoublePrecisionProcessing() && isDoublePrecision;

				processorToPlay->setProcessingPrecision(supportsDouble ? AudioProcessor::doublePrecision
					: AudioProcessor::singlePrecision);
				processorToPlay->prepareToPlay(sampleRate, blockSize);
			}

			AudioProcessor* oldOne;

			{
				const ScopedLock sl(lock);
				oldOne = isPrepared ? processor : nullptr;
				processor = processorToPlay;
				isPrepared = true;
			}

			if (oldOne != nullptr)
				oldOne->releaseResources();
		}
	}

	void setDoublePrecisionProcessing(bool doublePrecision) {
		if (doublePrecision != isDoublePrecision) {
			const ScopedLock sl(lock);

			if (processor != nullptr) {
				processor->releaseResources();

				bool supportsDouble = processor->supportsDoublePrecisionProcessing() && doublePrecision;

				processor->setProcessingPrecision(supportsDouble ? AudioProcessor::doublePrecision
					: AudioProcessor::singlePrecision);
				processor->prepareToPlay(sampleRate, blockSize);
			}

			isDoublePrecision = doublePrecision;
		}
	}

	void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels, float** outputChannelData, int numOutputChannels, int numSamples) override {
		jassert(sampleRate > 0 && blockSize > 0);

		int totalNumChans = 0;

		if (numInputChannels > numOutputChannels) {
			tempBuffer.setSize(numInputChannels - numOutputChannels, numSamples,
				false, false, true);

			for (int i = 0; i < numOutputChannels; ++i) {
				channels[totalNumChans] = outputChannelData[i];
				memcpy(channels[totalNumChans], inputChannelData[i], sizeof(float) * (size_t)numSamples);
				++totalNumChans;
			}

			for (int i = numOutputChannels; i < numInputChannels; ++i) {
				channels[totalNumChans] = tempBuffer.getWritePointer(i - numOutputChannels);
				memcpy(channels[totalNumChans], inputChannelData[i], sizeof(float) * (size_t)numSamples);
				++totalNumChans;
			}
		} else {
			for (int i = 0; i < numInputChannels; ++i) {
				channels[totalNumChans] = outputChannelData[i];
				memcpy(channels[totalNumChans], inputChannelData[i], sizeof(float) * (size_t)numSamples);
				++totalNumChans;
			}

			for (int i = numInputChannels; i < numOutputChannels; ++i) {
				channels[totalNumChans] = outputChannelData[i];
				zeromem(channels[totalNumChans], sizeof(float) * (size_t)numSamples);
				++totalNumChans;
			}
		}

		AudioSampleBuffer buffer(channels, totalNumChans, numSamples);
		
		{
			const ScopedLock sl(lock);

			if (processor != nullptr) {
				const ScopedLock sl2(processor->getCallbackLock());

				if (!processor->isSuspended()) {
					if (processor->isUsingDoublePrecision()) {
						conversionBuffer.makeCopyOf(buffer, true);
						processor->processBlock(conversionBuffer, getMidiBuffer(numSamples));
						buffer.makeCopyOf(conversionBuffer, true);
					} else {
						processor->processBlock(buffer, getMidiBuffer(numSamples));
					}

					return;
				}
			}
		}

		for (int i = 0; i < numOutputChannels; ++i)
			FloatVectorOperations::clear(outputChannelData[i], numSamples);
	}

	void audioDeviceAboutToStart(AudioIODevice* device) override {
		auto newSampleRate = device->getCurrentSampleRate();
		auto newBlockSize = device->getCurrentBufferSizeSamples();
		auto numChansIn = device->getActiveInputChannels().countNumberOfSetBits();
		auto numChansOut = device->getActiveOutputChannels().countNumberOfSetBits();

		const ScopedLock sl(lock);

		sampleRate = newSampleRate;
		blockSize = newBlockSize;
		numInputChans = numChansIn;
		numOutputChans = numChansOut;

		channels.calloc((size_t)jmax(numChansIn, numChansOut) + 2);

		if (processor != nullptr) {
			if (isPrepared)
				processor->releaseResources();

			auto* oldProcessor = processor;
			setProcessor(nullptr);
			setProcessor(oldProcessor);
		}
	}

	void audioDeviceStopped() override {
		const ScopedLock sl(lock);

		if (processor != nullptr && isPrepared)
			processor->releaseResources();

		sampleRate = 0.0;
		blockSize = 0;
		isPrepared = false;
		tempBuffer.setSize(1, 1);
	}


	AudioProcessor* processor = nullptr;
	CriticalSection lock;
	double sampleRate = 0;
	int blockSize = 0;
	bool isPrepared = false, isDoublePrecision = true;
	int numInputChans = 0, numOutputChans = 0;
	HeapBlock<float*> channels;
	// interesting case: for simplicity we generate a MidiBuffer object and return it. Gives us warning C4239.
	std::function<MidiBuffer&(int)> getMidiBuffer = [&](int) -> MidiBuffer& { MidiBuffer mb;  return std::move(mb); };
	AudioBuffer<float> tempBuffer;
	AudioBuffer<double> conversionBuffer;
};

class TCPServer : public Thread {
public:
	TCPServer(chaiscript::ChaiScript& chai) : Thread("tcp-server"), chai{ chai } {}

	void run() override {
		serverSocket.createListener(8989, "127.0.0.1");
		char* buf = new char[50000];
		while (true) {
			auto connSocket = serverSocket.waitForNextConnection();
			int i;
			connSocket->read(&i, 4, true);
			connSocket->read(buf, i, true);
			buf[i] = 0;

			auto utf8 = CharPointer_UTF8{ buf };
			auto s = String{ utf8 }.toStdString();
			
			MessageManager::callAsync([&, s]() {
				try {
					chai.eval(s);
					std::cout << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << std::endl;
				} catch (std::exception e) {
					std::cout << e.what() << std::endl;
				}
			});
		}
	}

	//sol::state& lua;
	chaiscript::ChaiScript& chai;
	StreamingSocket serverSocket;
};

typedef std::map<double, chaiscript::Boxed_Value> eventmap;

class MainContentComponent : public AudioAppComponent {
public:
	MainContentComponent() {
		setSize(100, 100);
		setTopLeftPosition(0, 700);
		setAudioChannels(2, 2);

		//saveDesktopPlugins();

		/*
		Map from double to whatever.
		Add this to chaiscript_stdlib.hpp.
		bootstrap::standard_library::map_type<std::map<double, Boxed_Value>>("RealMap", *lib);
		*/

		typedef std::map<double, chaiscript::Boxed_Value> RealMap;
		typedef std::map<double, chaiscript::Boxed_Value>::iterator RealMapIterator;
		chai.add(chaiscript::bootstrap::standard_library::map_type<RealMap>("RealMap"));


		chai.add(chaiscript::fun(static_cast
			<RealMapIterator(RealMap::*)(const double&)>
			(&RealMap::lower_bound)), "lower_bound");
		chai.add(chaiscript::fun(static_cast
			<RealMapIterator(RealMap::*)() noexcept>
			(&RealMap::begin)), "begin");
		chai.add(chaiscript::fun(static_cast
			<RealMapIterator(RealMap::*)()>
			(&RealMap::end)), "end");
			
		typedef std::_Tree_iterator<std::_Tree_val<std::_Tree_simple_types<std::pair<const double, chaiscript::Boxed_Value>>>> _rmitertype;

		chai.add(chaiscript::user_type<RealMapIterator>(), "RealMapIterator");
		chai.add(chaiscript::fun(static_cast
			<void(*)(RealMapIterator&)>
			([](RealMapIterator& iter) { iter++; })), "++");
		chai.add(chaiscript::fun(static_cast
			<void(*)(RealMapIterator&)>
			([](RealMapIterator& iter) { iter--; })), "--");
		chai.add(chaiscript::fun(static_cast
			<bool(*)(RealMapIterator&, RealMapIterator&)>
			([](RealMapIterator& iter1, RealMapIterator& iter2) { return iter1 == iter2; })), "==");
		chai.add(chaiscript::fun(static_cast
			<bool(*)(RealMapIterator&, RealMapIterator&)>
			([](RealMapIterator& iter1, RealMapIterator& iter2) { return iter1 != iter2; })), "!=");
		chai.add(chaiscript::fun(static_cast
			<double(*)(RealMapIterator&)>
			([](RealMapIterator& iter) {return iter->first; })), "key");
		chai.add(chaiscript::fun(static_cast
			<chaiscript::Boxed_Value(*)(RealMapIterator&)>
			([](RealMapIterator& iter) {return iter->second; })), "value");

		chai.add(chaiscript::var(std::ref(deviceManager)), "deviceManager");
		chai.add(chaiscript::var(std::ref(pluginWindow)), "pluginWindow");

		chai.add(chaiscript::user_type<AudioDeviceManager>(), "AudioDeviceManager");
		chai.add(chaiscript::fun(&AudioDeviceManager::getCurrentAudioDeviceType), "getCurrentAudioDeviceType");
		chai.add(chaiscript::fun(&AudioDeviceManager::getCpuUsage), "getCpuUsage");
		chai.add(chaiscript::fun(&AudioDeviceManager::createStateXml), "createStateXml");
		chai.add(chaiscript::fun(&AudioDeviceManager::playTestSound), "playTestSound");
		chai.add(chaiscript::fun(&AudioDeviceManager::addAudioCallback), "addAudioCallback");
		chai.add(chaiscript::fun(&AudioDeviceManager::removeAudioCallback), "removeAudioCallback");
		chai.add(chaiscript::fun(&AudioDeviceManager::addMidiInputCallback), "addMidiInputCallback");
		chai.add(chaiscript::fun(&AudioDeviceManager::removeMidiInputCallback), "removeMidiInputCallback");
		chai.add(chaiscript::fun(&AudioDeviceManager::setMidiInputEnabled), "setMidiInputEnabled");

		chai.add(chaiscript::user_type<AudioPluginFormatManager>(), "AudioPluginFormatManager");
		chai.add(chaiscript::constructor<AudioPluginFormatManager()>(), "AudioPluginFormatManager");
		chai.add(chaiscript::fun(&AudioPluginFormatManager::addDefaultFormats), "addDefaultFormats");
		chai.add(chaiscript::fun(&AudioPluginFormatManager::createPluginInstance), "createPluginInstance");

		chai.add(chaiscript::user_type<AudioPluginInstance>(), "AudioPluginInstance");
		chai.add(chaiscript::base_class<AudioProcessor, AudioPluginInstance>());
		
		chai.add(chaiscript::user_type<AudioProcessor>(), "AudioProcessor");
		chai.add(chaiscript::fun(&AudioProcessor::getName), "getName");
		chai.add(chaiscript::fun(&AudioProcessor::getBusCount), "getBusCount");
		chai.add(chaiscript::fun(&AudioProcessor::isUsingDoublePrecision), "isUsingDoublePrecision");
		chai.add(chaiscript::fun(&AudioProcessor::getTotalNumInputChannels), "getTotalNumInputChannels");
		chai.add(chaiscript::fun(&AudioProcessor::getTotalNumOutputChannels), "getTotalNumOutputChannels");
		chai.add(chaiscript::fun(&AudioProcessor::getMainBusNumInputChannels), "getMainBusNumInputChannels");
		chai.add(chaiscript::fun(&AudioProcessor::getMainBusNumOutputChannels), "getMainBusNumOutputChannels");
		chai.add(chaiscript::fun(&AudioProcessor::acceptsMidi), "acceptsMidi");
		chai.add(chaiscript::fun(&AudioProcessor::producesMidi), "producesMidi");
		chai.add(chaiscript::fun(&AudioProcessor::reset), "reset");
		chai.add(chaiscript::fun(&AudioProcessor::createEditor), "createEditor");
		chai.add(chaiscript::fun(&AudioProcessor::createEditorIfNeeded), "createEditorIfNeeded");
		chai.add(chaiscript::fun(&AudioProcessor::getStateInformation), "getStateInformation");

		chai.add(chaiscript::user_type<AudioProcessorPlayerV2>(), "AudioProcessorPlayerV2");
		chai.add(chaiscript::base_class<AudioIODeviceCallback, AudioProcessorPlayerV2>());
		chai.add(chaiscript::constructor<AudioProcessorPlayerV2()>(), "AudioProcessorPlayerV2");
		chai.add(chaiscript::fun(&AudioProcessorPlayerV2::setProcessor), "setProcessor");
		chai.add(chaiscript::fun(&AudioProcessorPlayerV2::getMidiBuffer), "getMidiBuffer");
		chai.add(chaiscript::fun(&AudioProcessorPlayerV2::lock), "lock");

		chai.add(chaiscript::user_type<File>(), "File");
		chai.add(chaiscript::constructor<File()>(), "File");
		chai.add(chaiscript::constructor<File(const String&)>(), "File");
		chai.add(chaiscript::fun(static_cast<File(*)(std::string)>([](std::string s) { return File{ s }; })), "File");
		chai.add(chaiscript::fun(&File::exists), "exists");

		chai.add(chaiscript::user_type<MidiBuffer>(), "MidiBuffer");
		chai.add(chaiscript::constructor<MidiBuffer()>(), "MidiBuffer");
		chai.add(chaiscript::fun(static_cast<void(MidiBuffer::*)()>(&MidiBuffer::clear)), "clear");
		chai.add(chaiscript::fun(static_cast<void(MidiBuffer::*)(int, int)>(&MidiBuffer::clear)), "clear");
		chai.add(chaiscript::fun(&MidiBuffer::isEmpty), "isEmpty");
		chai.add(chaiscript::fun(&MidiBuffer::getNumEvents), "getNumEvents");
		chai.add(chaiscript::fun(static_cast<void(MidiBuffer::*)(const MidiMessage&, int)>(&MidiBuffer::addEvent)), "addEvent");
		chai.add(chaiscript::fun(static_cast<void(MidiBuffer::*)(const void*, int, int)>(&MidiBuffer::addEvent)), "addEvent");
		chai.add(chaiscript::fun(&MidiBuffer::addEvents), "addEvents");
		chai.add(chaiscript::fun(&MidiBuffer::getFirstEventTime), "getFirstEventTime");
		chai.add(chaiscript::fun(&MidiBuffer::getLastEventTime), "getLastEventTime");
		chai.add(chaiscript::fun(&MidiBuffer::swapWith), "swapWith");
		chai.add(chaiscript::fun(static_cast<bool(*)(MidiBuffer&, MidiBuffer&)>([](MidiBuffer& buf1, MidiBuffer& buf2) { return &buf1 == &buf2; })), "MidiBufferEqual");
		chai.add(chaiscript::fun(static_cast<int(*)(MidiBuffer&)>([](MidiBuffer& buf) { return reinterpret_cast<int>(&buf); })), "address");

		chai.add(chaiscript::user_type<MidiInput>(), "MidiInput");
		chai.add(chaiscript::fun(MidiInput::getDevices), "MidiInputGetDevices");
		chai.add(chaiscript::fun(MidiInput::openDevice), "MidiInputOpenDevice");
		chai.add(chaiscript::fun(&MidiInput::getName), "getName");
		chai.add(chaiscript::fun(&MidiInput::start), "start");
		chai.add(chaiscript::fun(&MidiInput::stop), "stop");

		chai.add(chaiscript::base_class<MidiInputCallback, MidiInputCallbackImpl>());
		chai.add(chaiscript::user_type<MidiInputCallbackImpl>(), "MidiInputCallbackImpl");
		chai.add(chaiscript::constructor<MidiInputCallbackImpl()>(), "MidiInputCallbackImpl");
		chai.add(chaiscript::fun(&MidiInputCallbackImpl::handleIncomingMidiMessageFunc), "handleIncomingMidiMessageFunc");
		chai.add(chaiscript::fun(&MidiInputCallbackImpl::handlePartialSysexMessageFunc), "handlePartialSysexMessageFunc");

		chai.add(chaiscript::user_type<MidiMessage>(), "MidiMessage");
		chai.add(chaiscript::fun(static_cast<MidiMessage(*)(int, int, uint8)>(MidiMessage::noteOn)), "MidiMessageNoteOn");
		chai.add(chaiscript::fun(static_cast<MidiMessage(*)(int, int, uint8)>(MidiMessage::noteOff)), "MidiMessageNoteOff");
		chai.add(chaiscript::constructor<MidiMessage()>(), "MidiMessage");
		chai.add(chaiscript::constructor<MidiMessage(int, int, int, double)>(), "MidiMessage");
		chai.add(chaiscript::fun(&MidiMessage::getTimeStamp), "getTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::setTimeStamp), "setTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::addToTimeStamp), "addToTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::getNoteNumber), "getNoteNumber");
		chai.add(chaiscript::fun(&MidiMessage::getVelocity), "getVelocity");
		chai.add(chaiscript::fun(&MidiMessage::isNoteOn), "isNoteOn");
		chai.add(chaiscript::fun(&MidiMessage::isNoteOff), "isNoteOff");
		chai.add(chaiscript::fun(&MidiMessage::isActiveSense), "isActiveSense");
		chai.add(chaiscript::fun(&MidiMessage::isSysEx), "isSysex");

		chai.add(chaiscript::user_type<PluginDescription>(), "PluginDescription");
		chai.add(chaiscript::constructor<PluginDescription()>(), "PluginDescription");
		chai.add(chaiscript::fun(&PluginDescription::loadFromXml), "loadFromXml");
		chai.add(chaiscript::fun(&PluginDescription::name), "name");

		chai.add(chaiscript::user_type<PluginWindow>(), "PluginWindow");
		chai.add(chaiscript::fun(&PluginWindow::clear), "clear");
		chai.add(chaiscript::fun(&PluginWindow::setEditor), "setEditor");

		chai.add(chaiscript::user_type<GenericScopedLock<CriticalSection>>(), "ScopedLock");
		chai.add(chaiscript::constructor<GenericScopedLock<CriticalSection>(const CriticalSection&)>(), "ScopedLock");

		chai.add(chaiscript::user_type<String>(), "String");
		chai.add(chaiscript::constructor<String(const std::string&)>(), "String");
		chai.add(chaiscript::fun(static_cast<std::string(*)(String)>([](String s) { return s.toStdString(); })), "to_string");
		
		chai.add(chaiscript::user_type<XmlDocument>(), "XmlDocument");
		chai.add(chaiscript::constructor<XmlDocument(String)>(), "XmlDocument");
		chai.add(chaiscript::constructor<XmlDocument(File)>(), "XmlDocument");
		chai.add(chaiscript::fun(&XmlDocument::getDocumentElement), "getDocumentElement");
		
		chai.add(chaiscript::user_type<XmlElement>(), "XmlElement");
		
		pluginWindow.toFront(true);
		tcpServer.startThread();
	}

	~MainContentComponent() {
		shutdownAudio();
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {

	}

	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
		bufferToFill.clearActiveBufferRegion();
	}

	void releaseResources() override {

	}

	void paint(Graphics& g) override {
		g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
	}

	void resized() override {

	}
	
	chaiscript::ChaiScript chai;
	TCPServer tcpServer{ chai };
	PluginWindow pluginWindow;
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

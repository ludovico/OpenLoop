#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>
#include "../Source/json.hpp"

#pragma warning (disable : 4100)

int nextId() {
	static int id = 2;
	return id++;
}

enum EntityType {
	AudioProcessorType, AudioFormatReaderType, InputChannels, OutputChannels
};

struct Entity {
	Entity(long id, EntityType type) : id{ id }, type{ type } {}
	~Entity() {
		if (type == EntityType::AudioFormatReaderType) {
			delete audioFormatReader;
		} else if (type == EntityType::AudioProcessorType) {
			delete audioProcessor;
		}
	}
	
	long id;
	AudioProcessor* audioProcessor;
	AudioFormatReader* audioFormatReader;

	EntityType type;
};

struct Connection {
	Entity source;
	int sourceCh;
	Entity destination;
	int destCh;
};

struct Graph {

};

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

class MainContentComponent : public AudioAppComponent {
public:
	MainContentComponent() {
		setSize(100, 100);
		setTopLeftPosition(0, 700);
		setAudioChannels(64, 64);
		
		AudioDeviceManager::AudioDeviceSetup ads;
		deviceManager.getAudioDeviceSetup(ads);
		ads.inputDeviceName = "ASIO Fireface USB";
		ads.outputDeviceName = "ASIO Fireface USB";
		ads.sampleRate = 44100;
		ads.bufferSize = 256;
		ads.useDefaultInputChannels = true;
		ads.useDefaultOutputChannels = true;

		deviceManager.closeAudioDevice();
		deviceManager.setCurrentAudioDeviceType("ASIO", true);
		deviceManager.setAudioDeviceSetup(ads, true);
		deviceManager.initialise(64, 64, nullptr, true, "", &ads);

		entities.fill(nullptr);
		entities[0] = new Entity{ 0, EntityType::InputChannels };
		entities[1] = new Entity{ 1, EntityType::OutputChannels };


		//saveDesktopPlugins();

		pluginWindow.toFront(true);
		tcpServer.startThread();
	}

	~MainContentComponent() {
		shutdownAudio();
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
		this->sampleRate = sampleRate;
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

	class TCPServer : public Thread {
	public:
		TCPServer(MainContentComponent* mcc) : Thread("tcp-server"), mcc{ mcc } {}

		void run() override {
			serverSocket.createListener(8989, "127.0.0.1");
			char* buf = new char[1024 * 1024];
			while (true) {
				auto connSocket = serverSocket.waitForNextConnection();
				int i;
				connSocket->read(&i, 4, true);
				connSocket->read(buf, i, true);
				buf[i] = 0;

				// we received a json!
				auto utf8 = CharPointer_UTF8{ buf };
				auto s = String{ utf8 }.toStdString();
				auto msg = nlohmann::json::parse(s);

				nlohmann::json reply;

				if (msg["msgid"].is_null()) {
					reply["error"] = "field \"msgid\" cannot be null";
				} else if (msg["msg"] == "samplerate") {
					reply["samplerate"] = mcc->sampleRate;
				} else if (msg["msg"] == "get-input-channels") {
					reply["id"] = 0;
				} else if (msg["msg"] == "get-output-channels") {
					reply["id"] = 1;
				} else if (msg["msg"] == "load-plugin") {
					if (msg["path"].is_string()) {
						std::string path = msg["path"];
						if (File::isAbsolutePath(String{ path })) {
							auto file = File{ String{ path } };
							if (file.exists()) {
								auto xmlelem = XmlDocument::parse(file);
								if (xmlelem != nullptr) {
									PluginDescription pd;
									if (pd.loadFromXml(*xmlelem)) {
										AudioPluginFormatManager apfm;
										apfm.addDefaultFormats();
										auto api = apfm.createPluginInstance(pd, 44100, 256, String{});
										if (api != nullptr) {
											int id = nextId();
											mcc->entities[id] = new Entity{ id, EntityType::AudioProcessorType };
											mcc->entities[id]->audioProcessor = api;
											reply["id"] = id;
										} else {
											reply["error"] = "couldn't load plugin";
										}
									} else {
										reply["error"] = "not a valid plugin description";
									}
								} else {
									reply["error"] = "not an xml";
								}
							} else {
								reply["error"] = "file does not exist";
							}
						} else {
							reply["error"] = "Not a valid path";
						}
					} else {
						reply["error"] = "\"path\" field is null or it is not a string.";
					}
					reply["msgid"] = msg["msgid"];
				} else if (msg["msg"] == "remove-plugin") {
					if (msg["id"].is_number_integer()) {
						int id = msg["id"];
						if (mcc->entities[id] != nullptr) {
							Entity* entity = mcc->entities[id];
							if (entity->type == EntityType::AudioProcessorType) {
								delete entity;
								mcc->entities[id] = nullptr;
							} else {
								reply["error"] = "This id does not refer to a plugin. Check your programming logic!";
							}
						} else {
							reply["error"] = "This id does not refer to anything! Check your programming logic!";
						}
					} else {
						reply["error"] = "\"id\" field is null or not an integer id";
					}
				} else if (msg["msg"] == "add-connection") {
					if (msg["source-id"].is_number_integer() && msg["source-ch"].is_number_integer() 
						&& msg["dest-id"].is_number_integer() && msg["dest-ch"].is_number_integer()) {
						
					} else {
						reply["error"] = "fields \"source-id\", \"source-ch\", \"dest-id\" and \"dest-ch\" must be present and contain an integer!";
					}
				}

				reply["msgid"] = msg["msgid"];
				auto replystring = reply.dump();
				auto replybuf = replystring.data();
				int replysize = strlen(replybuf);
				connSocket->write(&replysize, 4);
				connSocket->write(replybuf, replysize);
			}
		}

		StreamingSocket serverSocket;
		MainContentComponent* mcc;
	};
	
	double sampleRate;
	TCPServer tcpServer{ this };
	PluginWindow pluginWindow;
	std::array<Entity*, 1000> entities;
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

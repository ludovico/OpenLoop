#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>
#include "../Source/json.hpp"
#include <algorithm>
#include <deque>
#include <unordered_set>
#include <set>
#include <queue>

#pragma warning (disable : 4100)

using namespace std::string_literals;

constexpr int ENTITY_LIMIT = 10000;
constexpr int FILE_LIMIT = 100000;
constexpr int REALTIMEBUFFER_LIMIT = 1000;

int entityId = -1;

int nextEntityId() {
	entityId += 1;
	return entityId;
}

int audioFormatReaderId = -1;

int nextAudioFormatReaderId() {
	audioFormatReaderId += 1;
	return audioFormatReaderId;
}

MessageManager* messageManager;

class PluginWindow : public DocumentWindow {
public:
	PluginWindow(AudioProcessorEditor* editor, int x, int y)
		:DocumentWindow("", Colour::fromHSV(0.0, 0.0, 0.0, 1.0), DocumentWindow::minimiseButton) {
		if (editor != nullptr) {
			setName(editor->getName());
			setContentOwned(editor, true);
		}
		setTopLeftPosition(x, y);
		setVisible(true);
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

enum class EntityType {
	Plugin, BufferProcessor, InputProcessor, OutputProcessor, MidiInput, SoundFileProcessor
};

struct Entity {
	Entity(long id, EntityType type) : id{ id }, type{ type } {}
	~Entity() {}

	virtual void process(int64 sampleCount, int numSamples) = 0;
	
	long id;
	double orderOfComputation = 0;
	int sampleInDestBuffer = 0;
	bool removeFromPlaylist = false;
	
	AudioBuffer<double> buffer;
	MidiBuffer midibuffer;

	std::vector<MidiBuffer*> midiSources;
	std::deque<std::tuple<int64, MidiMessage>> midiQueue;

	EntityType type;
};

struct Plugin : public Entity {
	Plugin(long id, AudioProcessor* processor, double sampleRate, std::vector<int> inputs, std::vector<int> outputs) : 
		audioProcessor{ processor }, Entity{ id, EntityType::Plugin } {
		for (int i = 0; i < inputs.size(); i++) {
			auto success = audioProcessor->setChannelLayoutOfBus(true, i, AudioChannelSet::discreteChannels(inputs[i]));
			if (!success) { throw ""s + "Plugin does not support " + String{ inputs[i] }.toStdString() + " channels for input bus " + String{ i }.toStdString() + "."; }
		}
		for (int i = 0; i < outputs.size(); i++) {
			auto success = audioProcessor->setChannelLayoutOfBus(false, i, AudioChannelSet::discreteChannels(outputs[i]));
			if (!success) { throw ""s + "Plugin does not support " + String{ outputs[i] }.toStdString() + " channels for output bus " + String{ i }.toStdString() + "."; }
		}
		audioProcessor->prepareToPlay(sampleRate, 441);
	}

	~Plugin() {
		deleteEditorWindow();
		audioProcessor->releaseResources();
		delete audioProcessor;
	}

	void process(int64 sampleCount, int numSamples) override {
		midibuffer.clear();
		for (auto src : midiSources) {
			midibuffer.addEvents(*src, 0, numSamples, 0);
		}

		while (!midiQueue.empty() && std::get<0>(midiQueue.front()) < sampleCount) {
			midiQueue.pop_front();
		}

		while (!midiQueue.empty() && std::get<0>(midiQueue.front()) < sampleCount + numSamples) {
			midibuffer.addEvent(std::get<1>(midiQueue.front()), std::get<0>(midiQueue.front()) - sampleCount);
			midiQueue.pop_front();
		}

		//ScopedLock lock(audioProcessor->getCallbackLock());

		if (audioProcessor->supportsDoublePrecisionProcessing()) {
			audioProcessor->processBlock(buffer, midibuffer);
		} else {
			AudioBuffer<float> floatbuffer;
			floatbuffer.makeCopyOf(buffer);
			audioProcessor->processBlock(floatbuffer, midibuffer);
			buffer.makeCopyOf(floatbuffer);
		}
	}

	void createEditorWindow(int x, int y) {
		std::tuple<Plugin*, int, int> args = std::make_tuple(this, x, y);
		if (pluginWindow == nullptr) {
			messageManager->callFunctionOnMessageThread([](void* argsp) {
				auto args = static_cast<std::tuple<Plugin*, int, int>*>(argsp);
				Plugin* plugin = std::get<0>(*args);
				int x = std::get<1>(*args);
				int y = std::get<2>(*args);
				plugin->pluginWindow = new PluginWindow(plugin->audioProcessor->createEditorIfNeeded(), x, y);
				return argsp;
			}, &args);
		}
	}

	void deleteEditorWindow() {
		if (pluginWindow != nullptr) {
			messageManager->callFunctionOnMessageThread([](void* ent) {
				Plugin* plugin = static_cast<Plugin*>(ent);
				delete plugin->pluginWindow;
				plugin->pluginWindow = nullptr;
				return ent;
			}, this);
		}
	}

	AudioProcessor* audioProcessor;
	PluginWindow* pluginWindow = nullptr;
};

enum BufferProcessorMode { CLEAR, ADD, COPY };

struct BufferProcessor : public Entity {
	BufferProcessor(long id) :
		mode { BufferProcessorMode::CLEAR }, Entity{ id, EntityType::BufferProcessor } {
	}

	BufferProcessor(long id, BufferProcessorMode mode) :
		mode{ mode }, Entity{ id, EntityType::BufferProcessor } {}

	void process(int64 sampleCount, int numSamples) override {
		if (mode == BufferProcessorMode::CLEAR) {
			buffer.clear();
			//buffer1->clear();
		} else if (mode == BufferProcessorMode::COPY) {
			buffer.copyFrom(1, 0, buffer.getReadPointer(0), buffer.getNumSamples());
			//buffer2->copyFrom(0, 0, *buffer1, 0, 0, buffer1->getNumSamples());
			
		} else if (mode == BufferProcessorMode::ADD) {
			buffer.addFrom(1, 0, buffer.getReadPointer(0), buffer.getNumSamples());
			//buffer2->addFrom(0, 0, *buffer1, 0, 0, buffer1->getNumSamples());
		}
	}

	//AudioBuffer<double>* buffer1;
	//int buffer1_id;
	//AudioBuffer<double>* buffer2;
	//int buffer2_id;
	BufferProcessorMode mode;
};

struct SoundFileProcessor : public Entity {
	SoundFileProcessor(long id, AudioFormatReader* afr, int64 startSample, int64 length) : 
		afr{ afr }, startSample{ startSample }, Entity{ id, EntityType::SoundFileProcessor } {
		endSample = startSample + length;
		if (endSample > afr->lengthInSamples) {
			endSample = afr->lengthInSamples;
		} else if (endSample < 0) {
			endSample = 0;
		}
		if (startSample < 0) {
			startSample = 0;
		} else if (startSample > endSample) {
			startSample = endSample;
		}
		positionInFile = startSample;
	}

	void process(int64 sampleCount, int numSamples) override {
		if (firstProcessCall) {
			sampleInDestBuffer = startClipAtSample - sampleCount;
			jassert(sampleInDestBuffer < numSamples); // this holds as long as the audiocallback queue is working correctly.
			
			if (sampleInDestBuffer < 0) { // we are too late and must correct the position in file.
				positionInFile -= sampleInDestBuffer; // advance the position (- and - is +)
				sampleInDestBuffer = 0;
			}

			firstProcessCall = false;
		} else {
			sampleInDestBuffer = 0;
		}
		
		floatbuffer.setSize(afr->numChannels, buffer.getNumSamples(), false, false, true);
		floatbuffer.clear();

		int numSamplesToCopy = std::min(buffer.getNumSamples() - sampleInDestBuffer, static_cast<int>(endSample - positionInFile));
		if (numSamplesToCopy > 0) {
			afr->read(&floatbuffer, sampleInDestBuffer, numSamplesToCopy, positionInFile, true, true);
			positionInFile += numSamplesToCopy;
		}

		buffer.makeCopyOf(floatbuffer, true);
	}

	bool firstProcessCall = true;
	int startClipAtSample = 0;
	int sampleCount = 0;
	int sampleInDestBuffer = 0;
	AudioBuffer<float> floatbuffer;
	AudioFormatReader* afr;
	int64 startSample;
	int64 endSample;
	int64 positionInFile;
};

struct InputProcessor {
	InputProcessor(long id, int numChannels, int samplesPerBlockExpected, std::array<AudioBuffer<double>, REALTIMEBUFFER_LIMIT>& realtimebuffers) {
		std::vector<double*> inBufs;
		for (int i = 0; i < numChannels; i++) {
			inBufs.push_back(realtimebuffers[i].getWritePointer(0));
		}
		buffer.setDataToReferTo(inBufs.data(), inBufs.size(), samplesPerBlockExpected);
	}

	void process(const AudioSourceChannelInfo& bufferToFill) {
		for (int i = 0; i < buffer.getNumChannels(); i++) {
			for (int srcSample = bufferToFill.startSample, destSample = 0; destSample < bufferToFill.numSamples; srcSample++, destSample++) {
				buffer.setSample(i, destSample, bufferToFill.buffer->getSample(i, srcSample));
			}
		}
	}

	AudioBuffer<double> buffer;
};

struct OutputProcessor {
	OutputProcessor(long id, int numChannels, int startBuffer, int samplesPerBlockExpected, std::array<AudioBuffer<double>, REALTIMEBUFFER_LIMIT>& realtimebuffers) {
		std::vector<double*> outBufs;
		for (int i = 0; i < numChannels; i++) {
			realtimebuffers[i + startBuffer].clear();
			outBufs.push_back(realtimebuffers[i + startBuffer].getWritePointer(0));
		}
		buffer.setDataToReferTo(outBufs.data(), outBufs.size(), samplesPerBlockExpected);
	}

	void process(const AudioSourceChannelInfo& bufferToFill) {
		AudioBuffer<float> output;
		output.makeCopyOf(buffer, false);
		for (int i = 0; i < bufferToFill.buffer->getNumChannels(); i++) {
			bufferToFill.buffer->copyFrom(i, bufferToFill.startSample, output.getReadPointer(i), bufferToFill.numSamples);
		}
	}

	AudioBuffer<double> buffer;
};

struct MidiInputDevice {
	MidiInputDevice(String midiDeviceName, AudioDeviceManager& deviceManager, double sampleRate) {
		midiMessageCollector.reset(sampleRate);
		deviceManager.setMidiInputEnabled(midiDeviceName, true);
		deviceManager.addMidiInputCallback(midiDeviceName, &midiMessageCollector);
	}

	MidiMessageCollector midiMessageCollector;
	MidiBuffer midiBuffer;
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

class MainContentComponent : public Component, public AudioSource {
public:
	MainContentComponent() {
		setSize(200, 100);
		setTopLeftPosition(0, 700);

		messageManager = MessageManager::getInstance();

		deviceManager.initialise(64, 64, nullptr, true);
		
		if (getMachine() == "deskto") {
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
			String audioError = deviceManager.initialise(64, 64, nullptr, true, "", &ads);
			jassert(audioError.isEmpty());

			deviceManager.addAudioCallback(&audioSourcePlayer);
			audioSourcePlayer.setSource(this);
		} else {
			deviceManager.closeAudioDevice();
			String audioError = deviceManager.initialise(2, 2, nullptr, true);
			jassert(audioError.isEmpty());

			deviceManager.addAudioCallback(&audioSourcePlayer);
			audioSourcePlayer.setSource(this);
		}

		//saveDesktopPlugins();

		tcpServer.startThread();
	}

	~MainContentComponent() {
		audioSourcePlayer.setSource(nullptr);
		deviceManager.removeAudioCallback(&audioSourcePlayer);
		for (int i = 2; i < ENTITY_LIMIT; i++) {
			if (entities[i] != nullptr) {
				delete entities[i];
			}
		}
		deviceManager.closeAudioDevice();
		tcpServer.signalThreadShouldExit();
		tcpServer.serverSocket.close();
		tcpServer.stopThread(50);
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
		this->sampleRate = sampleRate;
		this->samplesPerBlockExpected = samplesPerBlockExpected;

		auto device = deviceManager.getCurrentAudioDevice();
		std::cout << "Device name: " << device->getName() << "\n";
		std::cout << "Device type: " << device->getTypeName() << "\n";
		std::cout << "Current sampleRate: " << device->getCurrentSampleRate() << "\n";
		std::cout << "Current bit depth: " << device->getCurrentBitDepth() << "\n";
		std::cout << "Expected samples per block: " << device->getCurrentBufferSizeSamples() << "\n";
		std::cout << "Input latency in samples: " << device->getInputLatencyInSamples() << "\n";
		std::cout << "Output latency in samples: " << device->getOutputLatencyInSamples() << "\n";
		std::cout << "Active input channels: " << device->getActiveInputChannels().toString(2) << "\n";
		std::cout << "Active output channels: " << device->getActiveOutputChannels().toString(2) << "\n";
		std::cout << device->getLastError() << "\n";

		if (initialized) {
			return;
		}

		for (auto& buffer : realtimebuffers) {
			buffer.setSize(1, samplesPerBlockExpected, false, false, false);
		}

		entities.fill(nullptr);
		audioFormatReaders.fill(nullptr);

		audioInterfaceInput = new InputProcessor{ 0, device->getActiveInputChannels().countNumberOfSetBits(), samplesPerBlockExpected, realtimebuffers };
		audioInterfaceOutput = new OutputProcessor{ 1, device->getActiveOutputChannels().countNumberOfSetBits(),audioInterfaceInput->buffer.getNumChannels(), samplesPerBlockExpected, realtimebuffers };

		auto midiDeviceNames = MidiInput::getDevices();
		for (int i = 0; i < midiDeviceNames.size(); i++) {
			midiInputDevices.push_back(new MidiInputDevice(midiDeviceNames[i], deviceManager, sampleRate));
			std::cout << "midi device at index " << i << ": " << midiDeviceNames[i] << "\n";
		}

		initialized = true;
	}

	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
		jassert(bufferToFill.numSamples == samplesPerBlockExpected);
		
		// fill input buffers - placed in realtimebuffers[0..17] on my interface
		audioInterfaceInput->process(bufferToFill);

		// a safety measure - to ensure that these "special" buffers doesn't continue to sound when
		// ending a calculation.
		// buffer.clear() has a ghostly behaviour, causing the buffers to not sound AT ALL
		// but this is not a particularly costly operation compared to normal DSP processing.
		audioInterfaceOutput->buffer.applyGain(0);
		
		// query the audio callback function queue - these functions set up the connections and directs what calculations to perform
		while (!audioCallbackFunctionQueue.empty() && std::get<0>(audioCallbackFunctionQueue.top()) < sampleCount) {
			auto func = std::get<1>(audioCallbackFunctionQueue.top());
			func(std::get<0>(audioCallbackFunctionQueue.top()) - sampleCount);
			audioCallbackFunctionQueue.pop();
		}

		while (!audioCallbackFunctionQueue.empty() && std::get<0>(audioCallbackFunctionQueue.top()) < sampleCount + bufferToFill.numSamples) {
			auto func = std::get<1>(audioCallbackFunctionQueue.top());
			func(std::get<0>(audioCallbackFunctionQueue.top()) - sampleCount);
			audioCallbackFunctionQueue.pop();
		}

		// collect midi input from each midi input device
		for (auto& midi : midiInputDevices) {
			midi->midiBuffer.clear();
			midi->midiMessageCollector.removeNextBlockOfMessages(midi->midiBuffer, bufferToFill.numSamples);
		}

		// we loop through the playlist, which is always sorted in order of computation (inputs before outputs)
		// and calculate each entity by calling process.
		for (auto entity : playlist) {
			entity->process(sampleCount, bufferToFill.numSamples);
			if (entity->removeFromPlaylist) {
				audioCallbackFunctionQueue.push(std::make_tuple(0, [=](int i) {
					playlist.erase(entity);
				}));
			}
		}

		// grab output buffers - they are in realtimebuffers[18..35] on my interface
		audioInterfaceOutput->process(bufferToFill);

		sampleCount += bufferToFill.numSamples;
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
		TCPServer(MainContentComponent* mcc) : Thread("tcp-server"), mcc{ mcc } {
			messageManager = MessageManager::getInstance();
		}

		////////////////////////
		////////////////////////
		//// ERROR HANDLING ////
		////////////////////////
		////////////////////////

		void checkValidField(nlohmann::json& msg, const std::string& field) {
			if (msg[field].is_null()) {
				throw "field \"" + field + "\" must be present.";
			}
		}

		int64 getIntegerInField(nlohmann::json& msg, const std::string& field) {
			checkValidField(msg, field);
			if (!msg[field].is_number_integer()) {
				throw "field \"" + field + "\" must be an integer.";
			}
			return msg[field];
		}

		double getFloatInField(nlohmann::json& msg, const std::string& field) {
			checkValidField(msg, field);
			if (!msg[field].is_number_float()) {
				throw "field \"" + field + "\" must be a floating point number.";
			}
			return msg[field];
		}

		std::string getStringInField(nlohmann::json& msg, const std::string& field) {
			checkValidField(msg, field);
			if (!msg[field].is_string()) {
				throw "field \"" + field + "\" must be a string.";
			}
			return msg[field];
		}

		int getValidEntityId(nlohmann::json& msg, const std::string& field) {
			int id = getIntegerInField(msg, field);
			if (id < 0 && id >= ENTITY_LIMIT) {
				throw "" + field + " is not within entity limits.";
			}
			return id;
		}

		int getValidAudioFormatReaderId(nlohmann::json& msg, const std::string& field) {
			int id = getIntegerInField(msg, field);
			if (id < 0 && id >= FILE_LIMIT) {
				throw "" + field + " is not within file limits.";
			}
		}

		int getValidRealtimeBufferId(nlohmann::json& msg, const std::string& field) {
			int id = getIntegerInField(msg, field);
			if (id < 0 && id >= REALTIMEBUFFER_LIMIT) {
				throw "" + field + " is not within realtime buffer limits.";
			}
		}

		AudioFormatReader* getValidAudioFormatReader(nlohmann::json& msg, const std::string& field, std::array<AudioFormatReader*, FILE_LIMIT>& audioFormatReaders) {
			AudioFormatReader* afr = audioFormatReaders[getValidAudioFormatReaderId(msg, field)];
			if (afr == nullptr) {
				throw "" + field + " is null";
			}
			return afr;
		}

		Entity* getValidEntity(nlohmann::json& msg, const std::string& field, std::array<Entity*, ENTITY_LIMIT>& entities) {
			Entity* entity = entities[getValidEntityId(msg, field)];
			if (entity == nullptr) {
				throw "" + field + " is null";
			}
			return entity;
		}

		File getValidFile(nlohmann::json& msg, const std::string& field) {
			std::string path{ getStringInField(msg, field) };
			String strpath{ path };
			if (!File::isAbsolutePath(strpath)) {
				throw path + " at " + field + " is not an absolute path.";
			}
			File file{ strpath };
			if (!file.exists()) {
				throw path + " does not exist.";
			} else if (file.isDirectory()) {
				throw path + " is a directory.";
			}
			return file;
		}

		AudioPluginInstance* createNewPlugin(nlohmann::json& msg, const std::string& field) {
			auto xmlelem = XmlDocument::parse(getValidFile(msg, field));
			if (xmlelem == nullptr) {
				throw "File is not a valid xml."s;
			}
			PluginDescription pd;
			if (!pd.loadFromXml(*xmlelem)) {
				throw "File is not a valid plugin description."s;
			}
			AudioPluginFormatManager apfm;
			apfm.addDefaultFormats();
			String error;
			auto api = apfm.createPluginInstance(pd, 44100, 256, error);
			if (api == nullptr) {
				throw "Couldn't load plugin. " + error.toStdString();
			}
			return api;
		}

		Plugin* getPlugin(nlohmann::json& msg, const std::string& field, std::array<Entity*, ENTITY_LIMIT>& entities) {
			Entity* plugin = getValidEntity(msg, field, entities);
			if (plugin->type != EntityType::Plugin) {
				throw "The id does not refer to a plugin."s;
			}
			return static_cast<Plugin*>(plugin);
		}

		// ------------------------------------------------------------------------- //
		// ------------------------------------------------------------------------- //

		void run() override {
			serverSocket.createListener(8989, "127.0.0.1");
			char* buf = new char[1024 * 1024];
			while (!threadShouldExit()) {
				auto connSocket = serverSocket.waitForNextConnection();

				if (connSocket == nullptr) {
					continue;
				}
				
				int i;
				connSocket->read(&i, 4, true);
				connSocket->read(buf, i, true);
				buf[i] = 0;

				auto utf8 = CharPointer_UTF8{ buf };
				auto s = String{ utf8 }.toStdString();
				auto msg = nlohmann::json::parse(s);

				nlohmann::json reply;
				try {
					auto message = getStringInField(msg, "msg");

					checkValidField(msg, "msgid");
					reply["msgid"] = msg["msgid"];
					
					if (message == "samplerate") {
						reply["samplerate"] = mcc->sampleRate;
					} else if (message == "get-input-channels") {
						reply["id"] = 0;
					} else if (message == "get-output-channels") {
						reply["id"] = 1;
					} else if (message == "load-audio-file") {
						// Switch to an AudioFormatManager for easy access to aiff, mp3, wma, flac and ogg.
						WavAudioFormat waf;
						AudioFormatReader* afr = waf.createReaderFor(new FileInputStream{ getValidFile(msg, "path") }, true);
						if (afr == nullptr) {
							throw "The file is not a wav file"s;
						}
						auto id = nextAudioFormatReaderId();
						mcc->audioFormatReaders[id] = afr;
						reply["id"] = id;
					} else if (message == "load-plugin") {
						std::vector<int> in;
						std::vector<int> out;
						if (msg["inputs"].is_array()) {
							auto inputs = msg["inputs"];
							for (auto input : inputs) {
								if (input.is_number_integer()) {
									in.push_back(input);
								} else {
									throw "Input array must consist of integers."s;
								}
							}
						}
						if (msg["outputs"].is_array()) {
							auto outputs = msg["outputs"];
							for (auto output : outputs) {
								if (output.is_number_integer()) {
									out.push_back(output);
								} else {
									throw "Output array must consist of integers."s;
								}
							}
						}
						int id = nextEntityId();
						auto plugin = new Plugin{ id, createNewPlugin(msg, "path"), mcc->sampleRate, in, out };
						mcc->entities[id] = plugin;
						reply["id"] = id;
					} else if (message == "load-clip") {
						auto afr = getValidAudioFormatReader(msg, "file-id", mcc->audioFormatReaders);
						auto fromSample = msg["from-sample"].is_null() ? 0 : getIntegerInField(msg, "from-sample");
						auto length = msg["length"].is_null() ? LONG_MAX : getIntegerInField(msg, "length");
						int id = nextEntityId();
						mcc->entities[id] = new SoundFileProcessor(id, afr, fromSample, length);
						reply["id"] = id;
					} else if (message == "load-buffer-processor") {
						auto type = getStringInField(msg, "type");
						if (!msg["clear"].is_null()) {
							//auto bufferId = getValidRealtimeBufferId(msg, "clear");
							int id = nextEntityId();
							//mcc->entities[id] = new BufferProcessor{ id, &mcc->realtimebuffers[bufferId], bufferId };
							mcc->entities[id] = new BufferProcessor{ id };
							reply["id"] = id;
						} else if (type == "add") {
							//auto bufferId1 = getValidRealtimeBufferId(msg, "from");
							//auto bufferId2 = getValidRealtimeBufferId(msg, "to");
							int id = nextEntityId();
							//mcc->entities[id] = new BufferProcessor{ id, &mcc->realtimebuffers[bufferId1], bufferId1, &mcc->realtimebuffers[bufferId2], bufferId2, BufferProcessorMode::ADD };
							mcc->entities[id] = new BufferProcessor{ id, BufferProcessorMode::ADD };
							reply["id"] = id;
						} else if (type == "copy") {
							//auto bufferId1 = getValidRealtimeBufferId(msg, "from");
							//auto bufferId2 = getValidRealtimeBufferId(msg, "to");
							int id = nextEntityId();
							//mcc->entities[id] = new BufferProcessor{ id, &mcc->realtimebuffers[bufferId1], bufferId1, &mcc->realtimebuffers[bufferId2], bufferId2, BufferProcessorMode::COPY };
							mcc->entities[id] = new BufferProcessor{ id, BufferProcessorMode::COPY };
							reply["id"] = id;
						} else {
							throw "type must be either clear, add or copy."s;
						}
					} else if (message == "remove-entity") {
						auto entity = getValidEntity(msg, "id", mcc->entities);
						auto atSample = msg["at-sample"].is_null() ? 0 : getIntegerInField(msg, "at-sample");
						mcc->audioCallbackFunctionQueue.push(std::make_tuple(atSample, [=](int sample) {
							if (entity != nullptr) {
								auto iter = mcc->playlist.find(entity);
								if (iter != mcc->playlist.end()) {
									mcc->playlist.erase(entity);
								}
								delete entity;
								mcc->entities[msg["id"]] = nullptr;
							}
						}));
											
					} else if (message == "calculate") {
						auto entity = getValidEntity(msg, "id", mcc->entities);
						auto order = getFloatInField(msg, "order");
						auto startSample = msg["start-sample"].is_null() ? 0 : getIntegerInField(msg, "start-sample");
						std::vector<double*> buffersToUse;
						if (msg["buffers-to-use"].is_array()) {
							auto buffers = msg["buffers-to-use"];
							if (buffers.size() != 0) {
								for (int i = 0; i < buffers.size(); i++) {
									if (buffers[i].is_number_integer()) {
										int bufId = msg["buffers-to-use"][i];
										buffersToUse.push_back(mcc->realtimebuffers[bufId].getWritePointer(0));
									} else {
										throw "field \"buffers-to-use\" must be an array of integers."s;
									}
								}
							} else {
								throw "buffers-to-use cannot be an empty array."s;
							}
						} else {
							throw "field \"buffers-to-use\" is null or not an array."s;
						}
						mcc->audioCallbackFunctionQueue.push(std::make_tuple(startSample, [=](int sample) mutable {
							// since we schedule a calculation, the given entity might be removed and nulled before we
							// get there.
							if (entity != nullptr) {
								entity->orderOfComputation = order;
								entity->buffer.setDataToReferTo(buffersToUse.data(), buffersToUse.size(), mcc->samplesPerBlockExpected);
								mcc->playlist.insert(entity);
								if (entity->type == EntityType::SoundFileProcessor) {
									SoundFileProcessor* sfp = static_cast<SoundFileProcessor*>(entity);
									sfp->startClipAtSample = startSample != 0 ? startSample : mcc->sampleCount;
									sfp->sampleCount = mcc->sampleCount;
								}
							}
						}));
						if (msg["end-sample"].is_number_integer()) {
							mcc->audioCallbackFunctionQueue.push(std::make_tuple(getIntegerInField(msg, "end-sample"), [=](int sample) {
								if (entity != nullptr) {
									mcc->playlist.erase(entity);
								}
							}));
						}
					} else if (message == "remove-calculation") {
						auto entity = getValidEntity(msg, "id", mcc->entities);
						auto atSample = msg["at-sample"].is_null() ? 0 : getIntegerInField(msg, "at-sample");
						auto found = mcc->playlist.find(entity);
						if (found != mcc->playlist.end()) {
							mcc->audioCallbackFunctionQueue.push(std::make_tuple(atSample, [=](int sample) {
								if (entity != nullptr) {
									mcc->playlist.erase(entity);
								}
							}));
						} else {
							msg["reply"] = "The entity is not running."s;
						}	
					} else if (message == "add-midi-connection") {
						MidiBuffer* midibuffer;
						if (msg["source-id"].is_number_integer()) {
							midibuffer = &(getPlugin(msg, "source-id", mcc->entities)->midibuffer);
						} else if (msg["source-id"].is_string()) {
							auto str = getStringInField(msg, "source-id");
							if (str == "m00") {
								midibuffer = &(mcc->midiInputDevices[0]->midiBuffer);
							} else if (str == "m01") {
								midibuffer = &(mcc->midiInputDevices[1]->midiBuffer);
							} else if (str == "m02") {
								midibuffer = &(mcc->midiInputDevices[2]->midiBuffer);
							} else if (str == "m03") {
								midibuffer = &(mcc->midiInputDevices[3]->midiBuffer);
							} else if (str == "m04") {
								midibuffer = &(mcc->midiInputDevices[4]->midiBuffer);
							} else if (str == "m05") {
								midibuffer = &(mcc->midiInputDevices[5]->midiBuffer);
							} else if (str == "m06") {
								midibuffer = &(mcc->midiInputDevices[6]->midiBuffer);
							} else {
								throw "valid midi inputs are m00-m06.";
							}
						} else {
							throw "Field \"source-id\" must be present and be either an integer or a string."s;
						}
						getPlugin(msg, "dest-id", mcc->entities)->midiSources.push_back(midibuffer);
					} else if (message == "plugin-queue-midi") {
						auto entity = getPlugin(msg, "id", mcc->entities);
						if (msg["midi"].is_array()) {
							auto midivec = msg["midi"];
							for (int i = 0; i < midivec.size(); i++) {
								if (midivec[i].is_object()) {
									auto obj = midivec[i];
									auto sample = getIntegerInField(obj, "sample");
									auto type = getStringInField(obj, "type");
									if (type == "on") {
										entity->midiQueue.push_back(std::make_tuple(sample, MidiMessage::noteOn(getIntegerInField(obj, "ch"), getIntegerInField(obj, "note"), static_cast<uint8>(getIntegerInField(obj, "vel")))));
									} else if (type == "off") {
										entity->midiQueue.push_back(std::make_tuple(sample, MidiMessage::noteOff(getIntegerInField(obj, "ch"), getIntegerInField(obj, "note"), static_cast<uint8>(getIntegerInField(obj, "vel")))));
									} else if (type == "cc") {
										entity->midiQueue.push_back(std::make_tuple(sample, MidiMessage::controllerEvent(getIntegerInField(obj, "ch"), getIntegerInField(obj, "ccnum"), static_cast<uint8>(getIntegerInField(obj, "val")))));
									} else {
										throw "midi type not understood."s;
									}
								} else {
									throw "array should contain objects"s;
								}
							}
						} else {
							throw "field midi does not exist or is not an array"s;
						}
					} else if (message == "plugin-clear-queue-midi") {
						auto entity = getPlugin(msg, "id", mcc->entities);
						entity->midiQueue.clear();
						entity->audioProcessor->reset();
					} else if (message == "plugin-open-editor") {
						// consider a mechanism for default values for non-mandatory fields
						int x, y;
						if (!msg["x"].is_number_integer() || !msg["y"].is_number_integer()) {
							x = 10; y = 10;
						} else {
							x = msg["x"]; y = msg["y"];
						}
						Plugin* plugin = getPlugin(msg, "id", mcc->entities);
						plugin->createEditorWindow(x, y);
					} else if (message == "plugin-close-editor") {
						Plugin* plugin = getPlugin(msg, "id", mcc->entities);
						plugin->deleteEditorWindow();
					} else if (message == "get-sample-time") {
						reply["sample"] = mcc->sampleCount;
					} else if (message == "get-entity-info") {
						reply["entity-info"] = nlohmann::json::array();
						for (auto entity : mcc->entities) {
							if (entity != nullptr) {
								auto entity_info = nlohmann::json::object();
								entity_info["id"] = entity->id;
								if (entity->type == EntityType::Plugin) {
									entity_info["type"] = "plugin";
									Plugin* plugin = static_cast<Plugin*>(entity);
									entity_info["name"] = plugin->audioProcessor->getName().toStdString();
									entity_info["latency"] = plugin->audioProcessor->getLatencySamples();
								} else if (entity->type == EntityType::BufferProcessor) {
									entity_info["type"] = "buffer-processor";
									BufferProcessor* bufferProcessor = static_cast<BufferProcessor*>(entity);
									if (bufferProcessor->mode == BufferProcessorMode::CLEAR) {
										entity_info["mode"] = "clear";
										//entity_info["buffer-id"] = bufferProcessor->buffer1_id;
									} else if (bufferProcessor->mode == BufferProcessorMode::ADD) {
										entity_info["mode"] = "add";
										//entity_info["from-buffer-id"] == bufferProcessor->buffer1_id;
										//entity_info["to-buffer-id"] == bufferProcessor->buffer2_id;
									} else if (bufferProcessor->mode == BufferProcessorMode::COPY) {
										entity_info["mode"] = "copy";
										//entity_info["from-buffer-id"] == bufferProcessor->buffer1_id;
										//entity_info["to-buffer-id"] == bufferProcessor->buffer2_id;
									}
								} else if (entity->type == EntityType::SoundFileProcessor) {
									entity_info["type"] = "sound-file-processor";
								}
								reply["entity-info"].emplace_back(entity_info);
							}
						}
					} else {
						throw "message not understood."s;
					}
				} catch (std::string error) {
					reply["error"] = error;
				}
				
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
	
	AudioDeviceManager deviceManager;
	AudioSourcePlayer audioSourcePlayer;
	InputProcessor* audioInterfaceInput;
	OutputProcessor* audioInterfaceOutput;
	bool initialized = false;
	int64 sampleCount = 0;
	double sampleRate;
	int samplesPerBlockExpected;
	TCPServer tcpServer{ this };
	std::vector<MidiInputDevice*> midiInputDevices;
	std::array<Entity*, ENTITY_LIMIT> entities;
	std::array<AudioFormatReader*, FILE_LIMIT> audioFormatReaders;
	std::array<AudioBuffer<double>, REALTIMEBUFFER_LIMIT> realtimebuffers;

	std::set<Entity*, std::function<bool(Entity*, Entity*)>> playlist{
		[](Entity* a, Entity* b) { 
			if (a->orderOfComputation != b->orderOfComputation) {
				return a->orderOfComputation < b->orderOfComputation; 
			} else {
				return a < b;
			}
		}
	};

	std::priority_queue<std::tuple<int64, std::function<void(int)>>, std::vector<std::tuple<int64, std::function<void(int)>>>, std::function<bool(std::tuple<int64, std::function<void(int)>>, std::tuple<int64, std::function<void(int)>>)>> audioCallbackFunctionQueue{
		[](std::tuple<int64, std::function<void(int)>> a, std::tuple<int64, std::function<void(int)>> b) {
			return std::get<0>(a) > std::get<0>(b);
		}
	};

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }
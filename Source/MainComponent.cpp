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

#pragma warning (disable : 4100)

using namespace std::string_literals;

constexpr int ENTITY_LIMIT = 10000;
constexpr int CONN_LIMIT = 100000;
constexpr int FILE_LIMIT = 100000;
constexpr int REALTIMEBUFFER_LIMIT = 1000;

int entityId = 1;

int nextEntityId() {
	entityId += 1;
	return entityId;
}

int connId = -1;

int nextConnId() {
	connId += 1;
	return connId;
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

enum BufferProcessorMode { CLEAR, ADD, COPY };

struct BufferProcessor {
	BufferProcessor(AudioBuffer<double>* bufferToClear) : buffer1{ bufferToClear }, mode{ BufferProcessorMode::CLEAR } {}

	BufferProcessor(AudioBuffer<double>* fromBuffer, AudioBuffer<double>* toBuffer, BufferProcessorMode mode) : buffer1{ fromBuffer }, buffer2{ toBuffer }, mode{ mode } {}

	void process() {
		if (mode == BufferProcessorMode::CLEAR) {
			buffer1->clear();
		} else if (mode == BufferProcessorMode::COPY) {
			buffer2->copyFrom(0, 0, *buffer1, 0, 0, buffer1->getNumSamples());
		} else if (mode == BufferProcessorMode::ADD) {
			buffer2->addFrom(0, 0, *buffer1, 0, 0, buffer1->getNumSamples());
		}
	}

	AudioBuffer<double>* buffer1;
	AudioBuffer<double>* buffer2;
	BufferProcessorMode mode;
};

struct SoundFileProcessor  {
	SoundFileProcessor(int sampleInCallback, AudioFormatReader* afr, int64 startSample, int64 length) : sampleInDestBuffer{ sampleInCallback }, afr{ afr }, startSample{ startSample } {
		positionInFile = startSample;
		endSample = startSample + length;
		if (endSample > afr->lengthInSamples) {
			endSample = afr->lengthInSamples;
		}
	}

	void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
		floatbuffer.setSize(afr->numChannels, buffer.getNumSamples(), false, false, true);
		floatbuffer.clear();
		int numSamplesToCopy = std::min(buffer.getNumSamples() - sampleInDestBuffer, static_cast<int>(endSample - positionInFile));
		afr->read(&floatbuffer, sampleInDestBuffer, numSamplesToCopy, positionInFile, true, true);
		buffer.makeCopyOf(floatbuffer, true);
		sampleInDestBuffer = 0;
		if (positionInFile + numSamplesToCopy >= endSample) {
			done = true;
		} else {
			positionInFile += numSamplesToCopy;
		}
	}

	bool done = false;
	AudioBuffer<float> floatbuffer;
	AudioFormatReader* afr;
	int sampleInDestBuffer;
	int64 startSample;
	int64 endSample;
	int64 positionInFile;
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


enum EntityType {
	AudioProcessorType, BufferProcessorType, InputChannels, OutputChannels, MidiInputType, SoundFileProcessorType
};

struct Entity;

struct Connection {
	Connection(int id, Entity* src, int srcCh, Entity* dest, int destCh) : id{ id }, source { src }, sourceCh{ srcCh }, destination{ dest }, destCh{ destCh } {}
	int id;
	Entity* source;
	int sourceCh;
	Entity* destination;
	int destCh;
	int64 startSample = 0;
	int64 endSample = 4611686018427387904; // 2**62. 3 million years in 44k. Should be enough
};

struct Entity {
	Entity(long id, EntityType type) : id{ id }, type{ type } {}
	Entity(long id, AudioProcessor* processor, double sampleRate) : id{ id }, audioProcessor{ processor } {
		type = EntityType::AudioProcessorType;
		auto inputBuses = audioProcessor->getBusesLayout().inputBuses;
		auto outputBuses = audioProcessor->getBusesLayout().outputBuses;
		for (auto bus : inputBuses) {
			numInputChannels += bus.size();
		}
		for (auto bus : outputBuses) {
			numOutputChannels += bus.size();
		}

		audioProcessor->prepareToPlay(sampleRate, 441);
	}
	
	Entity(long id, SoundFileProcessor* soundFileProcessor) : id{ id }, soundFileProcessor{ soundFileProcessor } {
		orderOfComputation = DBL_MIN;
	}

	Entity(long id, BufferProcessor* bufferProcessor, double order) :id{ id }, bufferProcessor{ bufferProcessor }, orderOfComputation{ order } {}
	
	~Entity() {
		if (type == EntityType::AudioProcessorType) {
			for (auto conn : audioSources) {
				conn->source->audioDestinations.erase(conn);
				// here we must delete conn from connections, but we haven't access. Restructure is imminent
				delete conn;
			}
			for (auto conn : audioDestinations) {
				conn->destination->audioSources.erase(conn);
				// here we must delete conn from connections, but we haven't access. Restructure is imminent
				delete conn;
			}
			deleteEditorWindow();
			audioProcessor->releaseResources();
			delete audioProcessor;
		}
	}

	void createEditorWindow(int x, int y) {
		std::tuple<Entity*, int, int> args = std::make_tuple(this, x, y);
		if (pluginWindow == nullptr) {
			messageManager->callFunctionOnMessageThread([](void* argsp) {
				auto args = static_cast<std::tuple<Entity*, int, int>*>(argsp);
				Entity* entity = std::get<0>(*args);
				int x = std::get<1>(*args);
				int y = std::get<2>(*args);
				entity->pluginWindow = new PluginWindow(entity->audioProcessor->createEditorIfNeeded(), x, y);
				return argsp;
			}, &args);
		}
	}

	void deleteEditorWindow() {
		if (pluginWindow != nullptr) {
			messageManager->callFunctionOnMessageThread([](void* ent) {
				Entity* entity = static_cast<Entity*>(ent);
				delete entity->pluginWindow;
				entity->pluginWindow = nullptr;
				return ent;
			}, this);
		}
	}

	void process(int64 sampleCount, int numSamples) {
		//buffer.setSize(std::max(numInputChannels, numOutputChannels), numSamples, false, false, true);
		//buffer.clear();
		
		if (type == EntityType::AudioProcessorType) {
			for (auto& connection : audioSources) {
				buffer.addFrom(connection->destCh, 0, connection->source->buffer.getReadPointer(connection->sourceCh), numSamples);
			}

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
		} else if (type == EntityType::SoundFileProcessorType) {
			soundFileProcessor->processBlock(buffer, midibuffer);
			if (soundFileProcessor->done) {
				removeFromPlaylist = true;
			}
		} else if (type == BufferProcessorType) {
			bufferProcessor->process();
		}
	}
	
	long id;
	double orderOfComputation = 0;
	bool removeFromPlaylist = false;
	//std::vector<double*> buffersToReferTo;
	
	AudioProcessor* audioProcessor = nullptr;
	SoundFileProcessor* soundFileProcessor = nullptr;
	BufferProcessor* bufferProcessor = nullptr;
	
	int numInputChannels = 0;
	int numOutputChannels = 0;
	AudioBuffer<double> buffer;
	MidiBuffer midibuffer;
	PluginWindow* pluginWindow = nullptr;
	//AudioFormatReader* audioFormatReader = nullptr;
	//MainContentComponent* mcc;
	std::unordered_set<Connection*> audioSources;
	std::unordered_set<Connection*> audioDestinations;
	std::vector<MidiBuffer*> midiSources;
	std::deque<std::tuple<int64, MidiMessage>> midiQueue;

	EntityType type;
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

typedef bool(*AudioCallbackFunctionQueueCompare)(std::tuple<int64, std::function<void(int)>>, std::tuple<int64, std::function<void(int)>>);

class MainContentComponent : public Component, public AudioSource {
public:
	MainContentComponent() {
		setSize(200, 100);
		setTopLeftPosition(0, 700);

		

		//AudioBuffer<double> buf1;
		//AudioBuffer<double> buf2;
		//AudioBuffer<double> buf3;
		//AudioBuffer<double> buf4;
		//double* bufs[2] = { buf1.getWritePointer(0), buf2.getWritePointer(0) };
		//std::vector<double*> bufs;
		//bufs.push_back(buf1.getWritePointer(0));
		//bufs.push_back(buf2.getWritePointer(0));

		//AudioBuffer<double> aggregateBuffer;
		//aggregateBuffer.setDataToReferTo(bufs.data(), 2, 256);

		messageManager = MessageManager::getInstance();

		deviceManager.initialise(64, 64, nullptr, true);
		
		if (getMachine() == "desktop") {
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
		connections.fill(nullptr);
		audioFormatReaders.fill(nullptr);

		AudioDeviceManager::AudioDeviceSetup ads;
		deviceManager.getAudioDeviceSetup(ads);
		entities[0] = new Entity{ 0, EntityType::InputChannels };
		entities[0]->numInputChannels = 0;
		entities[0]->numOutputChannels = device->getActiveInputChannels().countNumberOfSetBits();
		entities[0]->orderOfComputation = DBL_MIN;
		
		
		std::vector<double*> inBufs;
		for (int i = 0; i < device->getActiveInputChannels().countNumberOfSetBits(); i++) {
			inBufs.push_back(realtimebuffers[i].getWritePointer(0));
		}
		entities[0]->buffer.setDataToReferTo(inBufs.data(), inBufs.size(), samplesPerBlockExpected);

		entities[1] = new Entity{ 1, EntityType::OutputChannels };
		entities[1]->numInputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
		entities[1]->numOutputChannels = 0;
		entities[1]->orderOfComputation = DBL_MAX;

		std::vector<double*> outBufs;
		for (int i = 0; i < device->getActiveOutputChannels().countNumberOfSetBits(); i++) {
			realtimebuffers[i + inBufs.size()].clear();
			outBufs.push_back(realtimebuffers[i + inBufs.size()].getWritePointer(0));
		}
		entities[1]->buffer.setDataToReferTo(outBufs.data(), outBufs.size(), samplesPerBlockExpected);

		auto midiDeviceNames = MidiInput::getDevices();
		for (int i = 0; i < midiDeviceNames.size(); i++) {
			midiInputDevices.push_back(new MidiInputDevice(midiDeviceNames[i], deviceManager, sampleRate));
			std::cout << "midi device at index " << i << ": " << midiDeviceNames[i] << "\n";
		}

		initialized = true;
	}

	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
		jassert(bufferToFill.numSamples == samplesPerBlockExpected);
		// query the audio callback function queue - these functions set up the connections and directs what calculations to perform
		while (!audioCallbackFunctionQueue.empty() && std::get<0>(*audioCallbackFunctionQueue.begin()) < sampleCount) {
			auto func = std::get<1>(*audioCallbackFunctionQueue.begin());
			func(std::get<0>(*audioCallbackFunctionQueue.begin()) - sampleCount);
			audioCallbackFunctionQueue.erase(*audioCallbackFunctionQueue.begin());
		}

		while (!audioCallbackFunctionQueue.empty() && std::get<0>(*audioCallbackFunctionQueue.begin()) < sampleCount + bufferToFill.numSamples) {
			auto func = std::get<1>(*audioCallbackFunctionQueue.begin());
			func(std::get<0>(*audioCallbackFunctionQueue.begin()) - sampleCount);
			audioCallbackFunctionQueue.erase(*audioCallbackFunctionQueue.begin());
		}

		// receive midi input
		for (auto& midi : midiInputDevices) {
			midi->midiBuffer.clear();
			midi->midiMessageCollector.removeNextBlockOfMessages(midi->midiBuffer, bufferToFill.numSamples);
		}

		// entities[0] represents audio interface input. Fill those buffers!
		entities[0]->buffer.setSize(entities[0]->numOutputChannels, bufferToFill.numSamples, false, false, true);
		entities[0]->buffer.clear();
		for (int i = 0; i < entities[0]->numOutputChannels; i++) {
			for (int srcSample = bufferToFill.startSample, destSample = 0; destSample < bufferToFill.numSamples; srcSample++, destSample++) {
				entities[0]->buffer.setSample(i, destSample, bufferToFill.buffer->getSample(i, srcSample));
			}
		}

		// we loop through the playlist, which is always sorted in order of computation (inputs before outputs)
		// and calculate each entity by calling process.
		for (auto entity : playlist) {
			entity->process(sampleCount, bufferToFill.numSamples);
			if (entity->removeFromPlaylist) {
				audioCallbackFunctionQueue.insert(std::make_tuple(0, [=](int i) {
					playlist.erase(entity);
				}));
			}
		}

		// entities[1] represent audio interface output.
		AudioBuffer<float> output;
		output.makeCopyOf(entities[1]->buffer, false);
		for (int i = 0; i < bufferToFill.buffer->getNumChannels(); i++) {
			bufferToFill.buffer->copyFrom(i, bufferToFill.startSample, output.getReadPointer(i), bufferToFill.numSamples);
		}

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

		Entity* getPlugin(nlohmann::json& msg, const std::string& field, std::array<Entity*, ENTITY_LIMIT>& entities) {
			Entity* plugin = getValidEntity(msg, field, entities);
			if (plugin->type != EntityType::AudioProcessorType) {
				throw "The id does not refer to a plugin."s;
			}
			return plugin;
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
					} else if (message == "load-plugin") {
						//double order = getFloatInField(msg, "order");
						int id = nextEntityId();
						mcc->entities[id] = new Entity{ id, createNewPlugin(msg, "path"), mcc->sampleRate };
						reply["id"] = id;
					} else if (message == "load-buffer-processor") {
						auto order = getFloatInField(msg, "order");
						auto type = getStringInField(msg, "type");

						if (!msg["clear"].is_null()) {
							auto bufferId = getValidRealtimeBufferId(msg, "clear");
							int id = nextEntityId();
							mcc->entities[id] = new Entity{ id, new BufferProcessor(&mcc->realtimebuffers[bufferId]), order };
							reply["id"] = id;
						} else if (type == "add") {
							auto bufferId1 = getValidRealtimeBufferId(msg, "from");
							auto bufferId2 = getValidRealtimeBufferId(msg, "add-to");
							int id = nextEntityId();
							mcc->entities[id] = new Entity{ id, new BufferProcessor(&mcc->realtimebuffers[bufferId1], &mcc->realtimebuffers[bufferId2], BufferProcessorMode::ADD), order };
							reply["id"] = id;
						} else if (type == "copy") {
							auto bufferId1 = getValidRealtimeBufferId(msg, "from");
							auto bufferId2 = getValidRealtimeBufferId(msg, "copy-to");
							int id = nextEntityId();
							mcc->entities[id] = new Entity{ id, new BufferProcessor(&mcc->realtimebuffers[bufferId1], &mcc->realtimebuffers[bufferId2], BufferProcessorMode::COPY), order };
							reply["id"] = id;
						} else {
							throw "type must be either clear, add or copy."s;
						}
					} else if (message == "remove-entity") {
						auto entity = getValidEntity(msg, "id", mcc->entities);
						mcc->playlist.erase(mcc->playlist.find(entity));
						delete entity;
						mcc->entities[msg["id"]] = nullptr;
					} else if (message == "calculate") {
						auto entity = getValidEntity(msg, "id", mcc->entities);
						auto order = getFloatInField(msg, "order");
						auto startSample = msg["start-sample"].is_null() ? 0 : getIntegerInField(msg, "start-sample");
						std::vector<double*> buffersToUse;
						for (int i = 0; i < msg["buffers-to-use"].size(); i++) {
							int bufId = msg["buffers-to-use"][i];
							buffersToUse.push_back(mcc->realtimebuffers[bufId].getWritePointer(0));
						}
						mcc->audioCallbackFunctionQueue.insert(std::make_tuple(startSample, [=](int sample) mutable {
							entity->orderOfComputation = order;
							entity->buffer.setDataToReferTo(buffersToUse.data(), buffersToUse.size(), mcc->samplesPerBlockExpected);
							mcc->playlist.insert(entity);
						}));
						if (msg["end-sample"].is_number_integer()) {
							mcc->audioCallbackFunctionQueue.insert(std::make_tuple(getIntegerInField(msg, "end-sample"), [=](int sample) {
								mcc->playlist.erase(mcc->playlist.find(entity));
							}));
						}
					} else if (message == "remove-calculation") {
						auto id = getIntegerInField(msg, "id");
						auto atSample = msg["at-sample"].is_null() ? 0 : getIntegerInField(msg, "at-sample");
						Entity* e = nullptr;
						for (auto entity : mcc->playlist) {
							if (entity->id == id) {
								mcc->audioCallbackFunctionQueue.insert(std::make_tuple(atSample, [=](int sample) {
									mcc->playlist.erase(entity);
								}));
								break;
							}
						}
						if (e == nullptr) {
							msg["reply"] = "The entity is not running."s;
						}
					/*} else if (message == "add-connection") {
						int connId = nextConnId();
						mcc->connections[connId] = new Connection{ connId, getValidEntity(msg, "source-id", mcc->entities), static_cast<int>(getIntegerInField(msg, "source-ch")), getValidEntity(msg, "dest-id", mcc->entities), static_cast<int>(getIntegerInField(msg, "dest-ch")) };
						auto startSample = msg["start-sample"].is_number_integer() ? getIntegerInField(msg, "start-sample") : 0;
						mcc->audioCallbackFunctionQueue.insert(std::make_tuple(startSample, [=](int sample) {
							Connection* c = mcc->connections[connId];
							if (c != nullptr) {
								if (c->destination != nullptr && c->source != nullptr) {
									c->destination->audioSources.insert(c);
									c->source->audioDestinations.insert(c);
								}
							}
						}));
						if (msg["end-sample"].is_number_integer()) {
							auto endSample = getIntegerInField(msg, "end-sample");
							mcc->audioCallbackFunctionQueue.insert(std::make_tuple(endSample, [=](int sample) {
								Connection* c = mcc->connections[connId];
								if (c != nullptr) {
									if (c->destination != nullptr && c->source != nullptr) {
										c->source->audioDestinations.erase(c);
										c->destination->audioSources.erase(c);
										delete c;
										mcc->connections[connId] = nullptr;
									}
								}
							}));
						}*/
						
						//conn->destination->audioSources.insert(conn);
						//conn->source->audioDestinations.insert(conn);
					/*} else if (message == "remove-connection") {
						Entity* source = getValidEntity(msg, "source-id", mcc->entities);
						int sourceCh = getIntegerInField(msg, "source-ch");
						Entity* dest = getValidEntity(msg, "dest-id", mcc->entities);
						int destCh = getIntegerInField(msg, "dest-ch");
						Connection* foundConn = nullptr;
						for (auto conn : source->audioDestinations) {
							if (conn->destination == dest && conn->sourceCh == sourceCh && conn->destCh == destCh) {
								foundConn = conn;
							}
						}
						if (foundConn == nullptr) { throw "Did not find connection."s; }

						auto connId = foundConn->id;
						auto atSample = msg["at-sample"].is_number_integer() ? getIntegerInField(msg, "at-sample") : 0;
						mcc->audioCallbackFunctionQueue.insert(std::make_tuple(atSample, [=](int i) {
							Connection* c = mcc->connections[connId];
							if (c != nullptr) {
								if (c->destination != nullptr && c->source != nullptr) {
									c->source->audioDestinations.erase(c);
									c->destination->audioSources.erase(c);
									delete c;
									mcc->connections[connId] = nullptr;
								}
							}
						}));	*/		
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
					} else if (message == "queue-clip") {
						auto entity = getValidEntity(msg, "dest-id", mcc->entities);
						auto afr = getValidAudioFormatReader(msg, "file-id", mcc->audioFormatReaders);
						auto destCh1 = msg.is_null() ? 0 : getIntegerInField(msg, "dest-ch-1");
						auto destCh2 = msg.is_null() ? 0 : getIntegerInField(msg, "dest-ch-2");

						int connId1 = nextConnId();
						mcc->connections[connId] = new Connection{ connId, getValidEntity(msg, "source-id", mcc->entities), static_cast<int>(getIntegerInField(msg, "source-ch")), getValidEntity(msg, "dest-id", mcc->entities), static_cast<int>(getIntegerInField(msg, "dest-ch")) };


						
						SoundFileProcessor* sfp = new SoundFileProcessor(0, afr, getIntegerInField(msg, "from-sample"), getIntegerInField(msg, "length"));
						int id = nextEntityId();
						mcc->entities[id] = new Entity{ id, sfp };
						mcc->audioCallbackFunctionQueue.insert(std::make_tuple(getIntegerInField(msg, "at-sample"), [=](int i) {
							mcc->playlist.insert(mcc->entities[id]);
							Connection* c = mcc->connections[connId];
							if (c != nullptr) {
								if (c->destination != nullptr && c->source != nullptr) {
									c->destination->audioSources.insert(c);
									c->source->audioDestinations.insert(c);
								}
							}
						}));
					} else if (message == "plugin-open-editor") {
						// consider a mechanism for default values for non-mandatory fields
						int x, y;
						if (!msg["x"].is_number_integer() || !msg["y"].is_number_integer()) {
							x = 10; y = 10;
						} else {
							x = msg["x"]; y = msg["y"];
						}
						Entity* entity = getPlugin(msg, "id", mcc->entities);
						entity->createEditorWindow(x, y);
					} else if (message == "plugin-close-editor") {
						Entity* entity = getPlugin(msg, "id", mcc->entities);
						entity->deleteEditorWindow();
					} else if (message == "get-sample-time") {
						reply["sample"] = mcc->sampleCount;
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
	bool initialized = false;
	int64 sampleCount = 0;
	double sampleRate;
	int samplesPerBlockExpected;
	TCPServer tcpServer{ this };
	std::vector<MidiInputDevice*> midiInputDevices;
	std::array<Entity*, ENTITY_LIMIT> entities;
	std::array<Connection*, CONN_LIMIT> connections;
	std::array<AudioFormatReader*, FILE_LIMIT> audioFormatReaders;
	std::array<AudioBuffer<double>, REALTIMEBUFFER_LIMIT> realtimebuffers;
	

	std::set<Entity*, std::function<bool(Entity*, Entity*)>> playlist{
		[](Entity* a, Entity* b) { return a->orderOfComputation < b->orderOfComputation; }
	};

	std::set<std::tuple<int64, std::function<void(int)>>, std::function<bool(std::tuple<int64, std::function<void(int)>>, std::tuple<int64, std::function<void(int)>>)>> audioCallbackFunctionQueue{
		[](std::tuple<int64, std::function<void(int)>> a, std::tuple<int64, std::function<void(int)>> b) { return std::get<0>(a) < std::get<0>(b);  } 
	};

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }
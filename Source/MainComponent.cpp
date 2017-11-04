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

#pragma warning (disable : 4100)

using namespace std::string_literals;

constexpr int ENTITY_LIMIT = 1000;

MessageManager* messageManager;

int nextId() {
	static int id = 2;
	return id++;
}

struct MidiInputDevice {
	MidiInputDevice(String midiDeviceName, AudioDeviceManager& deviceManager, double sampleRate) {
		midiMessageCollector.reset(sampleRate);
		deviceManager.setMidiInputEnabled(midiDeviceName, true);
		deviceManager.addMidiInputCallback(midiDeviceName, &midiMessageCollector);
	}

	MidiMessageCollector midiMessageCollector;
	MidiBuffer midiBuffer;
};

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

enum EntityType {
	AudioProcessorType, AudioFormatReaderType, InputChannels, OutputChannels, MidiInputType
};

struct Entity;

struct Connection {
	Connection(Entity* src, int srcCh, Entity* dest, int destCh) : source{ src }, sourceCh{ srcCh }, destination{ dest }, destCh{ destCh } {}
	Entity* source;
	int sourceCh;
	Entity* destination;
	int destCh;
	int64 startSample = 0;
	int64 endSample = 4611686018427387904; // 2**62. 3 million years. Should be enough
};

struct Entity {
	Entity(long id, EntityType type) : id{ id }, type{ type } {}
	Entity(long id, AudioProcessor* processor, double order, double sampleRate) : id{ id }, audioProcessor{ processor }, orderOfComputation{ order } {
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
	
	~Entity() {
		if (type == EntityType::AudioFormatReaderType) {
			delete audioFormatReader;
		} else if (type == EntityType::AudioProcessorType) {
			for (auto conn : audioSources) {
				conn.first->source->audioDestinations.erase(conn.first);
				delete conn.first;
			}
			for (auto conn : audioDestinations) {
				conn.first->destination->audioSources.erase(conn.first);
				delete conn.first;
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

	void process(int sampleCount, int numSamples) {
		buffer.setSize(std::max(numInputChannels, numOutputChannels), numSamples, false, false, true);
		buffer.clear();
		for (auto& connection : audioSources) {
			buffer.addFrom(connection.first->destCh, 0, connection.first->source->buffer.getReadPointer(connection.first->sourceCh), numSamples);
		}

		midibuffer.clear();
		for (auto src : midiSources) {
			midibuffer.addEvents(*src, 0, numSamples, 0);
		}

		while (!midiQueue.empty() && midiQueue.front().getTimeStamp() < sampleCount) {
			midiQueue.pop_front();
		}

		while (!midiQueue.empty() && midiQueue.front().getTimeStamp() < sampleCount + numSamples) {
			midibuffer.addEvent(midiQueue.front(), midiQueue.front().getTimeStamp() - sampleCount);
			midiQueue.pop_front();
		}
		
		if (type == EntityType::AudioProcessorType) {
			ScopedLock lock(audioProcessor->getCallbackLock());

			if (audioProcessor->supportsDoublePrecisionProcessing()) {
				audioProcessor->processBlock(buffer, midibuffer);
			} else {
				AudioBuffer<float> floatbuffer;
				floatbuffer.makeCopyOf(buffer);
				audioProcessor->processBlock(floatbuffer, midibuffer);
				buffer.makeCopyOf(floatbuffer);
			}
		} else if (type == EntityType::InputChannels) {

		} else if (type == EntityType::OutputChannels) {

		}
	}
	
	long id;
	int numInputChannels = 0;
	int numOutputChannels = 0;
	AudioBuffer<double> buffer;
	MidiBuffer midibuffer;
	double orderOfComputation = 0;
	AudioProcessor* audioProcessor = nullptr;
	PluginWindow* pluginWindow = nullptr;
	AudioFormatReader* audioFormatReader = nullptr;
	std::unordered_map<Connection*, Connection*> audioSources;
	std::unordered_map<Connection*, Connection*> audioDestinations;
	std::vector<MidiBuffer*> midiSources;
	std::deque<MidiMessage> midiQueue;

	EntityType type;
};

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

int getIntegerInField(nlohmann::json& msg, const std::string& field) {
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

int getValidId(nlohmann::json& msg, const std::string& field) {
	int id = getIntegerInField(msg, field);
	if (id < 0 && id >= ENTITY_LIMIT) {
		throw "" + field + " is not within entity limits.";
	}
	return id;
}

Entity* getValidEntity(nlohmann::json& msg, const std::string& field, std::array<Entity*, ENTITY_LIMIT>& entities) {
	Entity* entity = entities[getValidId(msg, field)];
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

class MainContentComponent : public Component, public AudioSource {
public:
	MainContentComponent() {
		setSize(100, 100);
		setTopLeftPosition(0, 700);

		messageManager = MessageManager::getInstance();
		
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
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
		this->sampleRate = sampleRate;

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

		entities.fill(nullptr);

		AudioDeviceManager::AudioDeviceSetup ads;
		deviceManager.getAudioDeviceSetup(ads);
		entities[0] = new Entity{ 0, EntityType::InputChannels };
		entities[0]->numInputChannels = 0;
		entities[0]->numOutputChannels = device->getActiveInputChannels().countNumberOfSetBits();
		entities[0]->orderOfComputation = DBL_MIN;
		entities[1] = new Entity{ 1, EntityType::OutputChannels };
		entities[1]->numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
		entities[1]->numOutputChannels = 0;
		entities[1]->orderOfComputation = DBL_MAX;

		playlist.push_back(entities[1]);

		auto midiDeviceNames = MidiInput::getDevices();
		for (int i = 0; i < midiDeviceNames.size(); i++) {
			midiInputDevices.push_back(new MidiInputDevice(midiDeviceNames[i], deviceManager, sampleRate));
			std::cout << "midi device at index " << i << ": " << midiDeviceNames[i] << "\n";
		}

		initialized = true;
	}

	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
		for (auto& midi : midiInputDevices) {
			midi->midiBuffer.clear();
			midi->midiMessageCollector.removeNextBlockOfMessages(midi->midiBuffer, bufferToFill.numSamples);
		}

		entities[0]->buffer.setSize(entities[0]->numOutputChannels, bufferToFill.numSamples, false, false, true);
		entities[0]->buffer.clear();
		for (int i = 0; i < entities[0]->numOutputChannels; i++) {
			for (int srcSample = bufferToFill.startSample, destSample = 0; destSample < bufferToFill.numSamples; srcSample++, destSample++) {
				entities[0]->buffer.setSample(i, destSample, bufferToFill.buffer->getSample(i, srcSample));
			}
		}

		std::sort(playlist.begin(), playlist.end(), [](Entity* a, Entity* b) { return a->orderOfComputation < b->orderOfComputation; });
		for (auto entity : playlist) {
			entity->process(sampleCount, bufferToFill.numSamples);
		}

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

		void run() override {
			serverSocket.createListener(8989, "127.0.0.1");
			char* buf = new char[1024 * 1024];
			while (true) {
				auto connSocket = serverSocket.waitForNextConnection();
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
					} else if (message == "load-plugin") {; 
						double order = getFloatInField(msg, "order"); 
						int id = nextId();
						mcc->entities[id] = new Entity{ id, createNewPlugin(msg, "path"), order, mcc->sampleRate };
						mcc->playlist.push_back(mcc->entities[id]);
						reply["id"] = id;
					} else if (message == "remove-plugin") {
						auto entity = getPlugin(msg, "id", mcc->entities);
						// https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
						mcc->playlist.erase(std::remove(mcc->playlist.begin(), mcc->playlist.end(), entity), mcc->playlist.end());
						delete entity;
						mcc->entities[msg["id"]] = nullptr;
					} else if (message == "add-connection") {
						Connection* conn = new Connection{ getValidEntity(msg, "source-id", mcc->entities), getIntegerInField(msg, "source-ch"), getValidEntity(msg, "dest-id", mcc->entities), getIntegerInField(msg, "dest-ch") };
						conn->destination->audioSources[conn] = conn;
						conn->source->audioDestinations[conn] = conn;
					} else if (message == "remove-connection") {
						Entity* source = getValidEntity(msg, "source-id", mcc->entities);
						int sourceCh = getIntegerInField(msg, "source-ch");
						Entity* dest = getValidEntity(msg, "dest-id", mcc->entities);
						int destCh = getIntegerInField(msg, "dest-ch");
						Connection* foundConn = nullptr;
						for (auto conn : source->audioDestinations) {
							if (conn.first->destination == dest && conn.first->sourceCh == sourceCh && conn.first->destCh == destCh) {
								foundConn = conn.first;
							}
						}
						if (foundConn == nullptr) { throw "Did not find connection."s; }
						source->audioDestinations.erase(foundConn);
						dest->audioSources.erase(foundConn);
						delete foundConn;
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
							throw "Field \"source-id\" must be present and be either an integer or a string.";
						}
						getPlugin(msg, "dest-id", mcc->entities)->midiSources.push_back(midibuffer);
					} else if (message == "plugin-queue-midi") {
						auto entity = getPlugin(msg, "id", mcc->entities);
						if (msg["midi"].is_array()) {
							auto midivec = msg["midi"];
							for (int i = 0; i < midivec.size(); i++) {
								if (midivec[i].is_object()) {
									auto obj = midivec[i];
									auto sample = getFloatInField(obj, "sample");
									auto type = getStringInField(obj, "type");
									if (type == "on") {
										entity->midiQueue.push_back(MidiMessage::noteOn(getIntegerInField(obj, "ch"), getIntegerInField(obj, "note"), static_cast<uint8>(getIntegerInField(obj, "vel"))));
										entity->midiQueue.back().setTimeStamp(sample);
									} else if (type == "off") {
										entity->midiQueue.push_back(MidiMessage::noteOff(getIntegerInField(obj, "ch"), getIntegerInField(obj, "note"), static_cast<uint8>(getIntegerInField(obj, "vel"))));
										entity->midiQueue.back().setTimeStamp(sample);
									} else if (type == "cc") {
										entity->midiQueue.push_back(MidiMessage::controllerEvent(getIntegerInField(obj, "ch"), getIntegerInField(obj, "ccnum"), static_cast<uint8>(getIntegerInField(obj, "val"))));
										entity->midiQueue.back().setTimeStamp(sample);
									} else {
										throw "midi type not understood.";
									}
								} else {
									throw "array should contain objects";
								}
							}
						} else {
							throw "field midi does not exist or is not an array";
						}
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
	TCPServer tcpServer{ this };
	std::vector<MidiInputDevice*> midiInputDevices;
	std::array<Entity*, ENTITY_LIMIT> entities;
	std::vector<Connection*> calculationQueue; // sorted by sample
	std::vector<Entity*> playlist; // sorted by order
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

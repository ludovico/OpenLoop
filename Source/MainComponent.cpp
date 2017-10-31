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

#pragma warning (disable : 4100)

constexpr int ENTITY_LIMIT = 1000;

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

	//MidiInput* midiInput;
	MidiMessageCollector midiMessageCollector;
	MidiBuffer midiBuffer;
};

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

enum EntityType {
	AudioProcessorType, AudioFormatReaderType, InputChannels, OutputChannels, MidiInputType
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

		audioProcessor->prepareToPlay(sampleRate, 256);
	}
	
	~Entity() {
		if (type == EntityType::AudioFormatReaderType) {
			delete audioFormatReader;
		} else if (type == EntityType::AudioProcessorType) {
			audioProcessor->releaseResources();
			delete audioProcessor;
		}
	}

	void process(int numSamples) {
		auto midibuffer = MidiBuffer();
		buffer.setSize(std::max(numInputChannels, numOutputChannels), numSamples, false, false, true);
		buffer.clear();
		for (auto tup : getsInputFromEntityChannelToChannel) {
			Entity* source = std::get<0>(tup);
			int sourceCh = std::get<1>(tup);
			int destCh = std::get<2>(tup);
			buffer.addFrom(destCh, 0, source->buffer.getReadPointer(sourceCh), numSamples);
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
	double orderOfComputation = 0;
	AudioProcessor* audioProcessor = nullptr;
	PluginWindow* pluginWindow = nullptr;
	AudioFormatReader* audioFormatReader = nullptr;
	std::vector<std::tuple<Entity*, int, int>> getsInputFromEntityChannelToChannel;

	EntityType type;
};

struct Connection {
	Entity* source;
	int sourceCh;
	Entity* destination;
	int destCh;
	int64 startSample = 0;
	int64 endSample = 4611686018427387904; // 2**62. 3 million years. Should be enough
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

class MainContentComponent : public AudioAppComponent {
public:
	MainContentComponent() {
		setSize(100, 100);
		setTopLeftPosition(0, 700);
		setAudioChannels(64, 64);
		
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
			deviceManager.initialise(64, 64, nullptr, true, "", &ads);
		}

		//saveDesktopPlugins();

		pluginWindow.toFront(true);
		tcpServer.startThread();
	}

	~MainContentComponent() {
		shutdownAudio();
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
			entity->process(bufferToFill.numSamples);
		}

		AudioBuffer<float> output;
		output.makeCopyOf(entities[1]->buffer, false);
		for (int i = 0; i < bufferToFill.buffer->getNumChannels(); i++) {
			bufferToFill.buffer->copyFrom(i, bufferToFill.startSample, output.getReadPointer(i), bufferToFill.numSamples);
		}

		sample += bufferToFill.numSamples;
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
						if (msg["order"].is_number_float()) {
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
												mcc->entities[id] = new Entity{ id, api, msg["order"], mcc->sampleRate };												
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
							reply["error"] = "\"order\" field is null or not a real number";
						}
					} else {
						reply["error"] = "\"path\" field is null or it is not a string.";
					}
					reply["msgid"] = msg["msgid"];
				} else if (msg["msg"] == "remove-plugin") {
					if (msg["id"].is_number_integer()) {
						int id = msg["id"];
						if (id >= 0 && id < ENTITY_LIMIT) {
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
							reply["error"] = "This id is outside of the entity limits. Check your programming logic!";
						}
					} else {
						reply["error"] = "\"id\" field is null or not an integer id";
					}
				} else if (msg["msg"] == "add-connection") {
					if (msg["source-id"].is_number_integer() && msg["source-ch"].is_number_integer() 
						&& msg["dest-id"].is_number_integer() && msg["dest-ch"].is_number_integer()) {
						if (msg["source-id"] >= 0 && msg["dest-id"] >= 0 && msg["source-id"] < ENTITY_LIMIT && msg["dest-id"] < ENTITY_LIMIT) {
							Entity* source = mcc->entities[msg["source-id"]];
							Entity* dest = mcc->entities[msg["dest-id"]];
							if (source != nullptr && dest != nullptr) {
								int sourceCh = msg["source-ch"];
								int destCh = msg["dest-ch"];
								mcc->playlist.push_back(source);
								dest->getsInputFromEntityChannelToChannel.push_back(std::make_tuple(source, sourceCh, destCh));
							} else {
								reply["error"] = "This id does not refer to anything. Check your...";
							}
						} else {
							reply["error"] = "This id is outside of the entity limits. Check your programming logic!";
						}
					} else {
						reply["error"] = "fields \"source-id\", \"source-ch\", \"dest-id\" and \"dest-ch\" must be present and contain an integer!";
					}
				} else if (msg["msg"] == "plugin-open-editor") {
					if (msg["id"].is_number_integer()) {
						if (msg["id"] >= 0 && msg["id"] < ENTITY_LIMIT) {
							Entity* entity = mcc->entities[msg["id"]];
							if (entity != nullptr) {
								if (entity->type == EntityType::AudioProcessorType) {
									if (entity->pluginWindow == nullptr) {
										messageManager->callFunctionOnMessageThread([](void* ent) {
											Entity* entity = static_cast<Entity*>(ent);
											entity->pluginWindow = new PluginWindow();
											entity->pluginWindow->setEditor(entity->audioProcessor->createEditorIfNeeded());
											return ent; 
										}, entity);
									} else {
										entity->pluginWindow->setVisible(true);
									}
								} else {
									reply["error"] = "This id does not refer to a plugin. Check your programming logic!";
								}
							} else {
								reply["error"] = "This id does not refer to anything. Check your...";
							}
						} else {
							reply["error"] = "This id is outside of the entity limits. Check your programming logic!";
						}
					} else {
						reply["error"] = "\"id\" field is null or not an integer id";
					}
				} else if (msg["msg"] == "plugin-close-editor") {
					if (msg["id"].is_number_integer()) {
						if (msg["id"] >= 0 && msg["id"] < ENTITY_LIMIT) {
							Entity* entity = mcc->entities[msg["id"]];
							if (entity != nullptr) {
								if (entity->type == EntityType::AudioProcessorType) {
									if (entity->pluginWindow != nullptr) {
										entity->pluginWindow->setVisible(false);
									} 
								} else {
									reply["error"] = "This id does not refer to a plugin. Check your programming logic!";
								}
							} else {
								reply["error"] = "This id does not refer to anything. Check your...";
							}
						} else {
							reply["error"] = "This id is outside of the entity limits. Check your programming logic!";
						}
					} else {
						reply["error"] = "\"id\" field is null or not an integer id";
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
		MessageManager* messageManager;
		MainContentComponent* mcc;
	};
	
	int64 sample = 0;
	double sampleRate;
	TCPServer tcpServer{ this };
	PluginWindow pluginWindow;
	std::vector<MidiInputDevice*> midiInputDevices;
	std::array<Entity*, ENTITY_LIMIT> entities;
	std::vector<Connection*> calculationQueue; // sorted by sample
	std::vector<Entity*> playlist; // sorted by order
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

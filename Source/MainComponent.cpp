#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>
#include "../Source/sol.hpp"

#pragma warning (disable : 4100)

class PluginWindow : public DocumentWindow {
public:
	PluginWindow(AudioProcessorEditor* pluginEditor)
	:DocumentWindow (pluginEditor->getName(), Colour::fromHSV(0.4, 0.4, 0.6, 1.0), DocumentWindow::minimiseButton | DocumentWindow::closeButton) {
		setSize(400, 300);
		setContentOwned(pluginEditor, true);
		setVisible(true);
	}

	void closeButtonPressed() override { delete this; }

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

void savePluginXml(String path) {
	VSTPluginFormat vstPluginFormat;
	OwnedArray<PluginDescription> plugindescs;
	vstPluginFormat.findAllTypesForFile(plugindescs, path);
	for (auto pd : plugindescs) {
		auto xml = pd->createXml();
		File filepath{ path };
		File newFile{ "C:/Users/User/Documents/OpenLoop/Resources/plugins/" + filepath.getFileNameWithoutExtension() + ".xml" };
		auto res = xml->writeToFile(newFile, "");
		std::cout << res << std::endl;
	}
}

class TCPServer : public Thread {
public:
	TCPServer(sol::state& lua) : Thread("tcp-server"), lua{ lua } {}

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
			auto s = String{ utf8 };
			
			MessageManager::callAsync([=]() {
				auto str = s;
				lua.script(s.toStdString(), [](lua_State* L, sol::protected_function_result pfr) {
					std::cout << "syntax error" << std::endl;
					return pfr;
				});
			});
		}
	}

	sol::state& lua;
	StreamingSocket serverSocket;
};

class MainContentComponent : public AudioAppComponent {
public:
	MainContentComponent() {
		setSize(400, 300);
		setAudioChannels(2, 2);

		VSTPluginFormat vstPluginFormat;
		OwnedArray<PluginDescription> plugindescs;
		// U-HE
		//savePluginXml("C:/Program Files/VSTPlugIns/ACE(x64).dll");
		/*vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Bazille(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Diva(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Filterscape(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/FilterscapeQ6(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/FilterscapeVA(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Hive(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/ACE(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/MFM2(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Presswerk(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Protoverb(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Runciter(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Satin(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Zebra(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/ZRev(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-A(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-D(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-F(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-G(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-P(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-Q(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-S(x64).dll");
		vstPluginFormat.findAllTypesForFile(plugindescs, "C:/Program Files/VSTPlugIns/Uhbik-T(x64).dll");
		*/
		std::cout << plugindescs.size() << std::endl;

		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::os);

		lua.set("deviceManager", &deviceManager);
		
		lua.new_usertype<AudioDeviceManager>(
			"AudioDeviceManager", sol::constructors<>(),
			"getCurrentAudioDeviceType", &AudioDeviceManager::getCurrentAudioDeviceType,
			"getCpuUsage", &AudioDeviceManager::getCpuUsage,
			"createStateXml", &AudioDeviceManager::createStateXml,
			"playTestSound", &AudioDeviceManager::playTestSound,
			"addAudioCallback", &AudioDeviceManager::addAudioCallback
			);

		lua.new_usertype<AudioFormatReaderSource>(
			"AudioFormatReaderSource", sol::constructors<AudioFormatReaderSource(AudioFormatReader*, bool)>(),
			"setLooping", &AudioFormatReaderSource::setLooping,
			"isLooping", &AudioFormatReaderSource::isLooping,
			"getAudioFormatReader", &AudioFormatReaderSource::getAudioFormatReader,
			"getNextReadPosition", &AudioFormatReaderSource::getNextReadPosition,
			"getTotalLength", &AudioFormatReaderSource::getTotalLength
			);

		lua.new_usertype<AudioPluginFormatManager>(
			"AudioPluginFormatManager", sol::constructors<AudioPluginFormatManager()>(),
			"addDefaultFormats", &AudioPluginFormatManager::addDefaultFormats,
			"getNumFormats", &AudioPluginFormatManager::getNumFormats,
			"getFormat", &AudioPluginFormatManager::getFormat,
			"createPluginInstance", &AudioPluginFormatManager::createPluginInstance,
			"doesPluginStillExist", &AudioPluginFormatManager::doesPluginStillExist
			);
			
		lua.new_usertype<AudioPluginInstance>(
			"AudioPluginInstance", sol::constructors<>(),
			"fillInPluginDescription", &AudioPluginInstance::fillInPluginDescription,
			"getPluginDescription", &AudioPluginInstance::getPluginDescription,
			"getName", &AudioPluginInstance::getName,
			"getBusCount", &AudioPluginInstance::getBusCount,
			"isUsingDoublePrecision", &AudioPluginInstance::isUsingDoublePrecision,
			"getTotalNumInputChannels", &AudioPluginInstance::getTotalNumInputChannels,
			"getTotalNumOutputChannels", &AudioPluginInstance::getTotalNumOutputChannels,
			"getMainBusNumInputChannels", &AudioPluginInstance::getMainBusNumInputChannels,
			"getMainBusNumOutputChannels", &AudioPluginInstance::getMainBusNumOutputChannels,
			"acceptsMidi", &AudioPluginInstance::acceptsMidi,
			"producesMidi", &AudioPluginInstance::producesMidi,
			"reset", &AudioPluginInstance::reset,
			"createEditor", &AudioPluginInstance::createEditor,
			"createEditorIfNeeded", &AudioPluginInstance::createEditorIfNeeded,
			"getStateInformation", &AudioPluginInstance::getStateInformation
			//"setStateInformation", &AudioPluginInstance::setStateInformation
			);
		
		lua.new_usertype<AudioProcessorPlayer>(
			"AudioProcessorPlayer", sol::constructors<AudioProcessorPlayer(bool)>(),
			"setProcessor", &AudioProcessorPlayer::setProcessor,
			"getCurrentProcessor", &AudioProcessorPlayer::getCurrentProcessor,
			"getMidiMessageCollector", &AudioProcessorPlayer::getMidiMessageCollector,
			"setDoublePrecisionProcessing", &AudioProcessorPlayer::setDoublePrecisionProcessing
			);

		lua.new_usertype<Colour>(
			"Colour", sol::constructors<Colour(), Colour(float, float, float, float)>()
			);

		lua.set_function("createEditorWindow", [](String name) { MessageManager::callAsync([&]() { new DocumentWindow(name, Colour{}, 6, true); }); });

		lua.new_usertype<PluginWindow>(
			"PluginWindow", sol::constructors<PluginWindow(AudioProcessorEditor*)>()
			);

		/*lua.new_usertype<DocumentWindow>(
			"DocumentWindow", sol::constructors<DocumentWindow(String, Colour, int, bool)>(),
			//"DocumentWindow", sol::constructors<>(),
			"setName", &DocumentWindow::setName,
			"setTitleBarHeight", &DocumentWindow::setTitleBarHeight,
			"setTitleBarTextCentred", &DocumentWindow::setTitleBarTextCentred,
			"isResizable", &DocumentWindow::isResizable,
			"setResizable", &DocumentWindow::setResizable,
			"setContentOwned", &DocumentWindow::setContentOwned,
			"isVisible", &DocumentWindow::isVisible,
			"setVisible", &DocumentWindow::setVisible,
			"setSize", &DocumentWindow::setSize
			);*/

		lua.new_usertype<File>(
			"File", sol::constructors<File(), File(const String&)>(),
			"exists", &File::exists,
			"existsAsFile", &File::existsAsFile,
			"isDirectory", &File::isDirectory,
			"isRoot", &File::isRoot,
			"getSize", &File::getSize,
			"getFullPathName", &File::getFullPathName,
			"getFileName", &File::getFileName,
			"getRelativePathFrom", &File::getRelativePathFrom,
			"getFileExtension", &File::getFileExtension,
			"hasFileExtension", &File::hasFileExtension,
			"withFileExtension", &File::withFileExtension,
			"getFileNameWithoutExtension", &File::getFileNameWithoutExtension,
			"hashCode", &File::hashCode,
			"hashCode64", &File::hashCode64,
			"getChildFile", &File::getChildFile,
			"getSiblingFile", &File::getSiblingFile,
			"getParentDirectory", &File::getParentDirectory,
			"isAChildOf", &File::isAChildOf,
			"getNonexistentChildFile", &File::getNonexistentChildFile,
			"getNonexistentSibling", &File::getNonexistentSibling,
			"hasWriteAccess", &File::hasWriteAccess,
			"setReadOnly", &File::setReadOnly,
			"setExecutePermission", &File::setExecutePermission,
			"isHidden", &File::isHidden,
			"getFileIdentifier", &File::getFileIdentifier,
			"getLastModificationTime", &File::getLastModificationTime,
			"getLastAccessTime", &File::getLastAccessTime,
			"getCreationTime", &File::getCreationTime,
			"getVersion", &File::getVersion,
			"create", &File::create,
			"createDirectory", &File::createDirectory,
			"deleteFile", &File::deleteFile,
			"deleteRecursively", &File::deleteRecursively,
			"moveToTrash", &File::moveToTrash,
			"moveFileTo", &File::moveFileTo,
			"copyFileTo", &File::copyFileTo,
			"replaceFileIn", &File::replaceFileIn,
			"copyDirectoryTo", &File::copyDirectoryTo,
			"findChildFiles", &File::findChildFiles,
			"getNumberOfChildFiles", &File::getNumberOfChildFiles,
			"createInputStream", &File::createInputStream,
			"createOutputStream", &File::createOutputStream
			);

		lua.new_usertype<FileInputStream>(
			"FileInputStream", sol::constructors<FileInputStream(const File&)>(),
			"getFile", &FileInputStream::getFile,
			"getStatus", &FileInputStream::getStatus,
			"failedToOpen", &FileInputStream::failedToOpen,
			"openedOk", &FileInputStream::openedOk,
			"getTotalLength", &FileInputStream::getTotalLength,
			"read", &FileInputStream::read,
			"isExhausted", &FileInputStream::isExhausted,
			"getPosition", &FileInputStream::getPosition,
			"setPosition", &FileInputStream::setPosition
			);

		lua.new_usertype<KnownPluginList>(
			"KnownPluginList", sol::constructors<KnownPluginList()>(),
			"clear", &KnownPluginList::clear,
			"getNumTypes", &KnownPluginList::getNumTypes,
			"getType", &KnownPluginList::getType,
			"getTypeForFile", &KnownPluginList::getTypeForFile,
			"getTypeForIdentifierString", &KnownPluginList::getTypeForIdentifierString,
			"addType", &KnownPluginList::addType,
			"removeType", &KnownPluginList::removeType,
			"scanAndAddFile", &KnownPluginList::scanAndAddFile,
			"createXml", &KnownPluginList::createXml,
			"recreateFromXml", &KnownPluginList::recreateFromXml
			);

		lua.set_function("noteOn", [](int ch, int noteNum, uint8 vel) { return MidiMessage::noteOn(ch, noteNum, vel); });
		lua.set_function("noteOff", [](int ch, int noteNum, uint8 vel) { return MidiMessage::noteOff(ch, noteNum, vel); });
		
		lua.new_usertype<MidiMessage>(
			"MidiMessage", sol::constructors<MidiMessage(), MidiMessage(int, int, int, double)>(),
			"getTimeStamp", &MidiMessage::getTimeStamp,
			"getNoteNumber", &MidiMessage::getNoteNumber,
			"getVelocity", &MidiMessage::getVelocity,
			"isNoteOn", &MidiMessage::isNoteOn,
			"isNoteOff", &MidiMessage::isNoteOff
			);

		lua.new_usertype<MidiMessageCollector>(
			"MidiMessageCollector", sol::constructors<MidiMessageCollector()>(),
			"reset", &MidiMessageCollector::reset,
			"addMessageToQueue", &MidiMessageCollector::addMessageToQueue
			);

		lua.new_usertype<OwnedArray<PluginDescription>>(
			"OwnedArray", sol::constructors<OwnedArray<PluginDescription>()>(),
			"size", &OwnedArray<PluginDescription>::size,
			"get", &OwnedArray<PluginDescription>::operator[]
			);

		lua.new_usertype<PluginDescription>(
			"PluginDescription", sol::constructors<PluginDescription()>(),
			"isDuplicateOf", &PluginDescription::isDuplicateOf,
			"matchesIdentifierString", &PluginDescription::matchesIdentifierString,
			"createIdentifierString", &PluginDescription::createIdentifierString,
			"createXml", &PluginDescription::createXml,
			"loadFromXml", &PluginDescription::loadFromXml,
			"name", &PluginDescription::name,
			"descriptiveName", &PluginDescription::descriptiveName,
			"pluginFormatName", &PluginDescription::pluginFormatName,
			"category", &PluginDescription::category,
			"manufacturerName", &PluginDescription::manufacturerName,
			"version", &PluginDescription::version,
			"fileOrIdentifier", &PluginDescription::fileOrIdentifier,
			"lastFileModTime", &PluginDescription::lastFileModTime,
			"lastInfoUpdateTime", &PluginDescription::lastInfoUpdateTime,
			"uid", &PluginDescription::uid,
			"isInstrument", &PluginDescription::isInstrument,
			"numInputChannels", &PluginDescription::numInputChannels,
			"numOutputChannels", &PluginDescription::numOutputChannels,
			"hasSharedContainer", &PluginDescription::hasSharedContainer
			);

		lua.new_usertype<SoundPlayer>(
			"SoundPlayer", sol::constructors<SoundPlayer()>(),
			"play", [](SoundPlayer& soundPlayer, const File& file) { soundPlayer.play(file); },
			"playTestSound", &SoundPlayer::playTestSound
			);

		lua.new_usertype<String>(
			"String", sol::constructors<String(), String(const std::string&)>(),
			"length", &String::length,
			"hashCode", &String::hashCode,
			"hashCode64", &String::hashCode64,
			"toUTF8", &String::toUTF8
			);

		lua.new_usertype<WavAudioFormat>(
			"WavAudioFormat", sol::constructors<WavAudioFormat()>(),
			"createReaderFor", &WavAudioFormat::createReaderFor
			);

		lua.new_usertype<XmlDocument>(
			"XmlDocument", sol::constructors<XmlDocument(String), XmlDocument(File)>(),
			"getDocumentElement", &XmlDocument::getDocumentElement,
			"getLastParseError", &XmlDocument::getLastParseError
			);

		lua.set_function("parseXmlDocument", [](const File& file) { return XmlDocument::parse(file); });

		lua.new_usertype<XmlElement>(
			"XmlElement", sol::constructors<>(),
			"getTagName", &XmlElement::getTagName,
			"getNumAttributes", &XmlElement::getNumAttributes,
			"getAttributeName", &XmlElement::getAttributeName,
			"getAttributeValue", &XmlElement::getAttributeValue
			);
			

		/*lua.new_usertype<AudioPluginFormat>(
		//"AudioPluginFormat", sol::constructors<AudioPluginFormat()>(),
		"findAllTypesForFile", &AudioPluginFormat::findAllTypesForFile
		);*/

		/*lua.new_usertype<VSTPluginFormat>(
		"VSTPluginFormat", sol::constructors<VSTPluginFormat()>(),
		"findAllTypesForFile", &VSTPluginFormat::findAllTypesForFile
		);

		lua.new_usertype<VST3PluginFormat>(
		"VSTPluginFormat", sol::constructors<VST3PluginFormat()>(),
		"findAllTypesForFile", &VST3PluginFormat::findAllTypesForFile
		);*/

		/*auto pluginxml = XmlDocument::parse(File{ "C:/Users/User/Documents/OpenLoop/Resources/plugins/ACE(x64).xml" });
		pd.loadFromXml(*pluginxml);
		auto apfm = new AudioPluginFormatManager;
		apfm->addDefaultFormats();
		String errorstring;
		instance = apfm->createPluginInstance(pd, 44100, 480, errorstring);
		applayer.setProcessor(instance);
		deviceManager.addAudioCallback(&applayer);
		editor = instance->createEditorIfNeeded();
		
		pluginwindow = new PluginWindow{ editor };
		pluginwindow->toFront(true);*/
		//docwindow.setSize(500, 500);
		//docwindow.setContentOwned(editor, true);
		//docwindow.setVisible(true);
		//docwindow.toFront(true);

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
	
	sol::state lua;
	TCPServer tcpServer{ lua };

	/*AudioPluginInstance* instance;
	AudioProcessorEditor* editor;
	AudioProcessorPlayer applayer{ true };
	PluginDescription pd;
	DocumentWindow docwindow{ "ace", Colour::fromHSV(0.3, 0.2, 0.6, 0.4), 4, true };
	PluginWindow* pluginwindow;*/

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

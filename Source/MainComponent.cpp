#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>
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
			File{ "C:/Users/User/Documents/OpenLoop/Resources/plugins/" + filepath.getFileNameWithoutExtension() + ".xml" } :
			File{ "C:/code/c++/juce/OpenLoop/Resources/laptop/plugins/" + filepath.getFileNameWithoutExtension() + ".xml" };

		auto res = xml->writeToFile(newFile, "");
		std::cout << res << std::endl;
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

class TCPServer : public Thread {
public:
	//TCPServer(sol::state& lua) : Thread("tcp-server"), lua{ lua } {}
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
				//lua.script(s.toStdString(), [](lua_State* L, sol::protected_function_result pfr) {
				//	std::cout << "syntax error" << std::endl;
				//	return pfr;
				//);
				try {
					chai.eval(s);
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

auto constructMidiEventsFromCollectionWithinBufferRange = [](MidiBuffer* midiBuffer, double time, int numSamples) {

};

class MainContentComponent : public AudioAppComponent {
public:
	MainContentComponent() {
		setSize(100, 100);
		setTopLeftPosition(0, 700);
		setAudioChannels(2, 2);

		saveLaptopPlugins();

		//lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::os);

		/*lua.set("deviceManager", &deviceManager);
		lua.set("pluginWindow", &pluginWindow);
		
		lua.new_usertype<AudioDeviceManager>(
			"AudioDeviceManager", sol::constructors<>(),
			"getCurrentAudioDeviceType", &AudioDeviceManager::getCurrentAudioDeviceType,
			"getCpuUsage", &AudioDeviceManager::getCpuUsage,
			"createStateXml", &AudioDeviceManager::createStateXml,
			"playTestSound", &AudioDeviceManager::playTestSound,
			"addAudioCallback", &AudioDeviceManager::addAudioCallback,
			"removeAudioCallback", &AudioDeviceManager::removeAudioCallback,
			"addMidiInputCallback", &AudioDeviceManager::addMidiInputCallback,
			"removeMidiInputCallback", &AudioDeviceManager::removeMidiInputCallback,
			"setMidiInputEnabled", &AudioDeviceManager::setMidiInputEnabled
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

		lua.set_function("MidiInputGetDevices", MidiInput::getDevices);
		lua.set_function("MidiInputOpenDevice", MidiInput::openDevice);

		lua.new_usertype<MidiInput>(
			"MidiInput", sol::constructors<>(),
			"getName", &MidiInput::getName,
			"start", &MidiInput::start,
			"stop", &MidiInput::stop
			);

		lua.new_usertype<MidiKeyboardState>(
			"MidiKeyboardState", sol::constructors<MidiKeyboardState()>(),
			"reset", &MidiKeyboardState::reset,
			"allNotesOff", &MidiKeyboardState::allNotesOff,
			"addListener", &MidiKeyboardState::addListener,
			"removeListener", &MidiKeyboardState::removeListener
			);

		lua.set_function("noteOn", [](int ch, int noteNum, uint8 vel) { return MidiMessage::noteOn(ch, noteNum, vel); });
		lua.set_function("noteOff", [](int ch, int noteNum, uint8 vel) { return MidiMessage::noteOff(ch, noteNum, vel); });
		
		lua.new_usertype<MidiMessage>(
			"MidiMessage", sol::constructors<MidiMessage(), MidiMessage(int, int, int, double)>(),
			"getTimeStamp", &MidiMessage::getTimeStamp,
			"setTimeStamp", &MidiMessage::setTimeStamp,
			"addToTimeStamp", &MidiMessage::addToTimeStamp,
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

		lua.new_usertype<PluginWindow>(
			"PluginWindow", sol::constructors<>(),
			"clear", &PluginWindow::clear,
			"setEditor", &PluginWindow::setEditor
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
			);*/

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

		chai.add(chaiscript::user_type<File>(), "File");
		chai.add(chaiscript::constructor<File()>(), "File");
		chai.add(chaiscript::constructor<File(const String&)>(), "File");
		chai.add(chaiscript::fun(static_cast<File(*)(std::string)>([](std::string s) { return File{ s }; })), "File");
		chai.add(chaiscript::fun(&File::exists), "exists");

		chai.add(chaiscript::user_type<MidiInput>(), "MidiInput");
		chai.add(chaiscript::fun(MidiInput::getDevices), "MidiInputGetDevices");
		chai.add(chaiscript::fun(MidiInput::openDevice), "MidiInputOpenDevice");
		chai.add(chaiscript::fun(&MidiInput::getName), "getName");
		chai.add(chaiscript::fun(&MidiInput::start), "start");
		chai.add(chaiscript::fun(&MidiInput::stop), "stop");

		chai.add(chaiscript::user_type<MidiMessage>(), "MidiMessage");
		//chai.add(chaiscript::fun(static_cast<MidiMessage(*)(int, int, uint8)>([](int ch, int nn, uint8 vel) { return MidiMessage::noteOn(ch, nn, vel); })), "MidiMessageNoteOn");
		//chai.add(chaiscript::fun(static_cast<MidiMessage(*)(int, int, uint8)>([](int ch, int nn, uint8 vel) { return MidiMessage::noteOff(ch, nn, vel); })), "MidiMessageNoteOff");
		chai.add(chaiscript::fun(MidiMessage::noteOn), "noteOn");
		chai.add(chaiscript::fun(MidiMessage::noteOff), "noteOff");

		chai.add(chaiscript::constructor<MidiMessage()>(), "MidiMessage");
		chai.add(chaiscript::constructor<MidiMessage(int, int, int, double)>(), "MidiMessage");
		chai.add(chaiscript::fun(&MidiMessage::getTimeStamp), "getTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::setTimeStamp), "setTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::addToTimeStamp), "addToTimeStamp");
		chai.add(chaiscript::fun(&MidiMessage::getNoteNumber), "getNoteNumber");
		chai.add(chaiscript::fun(&MidiMessage::getVelocity), "getVelocity");
		chai.add(chaiscript::fun(&MidiMessage::isNoteOn), "isNoteOn");
		chai.add(chaiscript::fun(&MidiMessage::isNoteOff), "isNoteOff");

		chai.add(chaiscript::user_type<PluginDescription>(), "PluginDescription");
		chai.add(chaiscript::constructor<PluginDescription()>(), "PluginDescription");
		chai.add(chaiscript::fun(&PluginDescription::loadFromXml), "loadFromXml");
		chai.add(chaiscript::fun(&PluginDescription::name), "name");

		chai.add(chaiscript::user_type<PluginWindow>(), "PluginWindow");
		chai.add(chaiscript::fun(&PluginWindow::clear), "clear");
		chai.add(chaiscript::fun(&PluginWindow::setEditor), "setEditor");

		chai.add(chaiscript::user_type<String>(), "String");
		chai.add(chaiscript::constructor<String(const std::string&)>(), "String");
		chai.add(chaiscript::fun(static_cast<std::string(*)(String)>([](String s) { return s.toStdString(); })), "to_string");
		
		chai.add(chaiscript::user_type<XmlDocument>(), "XmlDocument");
		chai.add(chaiscript::constructor<XmlDocument(String)>(), "XmlDocument");
		chai.add(chaiscript::constructor<XmlDocument(File)>(), "XmlDocument");
		chai.add(chaiscript::fun(&XmlDocument::getDocumentElement), "getDocumentElement");
		
		chai.add(chaiscript::user_type<XmlElement>(), "XmlElement");

		/*lua.set_function("createEditorWindow", [](String name) { MessageManager::callAsync([&]() { new DocumentWindow(name, Colour{}, 6, true); }); });*/

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
		editor = instance->createEditorIfNeeded();*/
		
		
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
	
	//sol::state lua;
	//TCPServer tcpServer{ lua };
	chaiscript::ChaiScript chai;
	TCPServer tcpServer{ chai };
	PluginWindow pluginWindow;

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

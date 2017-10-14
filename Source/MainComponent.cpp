#include "../JuceLibraryCode/JuceHeader.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>
#include "../Source/sol.hpp"

#pragma warning (disable : 4100)


class TCPServer : public Thread {
public:
	TCPServer(sol::state& lua) : Thread("tcp-server"), lua{ lua } {}

	void run() override {
		serverSocket.createListener(8989, "127.0.0.1");
		char* buf = new char[50000];
		while (true) {
			auto connSocket = serverSocket.waitForNextConnection();
			std::cout << "connection established" << std::endl;
			int i = 0;
			connSocket->read(&i, 4, true);
			std::cout << i << " bytes to be sent" << std::endl;

			connSocket->read(buf, i, true);
			buf[i] = 0;

			auto utf8 = CharPointer_UTF8{ buf };
			auto s = String{ utf8 };
			
			lua.script(s.toStdString(), [](lua_State* L, sol::protected_function_result pfr) {
				std::cout << "syntax error" << std::endl;
				return pfr;
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

		lua.new_usertype<SoundPlayer>(
			"SoundPlayer", sol::constructors<SoundPlayer()>(),
			"play", [](SoundPlayer& soundPlayer, const File& file) { soundPlayer.play(file); },
			"playTestSound", &SoundPlayer::playTestSound
			);

		lua.new_usertype<WavAudioFormat>(
			"WavAudioFormat", sol::constructors<WavAudioFormat()>(),
			"createReaderFor", &WavAudioFormat::createReaderFor
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

		lua.new_usertype<XmlElement>(
			"XmlElement", sol::constructors<>(),
			"getTagName", &XmlElement::getTagName,
			"getNumAttributes", &XmlElement::getNumAttributes,
			"getAttributeName", &XmlElement::getAttributeName,
			"getAttributeValue", &XmlElement::getAttributeValue
			);

		lua.new_usertype<String>(
			"String", sol::constructors<String(), String(const std::string&)>(), 
			"length", &String::length,
			"hashCode", &String::hashCode,
			"hashCode64", &String::hashCode64,
			"toUTF8", &String::toUTF8
			);

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

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent() { return new MainContentComponent(); }

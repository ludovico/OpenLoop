#include "../JuceLibraryCode/JuceHeader.h"

Component* createMainContentComponent();

class OpenLoopApplication : public JUCEApplication {
public:
	OpenLoopApplication() {}

	const String getApplicationName() override { return ProjectInfo::projectName; }
	const String getApplicationVersion() override { return ProjectInfo::versionString; }
	bool moreThanOneInstanceAllowed() override { return true; }

	void initialise(const String& commandLine) override {
		mainWindow = new MainWindow(getApplicationName());
	}

	void shutdown() override {
		mainWindow = nullptr; // (deletes our window)
	}

	void systemRequestedQuit() override {
		quit();
	}

	void anotherInstanceStarted(const String& commandLine) override {

	}

	class MainWindow : public DocumentWindow {
	public:
		MainWindow(String name) : DocumentWindow(name,
			Desktop::getInstance().getDefaultLookAndFeel()
			.findColour(ResizableWindow::backgroundColourId),
			DocumentWindow::allButtons) {
			setUsingNativeTitleBar(true);
			setContentOwned(createMainContentComponent(), true);
			setResizable(true, true);

			centreWithSize(getWidth(), getHeight());
			setVisible(true);
		}

		void closeButtonPressed() override {
			JUCEApplication::getInstance()->systemRequestedQuit();
		}

		/* Note: Be careful if you override any DocumentWindow methods - the base
		   class uses a lot of them, so by overriding you might break its functionality.
		   It's best to do all your work in your content component instead, but if
		   you really have to override any DocumentWindow methods, make sure your
		   subclass also calls the superclass's method.
		*/

	private:
		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
	};

private:
	ScopedPointer<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(OpenLoopApplication)

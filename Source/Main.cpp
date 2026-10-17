#include "Ui.h"

using namespace juce;

class CleanYourMindApp : public JUCEApplication
{
public:
    const String getApplicationName() override { return "Clean Your Mind"; }
    const String getApplicationVersion() override { return CYM_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const String&) override { window = std::make_unique<Window>(); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const String&) override
    {
        if (window != nullptr)
            window->toFront (true);
    }

private:
    class Window : public DocumentWindow
    {
    public:
        Window() : DocumentWindow ("Clean Your Mind", Colour (0xff18191c), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1000, 660, 8000, 6000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (CleanYourMindApp)

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Library.h"
#include "Theme.h"

//==============================================================================
// Text in two languages: tr ("по-русски", "in English")
juce::String tr (const char* ru, const char* en);
void setUiLanguage (const juce::String& lang);
bool uiIsRussian();

juce::Font uiFont (float height, bool bold = false, float kerning = 0.0f);

// Colours that every screen needs, derived from the active theme
struct Pal
{
    juce::Colour bg, panel, widget, widgetHi, stroke, ink, mute, faint, accent, accentHi, accentDim, onAccent;
    static Pal from (const Theme& t);
};

class CymLook : public juce::LookAndFeel_V4
{
public:
    CymLook();
    void setTheme (const Theme&);
    const Theme& theme() const { return *current; }
    const Pal& pal() const { return p; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                        int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;

private:
    const Theme* current = nullptr;
    Pal p;
};

const Pal& palOf (const juce::Component&);
const Theme& themeOf (const juce::Component&);

// Pill or rounded button; toggle state = accent fill
class Chip : public juce::TextButton
{
public:
    explicit Chip (const juce::String& text = {}, bool pill = true);
};

enum class Icon { Refresh, Theme, Settings, Folder, Plus, Cross, Star, Camera, Back, Search, Reveal, Box, Globe, Ban };
juce::Path iconPath (Icon);

class IconButton : public juce::Button
{
public:
    IconButton (Icon i, const juce::String& label = {});
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    int idealWidth() const;
    Icon icon;
    bool accentIcon = true;
    bool primary = false;
};

// Small text prompt (OK / Cancel); the callback gets the text when OK is pressed
void askForText (const juce::String& title, const juce::String& message, const juce::String& initial,
                 std::function<void (const juce::String&)> onOk);

//==============================================================================
struct AppState
{
    Library lib;
    std::map<juce::String, juce::Image> thumbs;
    std::function<void()> refreshAll;               // re-filter / repaint everything
    std::function<void (const juce::String& key, const juce::File& image)> editImage;
    std::function<void (const juce::String& key)> editExistingImage;
    std::function<void (const juce::String& key)> quarantine;
    std::function<void (int index)> restore;
    std::function<void()> rescan;
    std::function<void (const juce::String& msg)> toast;
    std::function<void()> categoriesChanged;
    void newCategory (std::function<void (const juce::String&)> then = {});

    juce::Image thumbFor (const juce::String& key);
    juce::String lastUsedText (const Plugin&, bool longYear = false) const;
    juce::String frequencyText (const Plugin&) const;
    int monthsSince (const Plugin&) const;
    bool canDelete (const Plugin&) const;
    enum class Usage { Never, Often, Sometimes, Stale };
    Usage usage (const Plugin&) const;
};

//==============================================================================
class PluginCard : public juce::Component, public juce::SettableTooltipClient
{
public:
    PluginCard (AppState&, const juce::String& key);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    juce::String key;
    bool dropHighlight = false;
    static constexpr float imageAspect = 252.0f / 132.0f;
    static int heightForWidth (int w, float zoom);

private:
    AppState& app;
    bool hover = false, dragStarted = false;
    juce::Rectangle<float> starArea() const;
    juce::Rectangle<float> hideArea() const;
    float zoom() const { return app.lib.galleryZoom; }
    juce::Rectangle<float> imageArea() const;
    void showMenu();
    Plugin* plugin() const { return app.lib.find (key); }
};

class GalleryGrid : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit GalleryGrid (AppState&);
    void setKeys (const juce::StringArray&);
    void layoutForWidth (int w);
    void resized() override {}
    void paint (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;
    juce::String emptyText;

private:
    AppState& app;
    juce::OwnedArray<PluginCard> cards;
    PluginCard* cardAt (int x, int y);
    void clearHighlight();
};

class GalleryPage : public juce::Component, private juce::Timer
{
public:
    explicit GalleryPage (AppState&);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void updateTexts();

private:
    AppState& app;
    juce::TextEditor search;
    Chip all, inst, fx, fav, noImg, sandbox;
    juce::ComboBox category, sort;
    juce::Viewport view;
    GalleryGrid grid;
    juce::String status;
    int typeFilter = 0, lastCategoryId = 1;
    static constexpr int newCategoryId = 9999;
    void timerCallback() override { stopTimer(); refresh(); }
    void setTypeFilter (int);
};

//==============================================================================
class FolderList : public juce::Component, public juce::SettableTooltipClient
{
public:
    FolderList (AppState&, bool projects);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverRow = -1; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    int idealHeight() const;
    void updateTexts();
    std::function<void()> onHeightChanged;

private:
    AppState& app;
    bool projects, expanded = false;
    static constexpr int collapsedRows = 5;
    Chip add;
    int hoverRow = -1;
    int rowH() const;
    int shownFolders() const;           // folder rows drawn
    bool hasToggleRow() const;          // "... and N more" / "show less"
    int totalRows() const { return shownFolders() + (hasToggleRow() ? 1 : 0); }
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File>& folders();
    juce::Rectangle<int> rowBounds (int i) const;
};

class StatsPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit StatsPage (AppState&);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    void updateTexts();

private:
    AppState& app;
    Chip fAll, fOften, fStale, fNever, fDelete, fQuarantine;
    Chip m6, m12, m24;
    struct ZoomList : public juce::ListBox
    {
        // Ctrl + wheel goes up to the window (zoom) instead of scrolling the list
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isCtrlDown() || e.mods.isCommandDown()) Component::mouseWheelMove (e, w);
            else ListBox::mouseWheelMove (e, w);
        }
    };
    ZoomList list;
    juce::Viewport sideView;
    juce::Component sideContent;
    FolderList pluginFolders, projectFolders;
    int filter = 0, sortCol = 2;
    bool sortAsc = false;
    std::vector<juce::String> rows;   // plugin keys (or quarantine indices as text)

    juce::Rectangle<int> tilesArea, headerArea, deletePanel;
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setFilter (int);
    void columnLayout (int w, int* x) const;
public:
    void applyZoom();
};

//==============================================================================
class CropEditor : public juce::Component
{
public:
    CropEditor (AppState&, const juce::String& key, juce::Image original, bool isNew);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    std::function<void()> onClose;
    const juce::String& getKey() const { return key; }

private:
    AppState& app;
    juce::String key;
    juce::Image img;
    bool fresh = false;
    Library::ImageState st;
    juce::Slider zoom;
    Chip fill, fit, stretch, cancel, save;
    juce::Rectangle<int> card, stage, preview;
    juce::Rectangle<float> frame;
    juce::Point<float> dragStartCentre, dragStartMouse;
    bool draggingImage = false;
    juce::Rectangle<float> imageRectInFrame (juce::Rectangle<float> frameRect) const;
    juce::Image render (int w, int h) const;
    void setMode (int);
    void clampCentre();
};

//==============================================================================
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void zoomBy (float factor, bool reset = false);

private:
    AppState app;
    CymLook look;
    juce::TooltipWindow tooltips { this, 600 };
    Chip tabGallery, tabStats;
    IconButton rescanBtn { Icon::Refresh }, themeBtn { Icon::Theme }, langBtn { Icon::Globe };
    GalleryPage gallery { app };
    StatsPage stats { app };
    std::unique_ptr<CropEditor> crop;
    juce::Image background;
    juce::Rectangle<int> logoArea;
    juce::String toastText;
    juce::uint32 toastUntil = 0;
    bool scanning = false;
    juce::uint32 zoomSaveAt = 0;

    class ScanJob;
    std::unique_ptr<ScanJob> job;
    void startScan();
    void timerCallback() override;
    void showPage (int);
    void cycleTheme();
    void applyTheme();
    void updateTexts();
    void buildBackground();
    void openCrop (const juce::String& key, juce::Image img, bool isNew);
    void doQuarantine (const juce::String& key);
    void doRestore (int index);
    bool pasteImage();
    int page = 0;
};

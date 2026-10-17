#include "Ui.h"
#include "Logo.h"

using namespace juce;

Image readClipboardImage();

//==============================================================================
class MainComponent::ScanJob : public Thread
{
public:
    ScanJob (MainComponent& o) : Thread ("CYM scan"), owner (&o)
    {
        pluginRoots = o.app.lib.pluginFolders;
        projectRoots = o.app.lib.projectFolders;
        cache = o.app.lib.projectCache;
    }
    ~ScanJob() override { stopThread (8000); }

    void run() override
    {
        auto result = std::make_shared<ScanResult>();
        auto exit = [this] { return threadShouldExit(); };
        result->files = Scanner::scanPlugins (pluginRoots, exit);
        result->projects = Scanner::scanProjects (projectRoots, cache, exit);
        if (threadShouldExit())
            return;
        MessageManager::callAsync ([safe = owner, result]
        {
            if (auto* o = safe.getComponent())
            {
                o->app.lib.applyScan (std::move (*result));
                o->scanning = false;
                o->rescanBtn.setEnabled (true);
                o->app.refreshAll();
            }
        });
    }

private:
    Component::SafePointer<MainComponent> owner;
    Array<File> pluginRoots, projectRoots;
    std::map<String, ProjectRecord> cache;
};

//==============================================================================
static bool moveItem (const File& from, const File& to)
{
    to.getParentDirectory().createDirectory();
    if (from.moveFileTo (to))
        return true;
    if (from.isDirectory())
    {
        if (from.copyDirectoryTo (to) && from.deleteRecursively())
            return true;
        if (to.isDirectory() && from.isDirectory())
            to.deleteRecursively();
    }
    return false;
}

MainComponent::MainComponent()
{
    setLookAndFeel (&look);
    LookAndFeel::setDefaultLookAndFeel (&look);
    setUiLanguage (app.lib.lang);
    look.setTheme (Theme::byName (app.lib.themeName));

    app.refreshAll = [this]
    {
        app.lib.computeStats();
        gallery.refresh();
        stats.refresh();
        repaint();
    };
    app.rescan = [this] { startScan(); };
    app.categoriesChanged = [this] { gallery.updateTexts(); app.refreshAll(); };
    app.toast = [this] (const String& m)
    {
        toastText = m;
        toastUntil = Time::getMillisecondCounter() + 4500;
        repaint();
    };
    app.editImage = [this] (const String& key, const File& f)
    {
        const auto img = ImageFileFormat::loadFrom (f);
        if (! img.isValid())
        {
            app.toast (tr ("Не получилось открыть картинку: ", "Couldn't open the picture: ") + f.getFileName());
            return;
        }
        openCrop (key, img, true);
    };
    app.editExistingImage = [this] (const String& key)
    {
        auto img = ImageFileFormat::loadFrom (app.lib.originalImageFile (key));
        if (! img.isValid())
            img = ImageFileFormat::loadFrom (app.lib.imageFile (key));
        if (img.isValid())
        {
            openCrop (key, img, false);
            return;
        }
        auto chooser = std::make_shared<FileChooser> (tr ("Выбери картинку", "Choose a picture"), File(), "*.png;*.jpg;*.jpeg;*.gif");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, chooser, key] (const FileChooser& fc)
        {
            if (fc.getResult().existsAsFile())
                app.editImage (key, fc.getResult());
        });
    };
    app.quarantine = [this] (const String& key) { doQuarantine (key); };
    app.restore = [this] (int i) { doRestore (i); };

    tabGallery.getProperties().set ("pill", false);
    tabStats.getProperties().set ("pill", false);
    tabGallery.onClick = [this] { showPage (0); };
    tabStats.onClick = [this] { showPage (1); };
    addAndMakeVisible (tabGallery);
    addAndMakeVisible (tabStats);

    rescanBtn.onClick = [this] { startScan(); };
    themeBtn.onClick = [this] { cycleTheme(); };
    langBtn.accentIcon = false;
    langBtn.onClick = [this]
    {
        app.lib.lang = uiIsRussian() ? "en" : "ru";
        setUiLanguage (app.lib.lang);
        app.lib.saveSettings();
        updateTexts();
    };
    for (auto* b : { &rescanBtn, &themeBtn, &langBtn })
        addAndMakeVisible (*b);

    addAndMakeVisible (gallery);
    addChildComponent (stats);
    setWantsKeyboardFocus (true);

    updateTexts();
    showPage (0);
    setSize (1240, 860);

    // show what we know right away, then refresh from disk
    app.refreshAll();
    startScan();
    startTimer (200);
}

MainComponent::~MainComponent()
{
    job.reset();
    LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MainComponent::updateTexts()
{
    tabGallery.setButtonText (tr ("Галерея", "Gallery"));
    tabStats.setButtonText (tr ("Статистика", "Stats"));
    rescanBtn.setButtonText (tr ("Пересканировать", "Rescan"));
    rescanBtn.setTooltip (tr ("Заново найти плагины и прочитать проекты", "Find plugins and read projects again"));
    themeBtn.setTooltip (tr ("Тема: ", "Theme: ") + app.lib.themeName);
    langBtn.setButtonText (uiIsRussian() ? "RU" : "EN");
    langBtn.setTooltip (tr ("Язык: русский / английский", "Language: Russian / English"));
    gallery.updateTexts();
    stats.updateTexts();
    app.refreshAll();
    resized();
}

void MainComponent::showPage (int p)
{
    page = p;
    tabGallery.setToggleState (p == 0, dontSendNotification);
    tabStats.setToggleState (p == 1, dontSendNotification);
    gallery.setVisible (p == 0);
    stats.setVisible (p == 1);
}

void MainComponent::startScan()
{
    if (scanning)
        return;
    scanning = true;
    rescanBtn.setEnabled (false);
    job = std::make_unique<ScanJob> (*this);
    job->startThread();
    repaint();
}

void MainComponent::cycleTheme()
{
    const auto& all = Theme::all();
    size_t i = 0;
    for (; i < all.size(); ++i)
        if (all[i].name == app.lib.themeName)
            break;
    app.lib.themeName = all[(i + 1) % all.size()].name;
    app.lib.saveSettings();
    applyTheme();
}

void MainComponent::applyTheme()
{
    look.setTheme (Theme::byName (app.lib.themeName));
    background = {};
    themeBtn.setTooltip (tr ("Тема: ", "Theme: ") + app.lib.themeName);
    gallery.updateTexts();
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::buildBackground()
{
    const auto& t = themeOf (*this);
    const int S = 256;
    background = Image (Image::RGB, S, S, false);
    Random rnd (1234);
    Image::BitmapData bd (background, Image::BitmapData::writeOnly);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x)
        {
            if (t.gloss)
            {
                bd.setPixelColour (x, y, t.bg);
                continue;
            }
            const float nz = (rnd.nextFloat() - 0.5f) * 0.06f;
            auto c = rnd.nextFloat() < 0.004f ? t.bg.interpolatedWith (t.speck, 0.4f + rnd.nextFloat() * 0.6f)
                                              : (nz > 0 ? t.bg.brighter (nz * 2.0f) : t.bg.darker (-nz * 2.0f));
            bd.setPixelColour (x, y, c);
        }
}

void MainComponent::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto& c = palOf (*this);
    if (! background.isValid())
        buildBackground();
    g.setTiledImageFill (background, 0, 0, 1.0f);
    g.fillAll();
    ColourGradient vig (Colours::transparentBlack, (float) getWidth() * 0.5f, (float) getHeight() * 0.45f,
                        t.vignette, 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillAll();

    // header
    const auto la = logoArea.toFloat();
    if (t.gloss)
    {
        ColourGradient gold (Colour (0xfff6dc9a), la.getX(), la.getY(), Colour (0xffa47a2c), la.getX(), la.getBottom(), false);
        gold.addColour (0.55, Colour (0xffe8c46a));
        Logo::draw (g, la, FillType (gold));
    }
    else
        Logo::draw (g, la, FillType (t.logo));
    g.setColour (t.lightBg ? t.ink : Colours::white);
    g.setFont (uiFont (27.0f, true, 0.08f));
    g.drawText ("CLEAN YOUR MIND", logoArea.getRight() + 14, 14, 330, 32, Justification::centredLeft);
    g.setColour (c.mute);
    g.setFont (uiFont (10.0f, true, 0.4f));
    g.drawText ("PRIMAL RAGE LAB", logoArea.getRight() + 16, 46, 300, 14, Justification::centredLeft);

    // tab box
    const auto tabs = tabGallery.getBounds().getUnion (tabStats.getBounds()).expanded (4).toFloat();
    g.setColour (c.panel);
    g.fillRoundedRectangle (tabs, 10.0f);
    g.setColour (c.stroke);
    g.drawRoundedRectangle (tabs.reduced (0.5f), 10.0f, 1.0f);

    if (scanning)
    {
        g.setColour (c.mute);
        g.setFont (uiFont (12.0f));
        const int dots = (int) ((Time::getMillisecondCounter() / 350) % 4);
        g.drawText (tr ("Сканирую", "Scanning") + String::repeatedString (".", dots), rescanBtn.getX() - 140, rescanBtn.getY(), 130,
                    rescanBtn.getHeight(), Justification::centredRight);
    }
    else if (app.lib.lastScan.toMilliseconds() > 0)
    {
        g.setColour (c.mute);
        g.setFont (uiFont (12.0f));
        g.drawText (tr ("Скан: ", "Scanned: ") + app.lib.lastScan.formatted ("%H:%M"), rescanBtn.getX() - 140, rescanBtn.getY(), 130,
                    rescanBtn.getHeight(), Justification::centredRight);
    }

    g.setColour (c.stroke);
    g.drawHorizontalLine (76, 0.0f, (float) getWidth());
}

void MainComponent::paintOverChildren (Graphics& g)
{
    if (toastText.isEmpty() || Time::getMillisecondCounter() > toastUntil)
        return;
    const auto& c = palOf (*this);
    const auto f = uiFont (13.0f);
    const int w = jmin (getWidth() - 80, GlyphArrangement::getStringWidthInt (f, toastText) + 40);
    const auto r = Rectangle<int> (w, 42).withCentre ({ getWidth() / 2, getHeight() - 60 }).toFloat();
    g.setColour (c.panel.withAlpha (0.97f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (c.accent);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);
    g.setColour (c.ink);
    g.setFont (f);
    g.drawFittedText (toastText, r.toNearestInt().reduced (16, 4), Justification::centred, 2);
}

void MainComponent::resized()
{
    logoArea = { 24, 12, 50, 52 };
    auto header = getLocalBounds().removeFromTop (76).reduced (24, 20);
    langBtn.setBounds (header.removeFromRight (langBtn.idealWidth()));
    header.removeFromRight (8);
    themeBtn.setBounds (header.removeFromRight (36));
    header.removeFromRight (8);
    rescanBtn.setBounds (header.removeFromRight (rescanBtn.idealWidth()));

    const int tabsX = logoArea.getRight() + 14 + 300 + 30;
    const int tw1 = GlyphArrangement::getStringWidthInt (uiFont (13.0f, true), tabGallery.getButtonText()) + 36;
    const int tw2 = GlyphArrangement::getStringWidthInt (uiFont (13.0f, true), tabStats.getButtonText()) + 36;
    tabGallery.setBounds (tabsX, 22, tw1, 32);
    tabStats.setBounds (tabsX + tw1 + 4, 22, tw2, 32);

    const auto content = getLocalBounds().withTrimmedTop (77);
    gallery.setBounds (content);
    stats.setBounds (content);
    if (crop != nullptr)
        crop->setBounds (getLocalBounds());
}

void MainComponent::timerCallback()
{
    if (zoomSaveAt != 0 && Time::getMillisecondCounter() > zoomSaveAt)
    {
        zoomSaveAt = 0;
        app.lib.saveSettings();
    }
    if (scanning || (toastText.isNotEmpty() && Time::getMillisecondCounter() < toastUntil + 300))
        repaint();
}

void MainComponent::openCrop (const String& key, Image img, bool isNew)
{
    crop = std::make_unique<CropEditor> (app, key, img, isNew);
    SafePointer<MainComponent> safeThis (this);
    crop->onClose = [safeThis]
    {
        MessageManager::callAsync ([safe = safeThis]
        {
            if (safe != nullptr)
            {
                safe->crop.reset();
                safe->grabKeyboardFocus();
            }
        });
    };
    addAndMakeVisible (*crop);
    crop->setBounds (getLocalBounds());
    crop->grabKeyboardFocus();
}

bool MainComponent::pasteImage()
{
    const auto img = readClipboardImage();
    if (! img.isValid())
    {
        app.toast (tr ("В буфере нет картинки. Сделай скрин (Win+Shift+S) и жми Ctrl+V над карточкой.",
                       "No picture on the clipboard. Take a screenshot and press Ctrl+V over a card."));
        return true;
    }
    if (crop != nullptr)
    {
        // a new picture for the card that is being edited
        const auto key = crop->getKey();
        openCrop (key, img, true);
        return true;
    }
    const auto screenPos = Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
    auto* under = getComponentAt (getLocalPoint (nullptr, screenPos));
    PluginCard* card = dynamic_cast<PluginCard*> (under);
    if (card == nullptr && under != nullptr)
        card = under->findParentComponentOfClass<PluginCard>();
    if (card == nullptr || page != 0)
    {
        app.toast (tr ("Наведи мышку на карточку плагина и нажми Ctrl+V ещё раз.", "Hover a plugin card and press Ctrl+V again."));
        return true;
    }
    openCrop (card->key, img, true);
    return true;
}

void MainComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    // Ctrl (Cmd on Mac) + wheel: bigger or smaller pictures and lists. Viewports and lists
    // pass these events up instead of scrolling.
    if ((e.mods.isCtrlDown() || e.mods.isCommandDown()) && crop == nullptr && std::abs (w.deltaY) > 0.0001f)
    {
        const float d = jlimit (-0.5f, 0.5f, w.deltaY);
        zoomBy (std::exp (d * 0.9f));
    }
}

void MainComponent::zoomBy (float factor, bool reset)
{
    auto& z = page == 0 ? app.lib.galleryZoom : app.lib.statsZoom;
    const float nz = reset ? 1.0f : jlimit (0.6f, 2.5f, z * factor);
    if (std::abs (nz - z) < 0.0005f)
        return;
    z = nz;
    if (page == 0) gallery.resized();
    else stats.applyZoom();
    app.toast (tr ("Масштаб ", "Zoom ") + String (roundToInt (z * 100.0f)) + "%   " + tr ("(Ctrl+0 = обычный)", "(Ctrl+0 = normal)"));
    zoomSaveAt = Time::getMillisecondCounter() + 800;
}

bool MainComponent::keyPressed (const KeyPress& k)
{
    if (k == KeyPress ('=', ModifierKeys::commandModifier, 0) || k == KeyPress ('+', ModifierKeys::commandModifier, 0)
        || k == KeyPress (KeyPress::numberPadAdd, ModifierKeys::commandModifier, 0))
    {
        zoomBy (1.15f);
        return true;
    }
    if (k == KeyPress ('-', ModifierKeys::commandModifier, 0) || k == KeyPress (KeyPress::numberPadSubtract, ModifierKeys::commandModifier, 0))
    {
        zoomBy (1.0f / 1.15f);
        return true;
    }
    if (k == KeyPress ('0', ModifierKeys::commandModifier, 0) || k == KeyPress (KeyPress::numberPad0, ModifierKeys::commandModifier, 0))
    {
        zoomBy (1.0f, true);
        return true;
    }
    if (k == KeyPress ('v', ModifierKeys::commandModifier, 0))
        return pasteImage();
    if (k == KeyPress::escapeKey && crop != nullptr)
    {
        crop.reset();
        return true;
    }
    if (k == KeyPress ('f', ModifierKeys::commandModifier, 0))
    {
        showPage (0);
        return false;
    }
    return false;
}

void MainComponent::doQuarantine (const String& key)
{
    auto* p = app.lib.find (key);
    if (p == nullptr)
        return;
    const auto name = p->name;
    const auto msg = tr ("Убрать ", "Move ") + name + tr (" в карантин?\n\nФайлы переедут в папку программы, DAW перестанет их видеть. Вернуть можно во вкладке «Карантин».",
                                                        " to quarantine?\n\nIts files move into the app's folder and your DAW won't see them. You can restore it from the Quarantine tab.");
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, tr ("Карантин", "Quarantine"), msg,
                                  tr ("Убрать", "Move"), tr ("Отмена", "Cancel"), this,
                                  ModalCallbackFunction::create ([this, key] (int r)
    {
        if (r == 0)
            return;
        auto* pl = app.lib.find (key);
        if (pl == nullptr)
            return;
        const auto stamp = Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        int moved = 0, failed = 0;
        for (const auto& f : pl->files)
        {
            const auto dest = app.lib.quarantineDir().getChildFile (stamp).getChildFile (formatName (f.format)).getChildFile (f.file.getFileName());
            if (moveItem (f.file, dest))
            {
                app.lib.quarantined.push_back ({ key, pl->name + " (" + formatName (f.format) + ")", f.file.getFullPathName(),
                                                 dest.getFullPathName(), Time::currentTimeMillis() });
                ++moved;
            }
            else
                ++failed;
        }
        app.lib.saveQuarantine();
        if (failed > 0)
            app.toast (tr ("Не хватило прав на часть файлов. Запусти Clean Your Mind от имени администратора и повтори.",
                           "No permission for some files. Run Clean Your Mind as administrator and try again."));
        else
            app.toast (tr ("Убрал в карантин: ", "Quarantined: ") + String (moved) + tr (" файл(ов)", " file(s)"));
        startScan();
    }));
}

void MainComponent::doRestore (int index)
{
    if (index < 0 || index >= (int) app.lib.quarantined.size())
        return;
    const auto q = app.lib.quarantined[(size_t) index];
    const File from (q.to), to (q.from);
    if (to.exists())
    {
        app.toast (tr ("На старом месте уже есть такой файл: ", "A file already exists at the old place: ") + to.getFullPathName());
        return;
    }
    if (! moveItem (from, to))
    {
        app.toast (tr ("Не вышло вернуть: нужны права администратора.", "Couldn't restore: administrator rights needed."));
        return;
    }
    app.lib.quarantined.erase (app.lib.quarantined.begin() + index);
    app.lib.saveQuarantine();
    app.toast (tr ("Вернул: ", "Restored: ") + q.name);
    startScan();
}

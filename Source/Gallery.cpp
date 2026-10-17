#include "Ui.h"

using namespace juce;

static bool isImageFile (const String& path)
{
    const auto ext = File (path).getFileExtension().toLowerCase();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp" || ext == ".webp";
}

static String monogram (const String& name)
{
    StringArray words;
    words.addTokens (name.replaceCharacters ("-_()", "    "), " ", "");
    words.removeEmptyStrings();
    String m;
    for (auto& w : words)
    {
        if (CharacterFunctions::isDigit (w[0]))
            m += w.substring (0, 2);
        else if (CharacterFunctions::isLetter (w[0]))
            m += String::charToString (w[0]).toUpperCase();
        if (m.length() >= 3) break;
    }
    if (m.length() == 1 && name.length() > 1)
        m = name.substring (0, 2).toUpperCase();
    return m.substring (0, 3);
}

// On Windows a VST3 is usually a folder; DAWs want the binary inside it
// (Name.vst3\Contents\x86_64-win\Name.vst3), the same path Bitwig stores in projects.
static File dragPathFor (const PluginFile& f)
{
   #if JUCE_WINDOWS
    if (f.format == Format::VST3 && f.file.isDirectory())
    {
        const auto contents = f.file.getChildFile ("Contents");
        for (auto* arch : { "x86_64-win", "arm64x-win", "arm64ec-win" })
        {
            const auto bin = contents.getChildFile (arch).getChildFile (f.file.getFileName());
            if (bin.existsAsFile())
                return bin;
        }
        // any .vst3 binary inside a *-win folder
        for (const auto& e : RangedDirectoryIterator (contents, true, "*.vst3", File::findFiles))
            if (e.getFile().getParentDirectory().getFileName().endsWithIgnoreCase ("-win"))
                return e.getFile();
    }
   #endif
    return f.file;
}

//==============================================================================
PluginCard::PluginCard (AppState& a, const String& k) : key (k), app (a)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

int PluginCard::heightForWidth (int w, float z) { return roundToInt ((float) w / imageAspect) + roundToInt (58.0f * z); }

Rectangle<float> PluginCard::imageArea() const
{
    const float w = (float) getWidth();
    return { 0.0f, 0.0f, w, std::round (w / imageAspect) };
}

Rectangle<float> PluginCard::starArea() const
{
    const float s = jlimit (0.8f, 1.6f, zoom());
    return { (float) getWidth() - 34.0f * s, 4.0f * s, 30.0f * s, 28.0f * s };
}

// under the star: "don't need it" - sends the plugin to the sandbox
Rectangle<float> PluginCard::hideArea() const
{
    const float s = jlimit (0.8f, 1.6f, zoom());
    const auto ia = imageArea();
    return { (float) getWidth() - 34.0f * s, ia.getBottom() - 32.0f * s, 30.0f * s, 28.0f * s };
}

void PluginCard::paint (Graphics& g)
{
    const auto* p = plugin();
    if (p == nullptr)
        return;
    const auto& c = palOf (*this);
    const float z = zoom();
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = 12.0f;

    g.setColour (c.panel);
    g.fillRoundedRectangle (r, rad);

    // picture
    const auto ia = imageArea().reduced (0.5f, 0.5f).withTrimmedBottom (0.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip;
        clip.addRoundedRectangle (ia.getX(), ia.getY(), ia.getWidth(), ia.getHeight(), rad, rad, true, true, false, false);
        g.reduceClipRegion (clip);
        g.setColour (c.panel.darker (0.35f));
        g.fillRect (ia);
        const auto img = app.thumbFor (key);
        if (img.isValid())
        {
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (img, ia, RectanglePlacement::fillDestination);
        }
        else
        {
            const auto inner = ia.reduced (ia.getWidth() * 0.08f, ia.getHeight() * 0.11f);
            Path dash;
            dash.addRoundedRectangle (inner, 6.0f);
            Path dashed;
            const float lens[] = { 5.0f, 4.0f };
            PathStrokeType (1.3f).createDashedStroke (dashed, dash, lens, 2);
            g.setColour (c.faint);
            g.fillPath (dashed);
            g.setColour (c.accentDim);
            g.setFont (uiFont (jmin (34.0f, inner.getHeight() * 0.36f), true, 0.04f));
            g.drawText (monogram (p->name), inner.withTrimmedBottom (inner.getHeight() * 0.28f), Justification::centred, false);
            g.setColour (c.mute);
            g.setFont (uiFont (11.0f * z));
            g.drawText (tr ("перетащи скрин сюда", "drop a screenshot here"), inner.withTrimmedTop (inner.getHeight() * 0.62f),
                        Justification::centred, true);
        }
        if (dropHighlight)
        {
            g.setColour (c.accent.withAlpha (0.18f));
            g.fillRect (ia);
            g.setColour (Colours::white);
            g.setFont (uiFont (13.0f, true));
            g.drawText (tr ("Отпусти: поставлю картинку", "Drop to set the picture"), ia, Justification::centred, true);
        }
    }

    // format badge
    {
        auto text = p->formatsText();
        if (p->isShellOnly()) text = tr ("WAVES · из браузера DAW", "WAVES · use the DAW's browser");
        g.setFont (uiFont (10.0f * jlimit (0.8f, 1.6f, z), true, 0.08f));
        const float bw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 14.0f;
        const Rectangle<float> badge (8.0f, 8.0f, jmin (bw, (float) getWidth() - 50.0f), 18.0f * jlimit (0.8f, 1.6f, z));
        g.setColour (Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (badge, 5.0f);
        g.setColour (Colour (0xffececf0));
        g.drawText (text, badge, Justification::centred, true);
    }

    // favourite star
    {
        auto star = iconPath (Icon::Star);
        const auto sa = starArea();
        const float ss = sa.getHeight() / 28.0f;
        star.applyTransform (AffineTransform::scale (0.8f * ss).translated (sa.getX() + 5.0f * ss, sa.getY() + 3.0f * ss));
        if (p->favorite)
        {
            g.setColour (c.accent);
            g.fillPath (star);
        }
        else
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillPath (star);
            g.setColour (Colours::white.withAlpha (hover ? 0.7f : 0.25f));
            g.strokePath (star, PathStrokeType (1.2f));
        }
    }

    // sandbox button: crossed circle under the star, always there like the star
    {
        const auto ha = hideArea();
        const float hs = ha.getHeight() / 28.0f;
        const auto ctr = ha.getCentre();
        const float rad = 9.5f * hs;
        Path ban;
        ban.addEllipse (ctr.x - rad, ctr.y - rad, rad * 2.0f, rad * 2.0f);
        const float d = rad * 0.7071f;
        ban.startNewSubPath (ctr.x - d, ctr.y + d);
        ban.lineTo (ctr.x + d, ctr.y - d);
        if (p->hidden)
        {
            g.setColour (c.accent);
            g.fillEllipse (ctr.x - rad - 1.5f, ctr.y - rad - 1.5f, rad * 2.0f + 3.0f, rad * 2.0f + 3.0f);
            g.setColour (c.onAccent);
            g.strokePath (ban, PathStrokeType (2.0f * hs));
        }
        else
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillEllipse (ctr.x - rad, ctr.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (Colours::white.withAlpha (hover ? 0.75f : 0.25f));
            g.strokePath (ban, PathStrokeType (1.3f * hs));
        }
    }

    // text
    auto info = r.withTrimmedTop (imageArea().getHeight()).reduced (12.0f, 8.0f * z);
    g.setColour (c.ink);
    g.setFont (uiFont (14.0f * z, true));
    g.drawText (p->name, info.removeFromTop (20.0f * z), Justification::centredLeft, true);
    auto line = info.removeFromTop (18.0f * z);
    g.setFont (uiFont (11.0f * z));
    const auto last = app.lastUsedText (*p);
    if (last.isNotEmpty())
    {
        const float lw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), last) + 2.0f;
        g.setColour (c.ink);
        g.drawText (last, line.removeFromRight (lw), Justification::centredRight, false);
    }
    if (p->uses > 0)
    {
        const auto uses = String (p->uses) + String::fromUTF8 ("×");
        g.setFont (uiFont (11.0f * z, true));
        const float uw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), uses) + 8.0f;
        g.setColour (c.accentHi);
        g.drawText (uses, line.removeFromRight (uw), Justification::centredRight, false);
        g.setFont (uiFont (11.0f * z));
    }
    else if (! app.lib.projectCache.empty())
    {
        g.setColour (c.mute);
        const auto never = tr ("ни разу", "never");
        const float nw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), never) + 2.0f;
        g.drawText (never, line.removeFromRight (nw), Justification::centredRight, false);
    }
    g.setColour (c.mute);
    auto meta = p->vendor.isNotEmpty() ? p->vendor + String::fromUTF8 ("  ·  ") + p->category() : p->category();
    g.drawText (meta, line.withTrimmedRight (6.0f), Justification::centredLeft, true);

    // outline
    g.setColour (dropHighlight ? c.accent : (hover ? c.accent.withAlpha (0.45f) : c.stroke));
    g.drawRoundedRectangle (r, rad, dropHighlight ? 2.0f : 1.0f);
}

void PluginCard::mouseDown (const MouseEvent& e)
{
    dragStarted = false;
    if (auto* top = findParentComponentOfClass<MainComponent>())
        top->grabKeyboardFocus();
    if (e.mods.isPopupMenu())
        showMenu();
}

void PluginCard::mouseDrag (const MouseEvent& e)
{
    if (dragStarted || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6)
        return;
    dragStarted = true;
    auto* p = plugin();
    if (p == nullptr)
        return;
    const auto* f = p->fileForDrag (app.lib.formatPriority);
    if (f == nullptr)
    {
        app.toast (p->isShellOnly() ? tr ("Waves лежат внутри WaveShell: добавь их из браузера DAW.",
                                          "Waves plugins live inside WaveShell: add them from the DAW's browser.")
                                    : tr ("Этот плагин только в AU: перетащить не выйдет, бери его из браузера DAW.",
                                          "This plugin is AU only: it can't be dragged, use the DAW's browser."));
        return;
    }
    DragAndDropContainer::performExternalDragDropOfFiles ({ dragPathFor (*f).getFullPathName() }, false, this);
}

void PluginCard::mouseUp (const MouseEvent& e)
{
    if (dragStarted || e.mods.isPopupMenu() || e.getDistanceFromDragStart() > 5)
        return;
    if (starArea().contains (e.position))
    {
        if (auto* p = plugin())
        {
            p->favorite = ! p->favorite;
            app.lib.saveUserData();
            repaint();
            if (app.refreshAll) app.refreshAll();
        }
    }
    else if (hideArea().contains (e.position))
    {
        if (auto* p = plugin())
        {
            p->hidden = ! p->hidden;
            const auto name = p->name;
            const bool nowHidden = p->hidden;
            app.lib.saveUserData();
            if (app.toast)
                app.toast (nowHidden ? name + tr (" ушёл в песочницу. Вернуть: фильтр «Песочница».", " went to the sandbox. Bring it back from the Sandbox filter.")
                                     : name + tr (" вернулся в галерею.", " is back in the gallery."));
            auto* appPtr = &app;
            MessageManager::callAsync ([appPtr] { if (appPtr->refreshAll) appPtr->refreshAll(); });   // the card may go away
        }
    }
}

void PluginCard::mouseDoubleClick (const MouseEvent& e)
{
    if (imageArea().contains (e.position) && app.editExistingImage)
        app.editExistingImage (key);
}

void PluginCard::showMenu()
{
    auto* p = plugin();
    if (p == nullptr)
        return;
    PopupMenu m;
    m.addSectionHeader (p->name);
    m.addItem (1, p->favorite ? tr ("Убрать из избранного", "Remove from favourites") : tr ("В избранное", "Add to favourites"));
    m.addItem (5, p->hidden ? tr ("Вернуть из песочницы", "Bring back from the sandbox") : tr ("Не нужен: в песочницу", "Don't need it: to the sandbox"));
    m.addSeparator();
    m.addItem (2, tr ("Выбрать картинку...", "Choose picture..."));
    const bool hasImg = app.lib.originalImageFile (key).existsAsFile();
    m.addItem (3, tr ("Обрезать картинку", "Crop picture"), hasImg);
    m.addItem (4, tr ("Убрать картинку", "Remove picture"), hasImg || app.lib.imageFile (key).existsAsFile());
    m.addSeparator();

    PopupMenu typeMenu;
    typeMenu.addItem (10, tr ("Автоматически", "Automatic"), true, p->userType == PluginType::Unknown);
    typeMenu.addItem (11, tr ("Эффект", "Effect"), true, p->userType == PluginType::Effect);
    typeMenu.addItem (12, tr ("Инструмент", "Instrument"), true, p->userType == PluginType::Instrument);
    m.addSubMenu (tr ("Тип", "Type"), typeMenu);

    PopupMenu catMenu;
    catMenu.addItem (1000, tr ("Автоматически", "Automatic") + " (" + p->autoCategory() + ")", true, p->userCategory.isEmpty());
    const auto cats = allCategories();
    for (int i = 0; i < cats.size(); ++i)
    {
        if (i == builtInCategories().size()) catMenu.addSeparator();
        catMenu.addItem (1001 + i, cats[i], true, p->userCategory == cats[i]);
    }
    catMenu.addSeparator();
    catMenu.addItem (2000, tr ("+ Новая категория...", "+ New category..."));
    if (! app.lib.customCategories.isEmpty())
    {
        PopupMenu del;
        for (int i = 0; i < app.lib.customCategories.size(); ++i)
            del.addItem (2100 + i, app.lib.customCategories[i]);
        catMenu.addSubMenu (tr ("Удалить свою категорию", "Delete my category"), del);
    }
    m.addSubMenu (tr ("Категория", "Category"), catMenu);

    if (p->files.size() > 1)
    {
        PopupMenu fmtMenu;
        fmtMenu.addItem (40, tr ("Как в настройках", "As in settings"), true, p->dragFormat.isEmpty());
        int id = 41;
        for (auto& f : p->files)
        {
            const auto n = formatName (f.format);
            fmtMenu.addItem (id++, n + (f.is32bit ? " (32-bit)" : ""), ! f.isShell, p->dragFormat == n);
        }
        m.addSubMenu (tr ("Тащить формат", "Drag format"), fmtMenu);
    }

    PopupMenu reveal;
    int rid = 60;
    for (auto& f : p->files)
        reveal.addItem (rid++, formatName (f.format) + ":  " + f.file.getFullPathName());
    m.addSubMenu (tr ("Показать файл", "Show file"), reveal);

    SafePointer<PluginCard> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& app = safe->app;
        auto* p = safe->plugin();
        if (p == nullptr)
            return;
        const auto key = safe->key;
        bool changed = true;
        if (r == 1) p->favorite = ! p->favorite;
        else if (r == 5) p->hidden = ! p->hidden;
        else if (r == 2000)
        {
            app.newCategory ([&app, key] (const String& name)
            {
                if (auto* pl = app.lib.find (key))
                {
                    pl->userCategory = name;
                    app.lib.saveUserData();
                    if (app.refreshAll) app.refreshAll();
                }
            });
            changed = false;
        }
        else if (r >= 2100 && r < 2100 + app.lib.customCategories.size())
        {
            app.lib.removeCategory (app.lib.customCategories[r - 2100]);
            if (app.categoriesChanged) app.categoriesChanged();
            changed = false;
        }
        else if (r == 2)
        {
            auto chooser = std::make_shared<FileChooser> (tr ("Картинка для ", "Picture for ") + p->name, File(), "*.png;*.jpg;*.jpeg;*.gif");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [chooser, &app, key] (const FileChooser& fc)
            {
                if (fc.getResult().existsAsFile() && app.editImage)
                    app.editImage (key, fc.getResult());
            });
            changed = false;
        }
        else if (r == 3) { if (app.editExistingImage) app.editExistingImage (key); changed = false; }
        else if (r == 4)
        {
            app.lib.imageFile (key).deleteFile();
            app.lib.originalImageFile (key).deleteFile();
            app.lib.imageStates.erase (key);
            app.thumbs.erase (key);
        }
        else if (r >= 10 && r <= 12) p->userType = (PluginType) (r - 11);
        else if (r == 1000) p->userCategory = {};
        else if (r > 1000 && r < 1001 + allCategories().size()) p->userCategory = allCategories()[r - 1001];
        else if (r == 40) p->dragFormat = {};
        else if (r > 40 && r < 60)
        {
            const auto idx = (size_t) (r - 41);
            if (idx < p->files.size()) p->dragFormat = formatName (p->files[idx].format);
        }
        else if (r >= 60 && r < 1000)
        {
            const auto idx = (size_t) (r - 60);
            if (idx < p->files.size()) p->files[idx].file.revealToUser();
            changed = false;
        }
        if (changed)
        {
            app.lib.saveUserData();
            if (app.refreshAll) app.refreshAll();
        }
    });
}

//==============================================================================
GalleryGrid::GalleryGrid (AppState& a) : app (a) {}

void GalleryGrid::setKeys (const StringArray& keys)
{
    // reuse cards where possible to keep the scroll smooth
    OwnedArray<PluginCard> old;
    old.swapWith (cards);
    for (auto& k : keys)
    {
        PluginCard* card = nullptr;
        for (int i = 0; i < old.size(); ++i)
            if (old[i]->key == k)
            {
                card = old.removeAndReturn (i);
                break;
            }
        if (card == nullptr)
        {
            card = new PluginCard (app, k);
            addAndMakeVisible (card);
        }
        cards.add (card);
    }
    for (auto* c : cards) c->repaint();
    layoutForWidth (getWidth());
}

void GalleryGrid::layoutForWidth (int w)
{
    const float z = app.lib.galleryZoom;
    const int gap = 16, minW = roundToInt (220.0f * z);
    const int cols = jmax (1, (w + gap) / (minW + gap));
    const int cw = (w - gap * (cols - 1)) / cols;
    const int ch = PluginCard::heightForWidth (cw, z);
    for (int i = 0; i < cards.size(); ++i)
        cards[i]->setBounds ((i % cols) * (cw + gap), (i / cols) * (ch + gap), cw, ch);
    const int rows = (cards.size() + cols - 1) / cols;
    setSize (w, jmax (200, rows * (ch + gap)));
}

void GalleryGrid::paint (Graphics& g)
{
    if (cards.isEmpty() && emptyText.isNotEmpty())
    {
        g.setColour (palOf (*this).mute);
        g.setFont (uiFont (14.0f));
        g.drawFittedText (emptyText, getLocalBounds().withHeight (160).reduced (40, 0), Justification::centred, 4);
    }
}

PluginCard* GalleryGrid::cardAt (int x, int y)
{
    for (auto* c : cards)
        if (c->getBounds().contains (x, y))
            return c;
    return nullptr;
}

void GalleryGrid::clearHighlight()
{
    for (auto* c : cards)
        if (c->dropHighlight)
        {
            c->dropHighlight = false;
            c->repaint();
        }
}

bool GalleryGrid::isInterestedInFileDrag (const StringArray& files)
{
    return files.size() == 1 && isImageFile (files[0]);
}

void GalleryGrid::fileDragMove (const StringArray&, int x, int y)
{
    auto* hit = cardAt (x, y);
    for (auto* c : cards)
    {
        const bool h = c == hit;
        if (c->dropHighlight != h)
        {
            c->dropHighlight = h;
            c->repaint();
        }
    }
}

void GalleryGrid::fileDragExit (const StringArray&) { clearHighlight(); }

void GalleryGrid::filesDropped (const StringArray& files, int x, int y)
{
    clearHighlight();
    if (auto* c = cardAt (x, y))
        if (app.editImage)
            app.editImage (c->key, File (files[0]));
}

//==============================================================================
GalleryPage::GalleryPage (AppState& a) : app (a), grid (a)
{
    search.setFont (uiFont (13.0f));
    search.setIndents (34, 0);
    search.setJustification (Justification::centredLeft);
    search.onTextChange = [this] { startTimer (150); };
    search.onEscapeKey = [this] { search.clear(); refresh(); };
    addAndMakeVisible (search);

    int idx = 0;
    for (auto* c : { &all, &inst, &fx, &fav, &noImg, &sandbox })
    {
        const int i = idx++;
        c->setClickingTogglesState (false);
        c->onClick = [this, i] { setTypeFilter (i); };
        addAndMakeVisible (*c);
    }
    all.setToggleState (true, dontSendNotification);

    category.onChange = [this]
    {
        if (category.getSelectedId() == newCategoryId)
        {
            category.setSelectedId (lastCategoryId, dontSendNotification);
            app.newCategory ([this] (const String& name)
            {
                const int idx = allCategories().indexOf (name);
                if (idx >= 0) category.setSelectedId (idx + 2);
            });
            return;
        }
        lastCategoryId = category.getSelectedId();
        refresh();
    };
    sort.onChange = [this] { refresh(); };
    addAndMakeVisible (category);
    addAndMakeVisible (sort);

    view.setViewedComponent (&grid, false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (10);
    view.getVerticalScrollBar().setAutoHide (false);
    addAndMakeVisible (view);
    updateTexts();
}

void GalleryPage::updateTexts()
{
    search.setTextToShowWhenEmpty (tr ("Найти плагин или производителя", "Find a plugin or vendor"), palOf (*this).mute);
    all.setButtonText (tr ("Все", "All"));
    inst.setButtonText (tr ("Инструменты", "Instruments"));
    fx.setButtonText (tr ("Эффекты", "Effects"));
    fav.setButtonText (String::fromUTF8 ("★ ") + tr ("Избранное", "Favourites"));
    noImg.setButtonText (tr ("Без картинки", "No picture"));
    int hiddenCount = 0;
    for (auto& p : app.lib.plugins) if (p.hidden) ++hiddenCount;
    sandbox.setButtonText (tr ("Песочница", "Sandbox") + (hiddenCount > 0 ? " " + String (hiddenCount) : String()));
    sandbox.setTooltip (tr ("Плагины, которые ты пометил как ненужные. В остальной галерее их не видно.",
                            "Plugins you marked as not needed. They don't show up anywhere else in the gallery."));

    const int cat = jmax (1, category.getSelectedId()), srt = jmax (1, sort.getSelectedId());
    category.clear (dontSendNotification);
    category.addItem (tr ("Все категории", "All categories"), 1);
    const auto cats = allCategories();
    for (int i = 0; i < cats.size(); ++i)
    {
        if (i == builtInCategories().size()) category.addSeparator();
        category.addItem (cats[i], i + 2);
    }
    category.addSeparator();
    category.addItem (tr ("+ Новая категория...", "+ New category..."), newCategoryId);
    category.setSelectedId (cat <= cats.size() + 1 ? cat : 1, dontSendNotification);

    sort.clear (dontSendNotification);
    sort.addItem (tr ("По имени", "By name"), 1);
    sort.addItem (tr ("Последние", "Recent"), 2);
    sort.addItem (tr ("Чаще всего", "Most used"), 3);
    sort.addItem (tr ("По производителю", "By vendor"), 4);
    sort.setSelectedId (srt, dontSendNotification);
    resized();
}

void GalleryPage::setTypeFilter (int i)
{
    typeFilter = i;
    int k = 0;
    for (auto* c : { &all, &inst, &fx, &fav, &noImg, &sandbox })
        c->setToggleState (k++ == i, dontSendNotification);
    refresh();
}

void GalleryPage::refresh()
{
    const auto q = search.getText().trim().toLowerCase();
    const int cat = category.getSelectedId();
    std::vector<const Plugin*> items;
    for (const auto& p : app.lib.plugins)
    {
        if (q.isNotEmpty() && ! p.name.toLowerCase().contains (q) && ! p.vendor.toLowerCase().contains (q))
            continue;
        if (typeFilter == 1 && p.type() != PluginType::Instrument) continue;
        if (typeFilter == 2 && p.type() == PluginType::Instrument) continue;
        if (typeFilter == 3 && ! p.favorite) continue;
        if (typeFilter == 4 && app.thumbFor (p.key).isValid()) continue;
        if ((typeFilter == 5) != p.hidden) continue;   // the sandbox shows only hidden ones, everything else never does
        if (cat > 1 && cat != newCategoryId && p.category() != allCategories()[cat - 2]) continue;
        items.push_back (&p);
    }
    const int s = sort.getSelectedId();
    std::stable_sort (items.begin(), items.end(), [s] (const Plugin* a, const Plugin* b)
    {
        if (s == 2 && a->lastUsed != b->lastUsed) return a->lastUsed > b->lastUsed;
        if (s == 3 && a->uses != b->uses) return a->uses > b->uses;
        if (s == 4 && a->vendor.compareIgnoreCase (b->vendor) != 0) return a->vendor.compareIgnoreCase (b->vendor) < 0;
        return a->name.compareNatural (b->name) < 0;
    });
    StringArray keys;
    for (auto* p : items) keys.add (p->key);

    if (typeFilter == 5)
        grid.emptyText = tr ("Песочница пуста. Жми на перечёркнутый кружок на карточке (снизу под звездой), и плагин пропадёт из галереи.",
                             "The sandbox is empty. Press the crossed circle on a card (under the star) and the plugin disappears from the gallery.");
    else if (app.lib.plugins.empty())
        grid.emptyText = tr ("Пока пусто. Жми «Пересканировать» или добавь папки с плагинами во вкладке «Статистика».",
                             "Nothing yet. Press Rescan or add plugin folders on the Stats tab.");
    else
        grid.emptyText = tr ("Ничего не нашлось", "Nothing found");
    grid.setKeys (keys);

    int instCount = 0, withImg = 0, hiddenCount = 0;
    for (auto& p : app.lib.plugins)
    {
        if (p.type() == PluginType::Instrument) ++instCount;
        if (app.thumbFor (p.key).isValid()) ++withImg;
        if (p.hidden) ++hiddenCount;
    }
    const auto sbText = tr ("Песочница", "Sandbox") + (hiddenCount > 0 ? " " + String (hiddenCount) : String());
    if (sandbox.getButtonText() != sbText)
    {
        sandbox.setButtonText (sbText);
        resized();
    }
    status = tr ("Плагинов: ", "Plugins: ") + String ((int) app.lib.plugins.size())
           + tr (" · инструментов ", " · instruments ") + String (instCount)
           + tr (" · с картинкой ", " · with picture ") + String (withImg)
           + tr (" · в песочнице ", " · in the sandbox ") + String (hiddenCount)
           + tr ("   Ctrl+колесо: крупнее / мельче.", "   Ctrl+wheel: bigger / smaller.")
           + tr ("   Хватай карточку и тащи на трек в DAW. Скрин кидай прямо на карточку или жми Ctrl+V над ней.",
                 "   Drag a card onto a track in your DAW. Drop a screenshot on a card, or hover it and press Ctrl+V.");
    repaint();
}

void GalleryPage::resized()
{
    auto r = getLocalBounds().reduced (28, 18);
    auto bar = r.removeFromTop (38);
    const int combo = bar.getWidth() > 1300 ? 170 : 150;
    sort.setBounds (bar.removeFromRight (combo));
    bar.removeFromRight (8);
    category.setBounds (bar.removeFromRight (combo));
    bar.removeFromRight (8);
    int chipsW = 0;
    std::vector<int> widths;
    for (auto* c : { &all, &inst, &fx, &fav, &noImg, &sandbox })
    {
        widths.push_back (GlyphArrangement::getStringWidthInt (uiFont (13.0f, true), c->getButtonText()) + 30);
        chipsW += widths.back() + 6;
    }
    search.setBounds (bar.removeFromLeft (jlimit (150, 280, bar.getWidth() - chipsW - 10)));
    bar.removeFromLeft (10);
    int i = 0;
    for (auto* c : { &all, &inst, &fx, &fav, &noImg, &sandbox })
    {
        c->setBounds (bar.removeFromLeft (widths[(size_t) i++]));
        bar.removeFromLeft (6);
    }
    r.removeFromTop (16);
    r.removeFromBottom (24);
    view.setBounds (r.withTrimmedRight (-12));
    grid.layoutForWidth (r.getWidth() - 4);
}

void GalleryPage::paintOverChildren (Graphics& g)
{
    auto icon = iconPath (Icon::Search);
    const auto sb = search.getBounds();
    icon.applyTransform (AffineTransform::scale (15.0f / 24.0f).translated ((float) sb.getX() + 12.0f, (float) sb.getCentreY() - 7.5f));
    g.setColour (palOf (*this).mute);
    g.strokePath (icon, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void GalleryPage::paint (Graphics& g)
{
    const auto& c = palOf (*this);

    auto foot = getLocalBounds().reduced (28, 0).removeFromBottom (30).withTrimmedBottom (10);
    g.setColour (c.accent);
    g.fillEllipse ((float) foot.getX(), (float) foot.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (c.mute);
    g.setFont (uiFont (11.0f));
    g.drawText (status, foot.withTrimmedLeft (14), Justification::centredLeft, true);
}

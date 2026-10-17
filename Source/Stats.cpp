#include "Ui.h"

using namespace juce;

//==============================================================================
FolderList::FolderList (AppState& a, bool proj) : app (a), projects (proj)
{
    add.getProperties().set ("accentOutline", true);
    add.getProperties().set ("fontSize", 11.0f);
    add.getProperties().set ("pill", false);
    add.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> (projects ? tr ("Папка с проектами", "Projects folder")
                                                          : tr ("Папка с плагинами", "Plugin folder"));
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.isDirectory())
                return;
            folders().addIfNotAlreadyThere (f);
            app.lib.saveSettings();
            repaint();
            if (app.rescan) app.rescan();
        });
    };
    addAndMakeVisible (add);
    updateTexts();
}

void FolderList::updateTexts() { add.setButtonText (tr ("+ Добавить", "+ Add")); }

Array<File>& FolderList::folders() { return projects ? app.lib.projectFolders : app.lib.pluginFolders; }

int FolderList::idealHeight() const
{
    auto& f = projects ? app.lib.projectFolders : app.lib.pluginFolders;
    return 44 + jmax (1, f.size()) * 22 + 40;
}

Rectangle<int> FolderList::rowBounds (int i) const { return { 16, 42 + i * 22, getWidth() - 32, 22 }; }

void FolderList::resized() { add.setBounds (getWidth() - 16 - 86, 12, 86, 24); }

void FolderList::paint (Graphics& g)
{
    const auto& c = palOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c.panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (c.stroke);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
    g.setColour (c.mute);
    g.setFont (uiFont (10.0f, true, 0.12f));
    g.drawText (projects ? tr ("ПАПКИ ПРОЕКТОВ", "PROJECT FOLDERS") : tr ("ПАПКИ ПЛАГИНОВ", "PLUGIN FOLDERS"),
                16, 12, getWidth() - 120, 24, Justification::centredLeft);

    auto& f = projects ? app.lib.projectFolders : app.lib.pluginFolders;
    const auto defaults = projects ? Array<File>() : Scanner::defaultPluginFolders();
    const int maxRows = jmax (1, (getHeight() - 42 - 40) / 22);
    g.setFont (uiFont (11.5f));
    if (f.isEmpty())
    {
        g.setColour (c.mute);
        g.drawText (projects ? tr ("[папка с проектами]", "[your projects folder]") : tr ("нет папок", "no folders"),
                    rowBounds (0), Justification::centredLeft, true);
    }
    for (int i = 0; i < f.size() && i < maxRows; ++i)
    {
        auto rb = rowBounds (i);
        if (i == maxRows - 1 && f.size() > maxRows)
        {
            g.setColour (c.mute);
            g.drawText (tr ("... и ещё ", "... and ") + String (f.size() - i) + tr ("", " more"), rb, Justification::centredLeft, true);
            break;
        }
        const bool isDefault = defaults.contains (f[i]);
        g.setColour (projects || ! isDefault ? c.mute : c.accent);
        g.fillEllipse ((float) rb.getX(), (float) rb.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setColour (c.ink);
        g.drawText (f[i].getFullPathName(), rb.withTrimmedLeft (12).withTrimmedRight (22), Justification::centredLeft, true);
        if (i == hoverRow)
        {
            auto x = iconPath (Icon::Cross);
            x.applyTransform (AffineTransform::scale (12.0f / 24.0f).translated ((float) rb.getRight() - 14.0f, (float) rb.getCentreY() - 6.0f));
            g.setColour (c.accent);
            g.strokePath (x, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    g.setColour (c.mute);
    g.setFont (uiFont (11.0f));
    g.drawFittedText (projects ? tr ("Пока читаю проекты Bitwig (.bwproject), бэкапы пропускаю.", "Reads Bitwig projects (.bwproject) for now, skips backups.")
                               : tr ("Розовые нашёл сам, серые добавил ты. Плагины не запускаю, только смотрю файлы.",
                                     "Pink ones were found automatically. Plugins are never launched, only their files are read."),
                      Rectangle<int> (16, getHeight() - 38, getWidth() - 32, 30), Justification::centredLeft, 2);
}

void FolderList::mouseMove (const MouseEvent& e)
{
    int row = -1;
    for (int i = 0; i < folders().size(); ++i)
        if (rowBounds (i).contains (e.getPosition()))
            row = i;
    if (row != hoverRow)
    {
        hoverRow = row;
        repaint();
    }
}

void FolderList::mouseUp (const MouseEvent& e)
{
    if (hoverRow < 0 || hoverRow >= folders().size())
        return;
    const auto rb = rowBounds (hoverRow);
    if (e.x < rb.getRight() - 20)
        return;
    folders().remove (hoverRow);
    hoverRow = -1;
    app.lib.saveSettings();
    repaint();
    if (app.rescan) app.rescan();
}

//==============================================================================
StatsPage::StatsPage (AppState& a) : app (a), pluginFolders (a, false), projectFolders (a, true)
{
    int i = 0;
    for (auto* c : { &fAll, &fOften, &fStale, &fNever, &fDelete, &fQuarantine })
    {
        const int idx = i++;
        c->onClick = [this, idx] { setFilter (idx); };
        addAndMakeVisible (*c);
    }
    fDelete.getProperties().set ("accentOutline", true);
    fAll.setToggleState (true, dontSendNotification);

    int months[] = { 6, 12, 24 };
    i = 0;
    for (auto* c : { &m6, &m12, &m24 })
    {
        const int m = months[i++];
        c->getProperties().set ("pill", false);
        c->onClick = [this, m]
        {
            app.lib.staleMonths = m;
            app.lib.saveSettings();
            if (app.refreshAll) app.refreshAll();
        };
        addAndMakeVisible (*c);
    }

    list.setModel (this);
    list.setRowHeight (38);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.getViewport()->setScrollBarThickness (10);
    addAndMakeVisible (list);
    addAndMakeVisible (pluginFolders);
    addAndMakeVisible (projectFolders);
    updateTexts();
}

void StatsPage::updateTexts()
{
    fAll.setButtonText (tr ("Все", "All"));
    fOften.setButtonText (tr ("Часто", "Often"));
    fStale.setButtonText (tr ("Давно", "Stale"));
    fNever.setButtonText (tr ("Никогда", "Never"));
    fDelete.setButtonText (tr ("Можно удалять", "Can delete"));
    fQuarantine.setButtonText (tr ("Карантин", "Quarantine"));
    m6.setButtonText (tr ("6 мес.", "6 mo"));
    m12.setButtonText (tr ("1 год", "1 year"));
    m24.setButtonText (tr ("2 года", "2 years"));
    pluginFolders.updateTexts();
    projectFolders.updateTexts();
    resized();
}

void StatsPage::setFilter (int f)
{
    filter = f;
    int k = 0;
    for (auto* c : { &fAll, &fOften, &fStale, &fNever, &fDelete, &fQuarantine })
        c->setToggleState (k++ == f, dontSendNotification);
    if (f == 2) { sortCol = 4; sortAsc = true; }
    else if (f != 5 && sortCol == 4 && sortAsc) { sortCol = 2; sortAsc = false; }
    refresh();
}

void StatsPage::refresh()
{
    const int sm = app.lib.staleMonths;
    m6.setToggleState (sm == 6, dontSendNotification);
    m12.setToggleState (sm == 12, dontSendNotification);
    m24.setToggleState (sm == 24, dontSendNotification);

    rows.clear();
    if (filter == 5)
    {
        for (int i = (int) app.lib.quarantined.size(); --i >= 0;)
            rows.push_back (String (i));
    }
    else
    {
        std::vector<const Plugin*> items;
        for (const auto& p : app.lib.plugins)
        {
            const auto u = app.usage (p);
            if (filter == 1 && u != AppState::Usage::Often) continue;
            if (filter == 2 && u != AppState::Usage::Stale) continue;
            if (filter == 3 && u != AppState::Usage::Never) continue;
            if (filter == 4 && ! app.canDelete (p)) continue;
            items.push_back (&p);
        }
        const int col = sortCol;
        const bool asc = sortAsc;
        std::stable_sort (items.begin(), items.end(), [col, asc] (const Plugin* a, const Plugin* b)
        {
            int cmp = 0;
            switch (col)
            {
                case 0: cmp = a->name.compareNatural (b->name); break;
                case 1: cmp = a->formatsText().compare (b->formatsText()); break;
                case 2: cmp = a->uses - b->uses; break;
                case 3: cmp = a->projects - b->projects; break;
                case 4: cmp = a->lastUsed < b->lastUsed ? -1 : (a->lastUsed > b->lastUsed ? 1 : 0); break;
                case 5:
                {
                    const double ya = jmax (1.0, (Time::getCurrentTime() - a->firstUsed).inDays() / 365.25);
                    const double yb = jmax (1.0, (Time::getCurrentTime() - b->firstUsed).inDays() / 365.25);
                    const double ra = a->uses / ya, rb = b->uses / yb;
                    cmp = ra < rb ? -1 : (ra > rb ? 1 : 0);
                    break;
                }
                default: break;
            }
            if (cmp == 0) return a->name.compareNatural (b->name) < 0;
            return asc ? cmp < 0 : cmp > 0;
        });
        for (auto* p : items) rows.push_back (p->key);
    }
    list.updateContent();
    list.repaint();
    pluginFolders.repaint();
    projectFolders.repaint();
    resized();
    repaint();
}

void StatsPage::columnLayout (int w, int* x) const
{
    // x[0..6]: column starts, x[6] = end of data columns (actions start)
    const bool actions = filter == 4 || filter == 5;
    const float avail = (float) (w - 32 - (actions ? 200 : 0));
    const float weights[] = { 2.3f, 1.0f, 1.6f, 0.8f, 1.1f, 1.6f };
    float total = 0;
    for (auto v : weights) total += v;
    float pos = 16.0f;
    for (int i = 0; i < 6; ++i)
    {
        x[i] = roundToInt (pos);
        pos += avail * weights[i] / total;
    }
    x[6] = roundToInt (pos);
}

void StatsPage::resized()
{
    auto r = getLocalBounds().reduced (24, 18);
    auto sideR = r.removeFromRight (310);
    r.removeFromRight (18);

    // sidebar
    deletePanel = sideR.removeFromTop (128);
    {
        auto b = deletePanel.reduced (16, 0).withTop (deletePanel.getY() + 86).withHeight (28);
        const int w = (b.getWidth() - 12) / 3;
        m6.setBounds (b.removeFromLeft (w)); b.removeFromLeft (6);
        m12.setBounds (b.removeFromLeft (w)); b.removeFromLeft (6);
        m24.setBounds (b);
    }
    sideR.removeFromTop (14);
    projectFolders.setBounds (sideR.removeFromTop (jmin (projectFolders.idealHeight(), 190)));
    sideR.removeFromTop (14);
    pluginFolders.setBounds (sideR.withHeight (jmin (sideR.getHeight(), pluginFolders.idealHeight())));

    // main column
    tilesArea = r.removeFromTop (92);
    r.removeFromTop (14);
    auto chips = r.removeFromTop (34);
    for (auto* c : { &fAll, &fOften, &fStale, &fNever, &fDelete, &fQuarantine })
    {
        const int w = GlyphArrangement::getStringWidthInt (uiFont (12.0f, true), c->getButtonText()) + 30;
        c->getProperties().set ("fontSize", 12.0f);
        c->setBounds (chips.removeFromLeft (w));
        chips.removeFromLeft (8);
    }
    r.removeFromTop (12);
    headerArea = r.removeFromTop (32);
    list.setBounds (r);
}

void StatsPage::paint (Graphics& g)
{
    const auto& c = palOf (*this);

    // tiles
    {
        int totalUses = 0, inUse = 0, deletable = 0;
        const Plugin* leader = nullptr;
        for (auto& p : app.lib.plugins)
        {
            totalUses += p.uses;
            if (p.uses > 0) ++inUse;
            if (app.canDelete (p)) ++deletable;
            if (p.uses > 0 && (leader == nullptr || p.uses > leader->uses)) leader = &p;
        }
        const ProjectRecord* latest = nullptr;
        for (auto& [path, rec] : app.lib.projectCache)
            if (latest == nullptr || rec.modified > latest->modified)
                latest = &rec;

        const bool noProjects = app.lib.projectCache.empty();
        struct Tile { String label, value, sub; bool accent; };
        Tile tiles[4] = {
            { tr ("ПРОЕКТОВ", "PROJECTS"), String (app.lib.projectCount()),
              latest ? File (latest->path).getFileNameWithoutExtension() + String::fromUTF8 (" · ") + Time (latest->modified).formatted ("%d.%m.%Y")
                     : tr ("добавь папку с проектами", "add a projects folder"), false },
            { tr ("ПЛАГИНОВ В ДЕЛЕ", "PLUGINS IN USE"), String (inUse),
              tr ("из ", "of ") + String ((int) app.lib.plugins.size()) + tr (" на компе", " installed"), false },
            { tr ("ВСТАВОК ВСЕГО", "TOTAL INSERTS"), String (totalUses),
              leader ? tr ("лидер: ", "top: ") + leader->name + ", " + String (leader->uses) : String ("-"), false },
            { tr ("МОЖНО УДАЛЯТЬ", "CAN DELETE"), noProjects ? String ("?") : String (deletable),
              noProjects ? tr ("сначала папка проектов", "needs a projects folder")
                         : tr ("ни разу или дольше ", "never, or older than ") + String (app.lib.staleMonths) + tr (" мес.", " mo"), true },
        };
        const int gap = 12;
        const int tw = (tilesArea.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            const auto tr_ = Rectangle<int> (tilesArea.getX() + i * (tw + gap), tilesArea.getY(), tw, tilesArea.getHeight()).toFloat().reduced (0.5f);
            g.setColour (c.panel);
            g.fillRoundedRectangle (tr_, 12.0f);
            g.setColour (tiles[i].accent ? c.accent.withAlpha (0.45f) : c.stroke);
            g.drawRoundedRectangle (tr_, 12.0f, 1.0f);
            auto in = tr_.reduced (16.0f, 12.0f);
            g.setColour (tiles[i].accent ? c.accent : c.mute);
            g.setFont (uiFont (10.0f, true, 0.1f));
            g.drawText (tiles[i].label, in.removeFromTop (14.0f), Justification::centredLeft, true);
            g.setColour (c.ink);
            g.setFont (uiFont (28.0f, true));
            g.drawText (tiles[i].value, in.removeFromTop (36.0f), Justification::centredLeft, true);
            g.setColour (c.mute);
            g.setFont (uiFont (11.0f));
            g.drawText (tiles[i].sub, in, Justification::centredLeft, true);
        }
    }

    // table frame + header
    {
        const auto frame = headerArea.getUnion (list.getBounds()).toFloat().reduced (0.5f);
        g.setColour (c.panel);
        g.fillRoundedRectangle (frame, 12.0f);
        g.setColour (c.stroke);
        g.drawRoundedRectangle (frame, 12.0f, 1.0f);
        g.drawHorizontalLine (headerArea.getBottom() - 1, (float) headerArea.getX(), (float) headerArea.getRight());

        int x[7];
        columnLayout (headerArea.getWidth(), x);
        g.setFont (uiFont (10.0f, true, 0.12f));
        if (filter == 5)
        {
            const char* hru[] = { "ПЛАГИН", "ОТКУДА", "КОГДА" };
            const char* hen[] = { "PLUGIN", "FROM", "WHEN" };
            const int qx[] = { x[0], x[2], x[5] };
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (c.mute);
                g.drawText (tr (hru[i], hen[i]), headerArea.getX() + qx[i], headerArea.getY(), 200, headerArea.getHeight(), Justification::centredLeft);
            }
        }
        else
        {
            const char* hru[] = { "ПЛАГИН", "ФОРМАТ", "РАЗ ВСЕГО", "ПРОЕКТОВ", "ПОСЛЕДНИЙ РАЗ", "ЧАСТОТА" };
            const char* hen[] = { "PLUGIN", "FORMAT", "TIMES USED", "PROJECTS", "LAST USED", "FREQUENCY" };
            for (int i = 0; i < 6; ++i)
            {
                String label = tr (hru[i], hen[i]);
                if (i == sortCol) label += sortAsc ? String::fromUTF8 (" ▲") : String::fromUTF8 (" ▼");
                g.setColour (i == sortCol ? c.accent : c.mute);
                g.drawText (label, headerArea.getX() + x[i], headerArea.getY(), x[i + 1] - x[i] - 6, headerArea.getHeight(), Justification::centredLeft, true);
            }
        }
        if (rows.empty())
        {
            g.setColour (c.mute);
            g.setFont (uiFont (13.0f));
            String msg;
            if (filter == 5) msg = tr ("Карантин пуст. Сюда попадают плагины, которые ты убрал из папок: их можно вернуть одной кнопкой.",
                                       "Quarantine is empty. Plugins you move out end up here and can be restored with one click.");
            else if (app.lib.projectCache.empty() && filter != 0)
                msg = tr ("Добавь справа папку с проектами, и я посчитаю, что ты реально используешь.",
                          "Add your projects folder on the right and I'll count what you actually use.");
            else msg = tr ("Тут пусто", "Nothing here");
            g.drawFittedText (msg, list.getBounds().reduced (30, 20).withHeight (60), Justification::centred, 3);
        }
    }

    // can-delete panel
    {
        const auto pr = deletePanel.toFloat().reduced (0.5f);
        g.setColour (c.panel);
        g.fillRoundedRectangle (pr, 12.0f);
        g.setColour (c.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (pr, 12.0f, 1.0f);
        g.setColour (c.accent);
        g.setFont (uiFont (10.0f, true, 0.12f));
        g.drawText (tr ("МОЖНО УДАЛЯТЬ", "CAN DELETE"), deletePanel.getX() + 16, deletePanel.getY() + 12, 260, 18, Justification::centredLeft);
        g.setColour (c.ink);
        g.setFont (uiFont (12.0f));
        g.drawFittedText (tr ("Стоит на компе, но ни разу не встретился в проектах. Или не трогал дольше, чем:",
                              "Installed but never used in a project. Or untouched for longer than:"),
                          Rectangle<int> (deletePanel.getX() + 16, deletePanel.getY() + 34, deletePanel.getWidth() - 32, 46),
                          Justification::topLeft, 3);
    }
}

void StatsPage::mouseUp (const MouseEvent& e)
{
    if (filter == 5 || ! headerArea.contains (e.getPosition()))
        return;
    int x[7];
    columnLayout (headerArea.getWidth(), x);
    const int lx = e.x - headerArea.getX();
    for (int i = 0; i < 6; ++i)
        if (lx >= x[i] && lx < x[i + 1])
        {
            if (sortCol == i) sortAsc = ! sortAsc;
            else { sortCol = i; sortAsc = (i == 0 || i == 1); }
            refresh();
            return;
        }
}

int StatsPage::getNumRows() { return (int) rows.size(); }

void StatsPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size())
        return;
    const auto& c = palOf (*this);
    g.setColour (c.stroke.withMultipliedAlpha (0.6f));
    g.drawHorizontalLine (h - 1, 16.0f, (float) w - 16.0f);
    int x[7];
    columnLayout (w, x);

    if (filter == 5)
    {
        const int idx = rows[(size_t) row].getIntValue();
        if (idx < 0 || idx >= (int) app.lib.quarantined.size())
            return;
        const auto& q = app.lib.quarantined[(size_t) idx];
        g.setColour (c.ink);
        g.setFont (uiFont (13.0f, true));
        g.drawText (q.name, x[0], 0, x[2] - x[0] - 8, h, Justification::centredLeft, true);
        g.setColour (c.mute);
        g.setFont (uiFont (11.0f));
        g.drawText (q.from, x[2], 0, x[5] - x[2] - 8, h, Justification::centredLeft, true);
        g.drawText (Time (q.when).formatted ("%d.%m.%Y"), x[5], 0, x[6] - x[5], h, Justification::centredLeft, true);
        return;
    }

    const auto* p = app.lib.find (rows[(size_t) row]);
    if (p == nullptr)
        return;
    g.setColour (c.ink);
    g.setFont (uiFont (13.0f, true));
    g.drawText (p->name, x[0], 0, x[1] - x[0] - 8, h, Justification::centredLeft, true);
    g.setColour (c.mute);
    g.setFont (uiFont (11.0f, false, 0.06f));
    g.drawText (p->formatsText(), x[1], 0, x[2] - x[1] - 6, h, Justification::centredLeft, true);

    int maxUses = 1;
    for (auto& q : app.lib.plugins) maxUses = jmax (maxUses, q.uses);
    g.setColour (c.ink);
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (p->uses), x[2], 0, 30, h, Justification::centredLeft);
    const auto bar = Rectangle<float> ((float) x[2] + 34.0f, (float) h * 0.5f - 3.0f, (float) (x[3] - x[2] - 50), 6.0f);
    g.setColour (c.bg.darker (0.5f));
    g.fillRoundedRectangle (bar, 3.0f);
    if (p->uses > 0)
    {
        g.setColour (c.accent);
        g.fillRoundedRectangle (bar.withWidth (jmax (6.0f, bar.getWidth() * (float) p->uses / (float) maxUses)), 3.0f);
    }
    g.setColour (c.ink);
    g.setFont (uiFont (13.0f));
    g.drawText (String (p->projects), x[3], 0, x[4] - x[3], h, Justification::centredLeft);
    g.drawText (app.lastUsedText (*p, true).isNotEmpty() ? app.lastUsedText (*p, true) : String ("-"), x[4], 0, x[5] - x[4], h, Justification::centredLeft);

    const auto freq = app.frequencyText (*p);
    const auto u = app.usage (*p);
    g.setFont (uiFont (11.0f, true));
    const float fw = jmin ((float) (x[6] - x[5] - 6), (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), freq) + 18.0f);
    const Rectangle<float> pill ((float) x[5], (float) h * 0.5f - 10.0f, fw, 20.0f);
    const auto pc = u == AppState::Usage::Often ? c.accent : (u == AppState::Usage::Never ? c.mute : c.accentHi);
    g.setColour (pc.withAlpha (u == AppState::Usage::Often ? 0.25f : 0.12f));
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (u == AppState::Usage::Never ? c.mute : c.accentHi);
    g.drawText (freq, pill, Justification::centred, true);
}

namespace
{
struct RowActions : public Component
{
    IconButton a { Icon::Reveal }, b { Icon::Box };
    std::function<void()> onA, onB;
    RowActions()
    {
        a.accentIcon = false;
        b.primary = true;
        a.onClick = [this] { if (onA) onA(); };
        b.onClick = [this] { if (onB) onB(); };
        addAndMakeVisible (a);
        addAndMakeVisible (b);
        setInterceptsMouseClicks (false, true);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 5);
        r.removeFromRight (14);
        const int bw = b.idealWidth();
        b.setBounds (r.removeFromRight (bw));
        r.removeFromRight (6);
        if (a.isVisible())
            a.setBounds (r.removeFromRight (34));
    }
};
}

Component* StatsPage::refreshComponentForRow (int row, bool, Component* existing)
{
    if ((filter != 4 && filter != 5) || row < 0 || row >= (int) rows.size())
    {
        delete existing;
        return nullptr;
    }
    auto* ra = dynamic_cast<RowActions*> (existing);
    if (ra == nullptr)
    {
        delete existing;
        ra = new RowActions();
    }
    const auto id = rows[(size_t) row];
    if (filter == 4)
    {
        ra->a.setVisible (true);
        ra->a.setTooltip (tr ("Показать в проводнике", "Show in file browser"));
        ra->b.setButtonText (tr ("В карантин", "Quarantine"));
        ra->b.icon = Icon::Box;
        ra->onA = [this, id]
        {
            if (auto* p = app.lib.find (id))
                if (! p->files.empty())
                    p->files.front().file.revealToUser();
        };
        ra->onB = [this, id] { if (app.quarantine) app.quarantine (id); };
    }
    else
    {
        ra->a.setVisible (false);
        ra->b.setButtonText (tr ("Вернуть", "Restore"));
        ra->b.icon = Icon::Back;
        const int idx = id.getIntValue();
        ra->onB = [this, idx] { if (app.restore) app.restore (idx); };
    }
    ra->resized();
    return ra;
}

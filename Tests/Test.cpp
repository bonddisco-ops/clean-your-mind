// Offline test: builds a fake plugin folder, reads a real Bitwig project, renders every screen to PNG.
// Usage: CYMTest <out dir> <project.bwproject> [image.png]
#include "Ui.h"

using namespace juce;

static void writeFakeVst2 (const File& f)
{
    MemoryBlock mb (0x600, true);
    auto* d = static_cast<uint8*> (mb.getData());
    auto w16 = [d] (int o, uint16 v) { d[o] = (uint8) v; d[o + 1] = (uint8) (v >> 8); };
    auto w32 = [d] (int o, uint32 v) { for (int i = 0; i < 4; ++i) d[o + i] = (uint8) (v >> (8 * i)); };
    d[0] = 'M'; d[1] = 'Z';
    w32 (0x3c, 0x40);
    memcpy (d + 0x40, "PE\0\0", 4);
    const int coff = 0x44;
    w16 (coff, 0x8664);
    w16 (coff + 2, 1);
    w16 (coff + 16, 240);
    const int opt = coff + 20;
    w16 (opt, 0x20b);
    w32 (opt + 112, 0x1000);
    w32 (opt + 116, 0x100);
    const int sec = opt + 240;
    memcpy (d + sec, ".edata\0\0", 8);
    w32 (sec + 8, 0x200);
    w32 (sec + 12, 0x1000);
    w32 (sec + 16, 0x200);
    w32 (sec + 20, 0x400);
    w32 (0x400 + 24, 1);
    w32 (0x400 + 32, 0x1040);
    w32 (0x440, 0x1060);
    memcpy (d + 0x460, "VSTPluginMain", 14);
    f.getParentDirectory().createDirectory();
    f.replaceWithData (mb.getData(), mb.getSize());
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "usage: CYMTest <outdir> <project> [image]\n";
        return 1;
    }
    ScopedJuceInitialiser_GUI gui;
    const File out (argv[1]);
    out.createDirectory();

    // 1. project parsing
    ProjectRecord rec;
    const bool ok = Scanner::parseProject (File (argv[2]), rec);
    int total = 0;
    std::cout << "parse ok=" << ok << "\n";
    for (auto& [k, n] : rec.uses)
    {
        std::cout << "  " << k << " x" << n << "  (" << rec.names[k] << ")\n";
        total += n;
    }
    std::cout << "total inserts " << total << "\n";

    // 2. cleanName / keys
    for (auto* s : { "Kickstart-64bit", "OTT_x64", "SRX ORCHESTRA(VST3 64bit)", "WaveShaperCM x64", "FabFilter Pro-Q 3", "Serum2", "s(M)exoscope" })
        std::cout << "  clean '" << s << "' -> '" << Scanner::cleanName (s) << "'  key " << Scanner::keyFor (Scanner::cleanName (s)) << "\n";

    // 3. fake plugin folders
    const auto root = out.getChildFile ("fakeplugins");
    root.deleteRecursively();
    const auto vst3 = root.getChildFile ("VST3"), clap = root.getChildFile ("CLAP"), vst2 = root.getChildFile ("VstPlugins");
    for (auto* n : { "FabFilter/FabFilter Pro-Q 3.vst3", "Serum2.vst3", "ValhallaDSP/ValhallaRoom.vst3", "Cableguys/ShaperBox 3.vst3",
                     "LiquidSonics/Cinematic Rooms Professional.vst3", "jv880.vst3", "Vital.vst3", "iZotope/Ozone 11 Imager.vst3",
                     "FabFilter/FabFilter Pro-L 2.vst3", "Voxengo/SPAN Plus.vst3", "Old Free Reverb.vst3", "WaveShell1-VST3 15.0_x64.vst3",
                     "Primal Rage Lab/Voice Fucker.vst3", "Primal Rage Lab/Crumble.vst3" })
        vst3.getChildFile (n).getChildFile ("Contents").createDirectory();
    vst3.getChildFile ("Serum2.vst3/Contents/Resources").createDirectory();
    vst3.getChildFile ("Serum2/Contents/x86_64-win").createDirectory();   // stale leftover folder, must be ignored
    vst3.getChildFile ("Serum2/Contents/x86_64-win/Serum2.vst3").create();
    vst3.getChildFile ("Serum2.vst3/Contents/Resources/moduleinfo.json").replaceWithText (
        R"({ "Name": "Serum2", "Factory Info": { "Vendor": "Xfer Records", }, "Classes": [ { "CID": "x", "Category": "Audio Module Class", "Name": "Serum 2", "Vendor": "Xfer Records", "Sub Categories": [ "Instrument", "Synth", ], }, ], })");
    for (auto* n : { "FabFilter/FabFilter Pro-Q 4.clap", "FabFilter/FabFilter Pro-Q 3.clap" })
        clap.getChildFile (n).create();
    for (auto* n : { "SoundToys/x64/Decapitator.dll", "Nicky Romero Kickstart x64/Kickstart-64bit.dll", "OTT/OTT_x64.dll",
                     "SugarBytes/Turnado.dll", "Cableguys/WaveShaperCM x64.dll", "Unused Crusher x64.dll" })
        writeFakeVst2 (vst2.getChildFile (n));
    vst2.getChildFile ("SoundToys/x64/NotAPlugin.dll").replaceWithText ("hello");

    bool is32 = false;
    std::cout << "dll check: " << Scanner::isVst2Dll (vst2.getChildFile ("OTT/OTT_x64.dll"), is32) << " / not-plugin: "
              << Scanner::isVst2Dll (vst2.getChildFile ("SoundToys/x64/NotAPlugin.dll"), is32) << "\n";

    const auto projects = out.getChildFile ("projects");
    projects.getChildFile ("Tech").createDirectory();
    File (argv[2]).copyFileTo (projects.getChildFile ("Tech/Tech.bwproject"));
    projects.getChildFile ("Tech/Tech.bwproject").setLastModificationTime (Time (2025, 5, 6, 2, 46));
    projects.getChildFile ("Tech/backups").createDirectory();
    File (argv[2]).copyFileTo (projects.getChildFile ("Tech/backups/Tech-backup.bwproject"));

    // settings for the app under test
    const auto settings = File::getSpecialLocation (File::userApplicationDataDirectory)
                              .getChildFile ("Primal Rage Lab").getChildFile ("Clean Your Mind");
    settings.deleteRecursively();
    settings.createDirectory();
    String extra;
    for (int i = 1; i <= 6; ++i)
    {
        const auto d = root.getChildFile ("More Plugins " + String (i) + "/Some/Deep/Vendor Folder/With A Long Name");
        d.createDirectory();
        extra << ",\"" << d.getFullPathName() << "\"";
    }
    settings.getChildFile ("settings.json").replaceWithText (
        "{\"pluginFolders\":[\"" + vst3.getFullPathName() + "\",\"" + clap.getFullPathName() + "\",\"" + vst2.getFullPathName()
        + "\"" + extra + "],\"projectFolders\":[\"" + projects.getFullPathName() + "\"],\"theme\":\"Asphalt\",\"lang\":\"" + (argc > 4 ? argv[4] : "ru")
        + "\",\"categories\":[\"" + String::fromUTF8 ("Для вокала") + "\"]}");
    settings.getChildFile ("userdata.json").replaceWithText (String::fromUTF8 (
        "{\"fabfilterproq4\":{\"fav\":true},\"ott\":{\"fav\":true},\"vital\":{\"hid\":true},\"oldfreereverb\":{\"hid\":true},"
        "\"valhallaroom\":{\"cat\":\"Для вокала\"}}"));

    // 4. the app
    auto mc = std::make_unique<MainComponent>();
    mc->setSize (1240, 860);
    for (int i = 0; i < 40; ++i)
        MessageManager::getInstance()->runDispatchLoopUntil (100);

    auto snap = [&] (const String& name)
    {
        for (int i = 0; i < 5; ++i)
            MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto img = mc->createComponentSnapshot (mc->getLocalBounds(), true, 1.0f);
        out.getChildFile (name).deleteFile();
        if (auto os = out.getChildFile (name).createOutputStream())
            PNGImageFormat().writeImageToStream (img, *os);
        std::cout << "wrote " << name << "\n";
    };

    // give one card a picture through the crop editor
    if (argc > 3)
    {
        const auto picture = ImageFileFormat::loadFrom (File (argv[3]));
        std::cout << "picture valid " << picture.isValid() << "\n";
    }
    snap ("gallery.png");

    // find tabs by text and click them
    std::function<Button* (Component&, const String&)> findButton = [&] (Component& c, const String& text) -> Button*
    {
        for (auto* ch : c.getChildren())
        {
            if (auto* b = dynamic_cast<Button*> (ch))
                if (b->getButtonText() == text)
                    return b;
            if (auto* r = findButton (*ch, text))
                return r;
        }
        return nullptr;
    };
    if (auto* b = findButton (*mc, tr ("Статистика", "Stats"))) b->triggerClick();
    snap ("stats.png");
    if (auto* b = findButton (*mc, tr ("Можно удалять", "Can delete"))) b->triggerClick();
    snap ("stats-delete.png");
    if (auto* b = findButton (*mc, tr ("Галерея", "Gallery"))) b->triggerClick();

    if (argc > 3)
    {
        // simulate a dropped screenshot on the Serum card
        std::function<GalleryGrid* (Component&)> findGrid = [&] (Component& c) -> GalleryGrid*
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* gg = dynamic_cast<GalleryGrid*> (ch)) return gg;
                if (auto* r = findGrid (*ch)) return r;
            }
            return nullptr;
        };
        if (auto* grid = findGrid (*mc))
        {
            for (auto* ch : grid->getChildren())
                if (auto* card = dynamic_cast<PluginCard*> (ch))
                    if (card->key == "serum2")
                        grid->filesDropped ({ String (argv[3]) }, card->getBounds().getCentreX(), card->getBounds().getCentreY());
        }
        snap ("crop.png");
        if (auto* b = findButton (*mc, tr ("Сохранить", "Save"))) b->triggerClick();
        snap ("gallery-with-picture.png");
    }

    // ---- sandbox, categories, zoom, folder list
    std::function<GalleryGrid* (Component&)> findGrid2 = [&] (Component& c) -> GalleryGrid*
    {
        for (auto* ch : c.getChildren())
        {
            if (auto* gg = dynamic_cast<GalleryGrid*> (ch)) return gg;
            if (auto* r = findGrid2 (*ch)) return r;
        }
        return nullptr;
    };
    auto cardKeys = [&]
    {
        StringArray k;
        if (auto* grid = findGrid2 (*mc))
            for (auto* ch : grid->getChildren())
                if (auto* card = dynamic_cast<PluginCard*> (ch)) k.add (card->key);
        return k;
    };
    int failures = 0;
    auto check = [&] (bool ok, const String& what) { std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n"; if (! ok) ++failures; };
    {
        const auto k = cardKeys();
        check (! k.contains ("vital") && ! k.contains ("oldfreereverb") && k.contains ("serum2"), "sandboxed plugins are not in the gallery (" + String (k.size()) + " cards)");
        if (auto* b = findButton (*mc, tr ("Песочница", "Sandbox") + " 2")) b->triggerClick();
        else check (false, "sandbox chip with a count");
        snap ("gallery-sandbox.png");
        const auto s = cardKeys();
        check (s.size() == 2 && s.contains ("vital"), "sandbox shows only the hidden ones");
        if (auto* b = findButton (*mc, tr ("Все", "All"))) b->triggerClick();
        check (allCategories().contains (String::fromUTF8 ("Для вокала")), "custom category loaded");
    }
    mc->zoomBy (1.7f);
    snap ("gallery-zoom.png");
    mc->zoomBy (1.0f, true);
    if (auto* b = findButton (*mc, tr ("Статистика", "Stats"))) b->triggerClick();
    snap ("stats-folders.png");
    {
        FolderList* plugins = nullptr;
        std::function<void (Component&)> collect = [&] (Component& c)
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* fl = dynamic_cast<FolderList*> (ch)) plugins = fl;   // the plugin list is added last
                collect (*ch);
            }
        };
        collect (*mc);
        if (plugins != nullptr)
        {
            const int before = plugins->getHeight();
            auto src = Desktop::getInstance().getMainMouseSource();
            const Point<float> pos (40.0f, 42.0f + 4.5f * 22.0f);
            MouseEvent me (src, pos, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, plugins, plugins, Time::getCurrentTime(), pos, Time::getCurrentTime(), 1, false);
            plugins->mouseMove (me);
            plugins->mouseUp (me);
            snap ("stats-folders-open.png");
            check (plugins->getHeight() > before + 60, "the folder list opens to show every folder (" + String (before) + " -> " + String (plugins->getHeight()) + ")");
        }
        else check (false, "folder list found");
    }
    mc->zoomBy (1.45f);
    snap ("stats-zoom.png");
    mc->zoomBy (1.0f, true);
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + String (failures)) << "\n";

    Library lib;
    ScanResult sr;
    sr.files = Scanner::scanPlugins (lib.pluginFolders, [] { return false; });
    sr.projects = Scanner::scanProjects (lib.projectFolders, {}, [] { return false; });
    std::cout << "projects found " << sr.projects.size() << "\n";
    lib.applyScan (std::move (sr));
    for (const auto& p : lib.plugins)
        std::cout << "  card " << p.key << " | " << p.name << " | " << p.vendor << " | " << p.formatsText() << " | " << p.category() << " | uses " << p.uses << "\n";

    mc.reset();
    return 0;
}

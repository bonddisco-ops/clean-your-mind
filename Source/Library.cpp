#include "Library.h"
#include <regex>
#include <set>

using namespace juce;

String formatName (Format f)
{
    switch (f)
    {
        case Format::VST3: return "VST3";
        case Format::VST2: return "VST2";
        case Format::CLAP: return "CLAP";
        case Format::AU:   return "AU";
    }
    return {};
}

static Format formatFromName (const String& s)
{
    if (s == "VST2") return Format::VST2;
    if (s == "CLAP") return Format::CLAP;
    if (s == "AU")   return Format::AU;
    return Format::VST3;
}

const StringArray& allCategories()
{
    static const StringArray c { String::fromUTF8 ("Синты"), "EQ", String::fromUTF8 ("Динамика"), String::fromUTF8 ("Грязь"),
                                 String::fromUTF8 ("Модуляция"), String::fromUTF8 ("Пространство"), String::fromUTF8 ("Анализ"),
                                 String::fromUTF8 ("Другое") };
    return c;
}

//==============================================================================
static bool containsAny (const String& lower, std::initializer_list<const char*> words)
{
    for (auto* w : words)
        if (lower.contains (w))
            return true;
    return false;
}

PluginType Plugin::type() const
{
    if (userType != PluginType::Unknown)
        return userType;
    for (const auto& f : files)
        if (f.type != PluginType::Unknown)
            return f.type;
    const auto n = name.toLowerCase();
    if (containsAny (n, { "serum", "synth", "vital", "massive", "sylenth", "spire", "kontakt", "omnisphere", "pigments",
                          "diva", "jv880", "jv-880", "srx", "piano", "sampler", "phase plant", "nexus", "keyscape", "dexed",
                          "surge", "repro", "hive", "zebra", "analog lab", "battery", "drum machine", "addictive" }))
        return PluginType::Instrument;
    return PluginType::Effect;
}

String Plugin::autoCategory() const
{
    if (type() == PluginType::Instrument)
        return allCategories()[0];

    for (const auto& f : files)
        for (const auto& s : f.subCategories)
        {
            const auto l = s.toLowerCase();
            if (l == "eq") return "EQ";
            if (l == "dynamics") return allCategories()[2];
            if (l == "distortion") return allCategories()[3];
            if (l == "modulation" || l == "pitch shift") return allCategories()[4];
            if (l == "reverb" || l == "delay" || l == "spatial") return allCategories()[5];
            if (l == "analyzer") return allCategories()[6];
        }

    const auto n = name.toLowerCase();
    if (containsAny (n, { "span", "meter", "scope", "analy", "metric", "tonal balance", "insight", "spectrum", "correlat" }))
        return allCategories()[6];
    if (containsAny (n, { "pro-q", " eq", "eq ", "equilib", "equaliz", "pultec", "kirchhoff", "eq8" }) || n.startsWith ("eq"))
        return "EQ";
    if (containsAny (n, { "shaperbox", "turnado", "pancake", "chorus", "flang", "phaser", "tremol", "lfo", "alterboy",
                          "pitch", "dimension", "vibrato", "glitch", "stutter", "gross beat", "effectrix" }))
        return allCategories()[4];
    if (containsAny (n, { "comp", "limit", "pro-l", "pro-c", "pro-mb", "ott", "dynamic", "kickstart", "duck", "gate",
                          "clipper", "transient", "volume", "maximiz", "multiband", "glue", "squash" }))
        return allCategories()[2];
    if (containsAny (n, { "decap", "crush", "distort", "satur", "corrosion", "waveshaper", "drive", "fuzz", "bitcr", "tape",
                          "studer", "camel", "trash", "saturn", "devil", "heat", "tube", "crumble", "destroy", "dirt" }))
        return allCategories()[3];
    if (containsAny (n, { "reverb", "verb", "room", "delay", "echo", "valhalla", "cinematic", "imager", "stereo", "width",
                          "space", "spatial", "hall", "plate", "shimmer", "microshift", "haas", "wider" }))
        return allCategories()[5];
    return allCategories()[7];
}

String Plugin::category() const { return userCategory.isNotEmpty() ? userCategory : autoCategory(); }

bool Plugin::hasFormat (Format f) const
{
    for (const auto& pf : files)
        if (pf.format == f)
            return true;
    return false;
}

bool Plugin::isShellOnly() const
{
    for (const auto& f : files)
        if (! f.isShell)
            return false;
    return ! files.empty();
}

const PluginFile* Plugin::fileForDrag (const StringArray& priority) const
{
    StringArray order;
    if (dragFormat.isNotEmpty())
        order.add (dragFormat);
    order.addArray (priority);
    order.removeDuplicates (false);

    for (int pass = 0; pass < 2; ++pass)          // 64-bit first, then anything
        for (const auto& fmt : order)
            for (const auto& f : files)
                if (! f.isShell && formatName (f.format) == fmt && (pass == 1 || ! f.is32bit))
                    return &f;
    return nullptr;
}

String Plugin::formatsText() const
{
    StringArray s;
    for (auto f : { Format::VST3, Format::CLAP, Format::VST2, Format::AU })
        if (hasFormat (f))
            s.add (formatName (f));
    return s.joinIntoString (" · ");
}

//==============================================================================
namespace Scanner
{
String cleanName (const String& stemIn)
{
    std::string s = stemIn.toStdString();
    static const std::regex bracket (R"(\s*[\(\[]\s*(vst3?|x64|x86_64|x86|64[ -]?bit|vst3? 64[ -]?bit|win64|64|aax)\s*[\)\]])", std::regex::icase);
    static const std::regex tail (R"([ _\-\.](x64|x86_64|x86|64bit|64-bit|win64|vst3?|clap|mac|osx)$)", std::regex::icase);
    s = std::regex_replace (s, bracket, "");
    for (int i = 0; i < 3; ++i)
        s = std::regex_replace (s, tail, "");
    String r = String::fromUTF8 (s.c_str()).replaceCharacter ('_', ' ').trim();
    while (r.contains ("  "))
        r = r.replace ("  ", " ");
    return r.isNotEmpty() ? r : stemIn;
}

String keyFor (const String& name)
{
    String k;
    for (auto c : name.toLowerCase())
        if (CharacterFunctions::isLetterOrDigit (c))
            k += c;
    return k;
}

String keyForFileName (const String& fileName)
{
    return keyFor (cleanName (File::createFileWithoutCheckingPath ("/" + fileName).getFileNameWithoutExtension()));
}

//------------------------------------------------------------------------------
static uint32 rd32 (const uint8* p) { return (uint32) p[0] | ((uint32) p[1] << 8) | ((uint32) p[2] << 16) | ((uint32) p[3] << 24); }
static uint16 rd16 (const uint8* p) { return (uint16) (p[0] | (p[1] << 8)); }

bool isVst2Dll (const File& f, bool& is32bit)
{
    FileInputStream in (f);
    if (! in.openedOk())
        return false;
    HeapBlock<uint8> head (65536, true);
    const auto got = in.read (head.get(), 65536);
    if (got < 512 || head[0] != 'M' || head[1] != 'Z')
        return false;
    const auto pe = (int) rd32 (head + 0x3c);
    if (pe <= 0 || pe + 24 >= got || memcmp (head + pe, "PE\0\0", 4) != 0)
        return false;
    const auto numSections = rd16 (head + pe + 6);
    const auto optSize = rd16 (head + pe + 20);
    const auto opt = pe + 24;
    if (opt + optSize > got)
        return false;
    const auto magic = rd16 (head + opt);
    is32bit = magic == 0x10b;
    const int dirOffset = opt + (is32bit ? 96 : 112);
    const auto exportRva = rd32 (head + dirOffset);
    if (exportRva == 0)
        return false;

    const int secTable = opt + optSize;
    auto rvaToOffset = [&] (uint32 rva) -> int64
    {
        for (int i = 0; i < numSections; ++i)
        {
            const auto* s = head + secTable + i * 40;
            if (secTable + i * 40 + 40 > got) break;
            const auto va = rd32 (s + 12), vsize = jmax (rd32 (s + 8), rd32 (s + 16)), raw = rd32 (s + 20);
            if (rva >= va && rva < va + vsize)
                return (int64) raw + (rva - va);
        }
        return -1;
    };

    auto readAt = [&] (int64 pos, void* dst, int n)
    {
        return in.setPosition (pos) && in.read (dst, n) == n;
    };

    uint8 dir[40];
    const auto dirPos = rvaToOffset (exportRva);
    if (dirPos < 0 || ! readAt (dirPos, dir, 40))
        return false;
    const auto numNames = jmin<uint32> (rd32 (dir + 24), 4096);
    const auto namesPos = rvaToOffset (rd32 (dir + 32));
    if (namesPos < 0)
        return false;
    HeapBlock<uint8> rvas (numNames * 4 + 4, true);
    if (! readAt (namesPos, rvas.get(), (int) numNames * 4))
        return false;
    for (uint32 i = 0; i < numNames; ++i)
    {
        const auto p = rvaToOffset (rd32 (rvas + i * 4));
        char nm[32] = {};
        if (p < 0 || ! in.setPosition (p))
            continue;
        in.read (nm, 31);
        if (strcmp (nm, "VSTPluginMain") == 0 || strcmp (nm, "main") == 0 || strcmp (nm, "VstPluginMain") == 0)
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
static void readVst3Info (PluginFile& pf)
{
    const auto info = pf.file.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("moduleinfo.json");
    if (! info.existsAsFile())
        return;
    std::string text = info.loadFileAsString().toStdString();
    static const std::regex trailing (R"(,(\s*[\}\]]))");
    text = std::regex_replace (text, trailing, "$1");
    const auto v = JSON::parse (String::fromUTF8 (text.c_str()));
    if (! v.isObject())
        return;
    if (auto* fi = v.getProperty ("Factory Info", {}).getDynamicObject())
        if (fi->getProperty ("Vendor").toString().isNotEmpty())
            pf.vendor = fi->getProperty ("Vendor").toString();
    if (auto* classes = v.getProperty ("Classes", {}).getArray())
    {
        for (const auto& c : *classes)
        {
            if (c.getProperty ("Category", {}).toString() != "Audio Module Class")
                continue;
            if (pf.name.isEmpty() || classes->size() > 0)
                if (c.getProperty ("Name", {}).toString().isNotEmpty() && pf.subCategories.isEmpty())
                    pf.name = c.getProperty ("Name", {}).toString();
            const auto sub = c.getProperty ("Sub Categories", {});
            StringArray subs;
            if (auto* arr = sub.getArray())
                for (const auto& s : *arr) subs.add (s.toString());
            else
                subs.addTokens (sub.toString(), "|", "");
            if (pf.subCategories.isEmpty())
                pf.subCategories = subs;
            if (subs.contains ("Instrument", true) || subs.contains ("Synth", true) || subs.contains ("Sampler", true))
                pf.type = PluginType::Instrument;
            else if (subs.contains ("Fx", true) && pf.type == PluginType::Unknown)
                pf.type = PluginType::Effect;
            if (pf.vendor.isEmpty())
                pf.vendor = c.getProperty ("Vendor", {}).toString();
        }
    }
}

static String guessVendor (const File& file, const File& root)
{
    static const StringArray generic { "x64", "x86", "x86_64", "64-bit", "64bit", "32-bit", "win64", "vstplugins", "vst plugins",
                                       "vst", "vst2", "vst3", "clap", "plugins", "plug-ins", "components", "common files",
                                       "program files", "program files (x86)", "steinberg", "c", "d", "contents", "vst mega" };
    for (auto dir = file.getParentDirectory(); dir != root && dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
    {
        if (dir.isAChildOf (root) == false)
            break;
        auto n = dir.getFileName();
        if (generic.contains (n, true) || n.containsChar ('[') || n.length() < 2)
            continue;
        return cleanName (n);
    }
    return {};
}

static void addPluginFile (std::vector<PluginFile>& out, const File& f, Format fmt, const File& root, bool is32 = false)
{
    PluginFile pf;
    pf.file = f;
    pf.format = fmt;
    pf.is32bit = is32;
    if (fmt == Format::VST3)
        readVst3Info (pf);
    const auto stem = f.getFileNameWithoutExtension();
    if (stem.startsWithIgnoreCase ("WaveShell"))
    {
        pf.isShell = true;
        pf.name = "Waves (WaveShell)";
        pf.vendor = "Waves";
    }
    if (pf.name.isEmpty())
        pf.name = cleanName (stem);
    if (pf.vendor.isEmpty())
        pf.vendor = guessVendor (f, root);
    if (keyFor (pf.vendor) == keyFor (pf.name) || keyFor (pf.name).startsWith (keyFor (pf.vendor) + "kick"))
        pf.vendor = {};
    out.push_back (std::move (pf));
}

static void walk (const File& dir, const File& root, int depth, std::vector<PluginFile>& out,
                  std::set<String>& seen, const std::function<bool()>& shouldExit)
{
    if (depth > 10 || shouldExit())
        return;
    for (const auto& entry : RangedDirectoryIterator (dir, false, "*", File::findFilesAndDirectories))
    {
        const auto f = entry.getFile();
        const auto ext = f.getFileExtension().toLowerCase();
        const bool isDir = entry.isDirectory();
        if (f.getFileName().startsWithChar ('.'))
            continue;

        const auto canon = f.getFullPathName().toLowerCase();
        auto claim = [&] { return seen.insert (canon).second; };

        if (ext == ".vst3")
        {
            if (claim()) addPluginFile (out, f, Format::VST3, root);
            continue;   // never look inside a bundle
        }
        if (ext == ".clap")
        {
            if (claim()) addPluginFile (out, f, Format::CLAP, root);
            continue;
        }
        if (ext == ".component")
        {
            if (claim()) addPluginFile (out, f, Format::AU, root);
            continue;
        }
        if (ext == ".vst" && isDir)
        {
            if (claim()) addPluginFile (out, f, Format::VST2, root);
            continue;
        }
        if (ext == ".dll" && ! isDir)
        {
            bool is32 = false;
            if (isVst2Dll (f, is32) && claim())
                addPluginFile (out, f, Format::VST2, root, is32);
            continue;
        }
        if (isDir && ! f.isSymbolicLink())
            walk (f, root, depth + 1, out, seen, shouldExit);
    }
}

Array<File> defaultPluginFolders()
{
    Array<File> r;
   #if JUCE_WINDOWS
    for (auto* p : { "C:\\Program Files\\Common Files\\VST3", "C:\\Program Files\\Common Files\\CLAP",
                     "C:\\Program Files\\VstPlugins", "C:\\Program Files\\Steinberg\\VSTPlugins",
                     "C:\\Program Files\\Common Files\\Steinberg\\VST2", "C:\\Program Files\\Common Files\\VST2",
                     "C:\\Program Files (x86)\\VstPlugins", "C:\\Program Files (x86)\\Steinberg\\VSTPlugins" })
        r.addIfNotAlreadyThere (File (p));
    const auto local = File::getSpecialLocation (File::windowsLocalAppData).getChildFile ("Programs").getChildFile ("Common");
    r.addIfNotAlreadyThere (local.getChildFile ("VST3"));
    r.addIfNotAlreadyThere (local.getChildFile ("CLAP"));
    for (auto* key : { "HKEY_LOCAL_MACHINE\\SOFTWARE\\VST\\VSTPluginsPath", "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\VST\\VSTPluginsPath" })
    {
        const auto v = WindowsRegistry::getValue (key);
        if (v.isNotEmpty())
            r.addIfNotAlreadyThere (File (v));
    }
   #elif JUCE_MAC
    for (auto* sub : { "VST3", "VST", "Components", "CLAP" })
    {
        r.add (File ("/Library/Audio/Plug-Ins").getChildFile (sub));
        r.add (File ("~/Library/Audio/Plug-Ins").getChildFile (sub));
    }
   #else
    r.add (File ("~/.vst3"));
    r.add (File ("~/.clap"));
    r.add (File ("/usr/lib/vst3"));
   #endif
    Array<File> existing;
    for (auto& f : r)
        if (f.isDirectory())
            existing.addIfNotAlreadyThere (f);
    return existing;
}

std::vector<PluginFile> scanPlugins (const Array<File>& roots, std::function<bool()> shouldExit)
{
    std::vector<PluginFile> out;
    std::set<String> seen;
    for (const auto& root : roots)
        if (root.isDirectory())
            walk (root, root, 0, out, seen, shouldExit);
    return out;
}

//------------------------------------------------------------------------------
static bool looksLikePluginPath (const String& s)
{
    const auto l = s.toLowerCase();
    return l.contains (".vst3") || l.endsWith (".dll") || l.endsWith (".clap") || l.endsWith (".vst") || l.endsWith (".component")
        || l.contains (".clap/") || l.contains (".vst/");
}

static String pluginFileNameFromPath (String p)
{
    p = p.replaceCharacter ('\\', '/');
    for (auto* ext : { ".vst3", ".clap", ".component", ".vst/" })
    {
        const auto i = p.toLowerCase().indexOf (ext);
        if (i >= 0)
        {
            p = p.substring (0, i + (int) strlen (ext));
            if (p.endsWithChar ('/')) p = p.dropLastCharacters (1);
            break;
        }
    }
    return p.fromLastOccurrenceOf ("/", false, false);
}

bool parseProject (const File& f, ProjectRecord& out)
{
    MemoryBlock mb;
    if (f.getSize() > 512 * 1024 * 1024 || ! f.loadFileAsData (mb))
        return false;
    const auto* d = static_cast<const uint8*> (mb.getData());
    const auto n = (int64) mb.getSize();
    if (n < 8 || memcmp (d, "BtWg", 4) != 0)
        return false;

    out.path = f.getFullPathName();
    out.modified = f.getLastModificationTime().toMilliseconds();
    out.size = f.getSize();
    out.uses.clear();
    out.names.clear();

    for (int64 i = 4; i + 4 < n; ++i)
    {
        if (d[i] != 'B' || d[i + 1] != 't' || d[i + 2] != 'W' || d[i + 3] != 'g')
            continue;
        // A device blob: its plugin path follows shortly after as <0x08><u32 length><utf8>,
        // before the next blob starts
        const auto end = jmin (n - 5, i + 2048);
        for (int64 j = i + 4; j < end; ++j)
        {
            if (d[j] == 'B' && d[j + 1] == 't' && d[j + 2] == 'W' && d[j + 3] == 'g')
                break;
            if (d[j] != 0x08)
                continue;
            const uint32 len = ((uint32) d[j + 1] << 24) | ((uint32) d[j + 2] << 16) | ((uint32) d[j + 3] << 8) | d[j + 4];
            if (len < 6 || len > 2048 || j + 5 + (int64) len > n)
                continue;
            const auto s = String::fromUTF8 ((const char*) d + j + 5, (int) len);
            if (! looksLikePluginPath (s))
                continue;
            const auto fileName = pluginFileNameFromPath (s);
            const auto key = keyForFileName (fileName);
            if (key.isNotEmpty())
            {
                out.uses[key]++;
                out.names[key] = fileName;
            }
            i = j + 5 + len - 1;
            break;
        }
    }
    return true;
}

static void walkProjects (const File& dir, int depth, Array<File>& out, const std::function<bool()>& shouldExit)
{
    if (depth > 12 || shouldExit())
        return;
    for (const auto& entry : RangedDirectoryIterator (dir, false, "*", File::findFilesAndDirectories))
    {
        const auto f = entry.getFile();
        const auto name = f.getFileName();
        if (name.startsWithChar ('.') || name.containsIgnoreCase ("backup"))
            continue;
        if (entry.isDirectory())
        {
            if (! f.isSymbolicLink())
                walkProjects (f, depth + 1, out, shouldExit);
        }
        else if (f.hasFileExtension ("bwproject"))
            out.add (f);
    }
}

std::vector<ProjectRecord> scanProjects (const Array<File>& roots, const std::map<String, ProjectRecord>& cache,
                                         std::function<bool()> shouldExit)
{
    Array<File> files;
    for (const auto& r : roots)
        if (r.isDirectory())
            walkProjects (r, 0, files, shouldExit);

    std::vector<ProjectRecord> out;
    std::set<String> done;
    for (const auto& f : files)
    {
        if (shouldExit())
            break;
        const auto path = f.getFullPathName();
        if (! done.insert (path).second)
            continue;
        const auto it = cache.find (path);
        if (it != cache.end() && it->second.modified == f.getLastModificationTime().toMilliseconds() && it->second.size == f.getSize())
        {
            out.push_back (it->second);
            continue;
        }
        ProjectRecord rec;
        if (parseProject (f, rec))
            out.push_back (std::move (rec));
    }
    return out;
}
} // namespace Scanner

//==============================================================================
Library::Library()
{
    loadAll();
}

File Library::dataDir() const
{
    auto d = File::getSpecialLocation (File::userApplicationDataDirectory);
   #if JUCE_MAC
    d = d.getChildFile ("Application Support");
   #endif
    d = d.getChildFile ("Primal Rage Lab").getChildFile ("Clean Your Mind");
    d.createDirectory();
    return d;
}

File Library::imageFile (const String& key) const
{
    auto d = dataDir().getChildFile ("images");
    d.createDirectory();
    return d.getChildFile (key + ".png");
}

File Library::originalImageFile (const String& key) const
{
    auto d = dataDir().getChildFile ("images").getChildFile ("originals");
    d.createDirectory();
    return d.getChildFile (key + ".png");
}

File Library::quarantineDir() const
{
    auto d = dataDir().getChildFile ("Quarantine");
    d.createDirectory();
    return d;
}

Plugin* Library::find (const String& key)
{
    for (auto& p : plugins)
        if (p.key == key)
            return &p;
    return nullptr;
}

static var filesToVar (const Array<File>& a)
{
    Array<var> r;
    for (auto& f : a) r.add (f.getFullPathName());
    return r;
}

static Array<File> varToFiles (const var& v)
{
    Array<File> r;
    if (auto* arr = v.getArray())
        for (auto& x : *arr)
            if (File::isAbsolutePath (x.toString()))
                r.addIfNotAlreadyThere (File (x.toString()));
    return r;
}

void Library::loadAll()
{
    const auto s = JSON::parse (dataDir().getChildFile ("settings.json"));
    if (s.isObject())
    {
        pluginFolders = varToFiles (s["pluginFolders"]);
        projectFolders = varToFiles (s["projectFolders"]);
        if (auto* fp = s["formatPriority"].getArray())
        {
            StringArray p;
            for (auto& x : *fp) p.add (x.toString());
            if (p.size() == 4) formatPriority = p;
        }
        staleMonths = jlimit (1, 120, (int) s.getProperty ("staleMonths", 12));
        themeName = s.getProperty ("theme", "Asphalt").toString();
        lang = s.getProperty ("lang", "ru").toString();
        lastScan = Time ((int64) s.getProperty ("lastScan", 0));
    }
    else
    {
        pluginFolders = Scanner::defaultPluginFolders();
    }

    const auto u = JSON::parse (dataDir().getChildFile ("userdata.json"));
    if (auto* o = u.getDynamicObject())
        for (auto& p : o->getProperties())
        {
            UserMeta m;
            m.favorite = p.value.getProperty ("fav", false);
            m.type = p.value.getProperty ("type", -1);
            m.category = p.value.getProperty ("cat", "").toString();
            m.dragFormat = p.value.getProperty ("drag", "").toString();
            meta[p.name.toString()] = m;
            const auto img = p.value.getProperty ("img", {});
            if (img.isObject())
                imageStates[p.name.toString()] = { (float) img.getProperty ("z", 1.0), (float) img.getProperty ("x", 0.5),
                                                   (float) img.getProperty ("y", 0.5), (int) img.getProperty ("m", 0) };
        }

    const auto pr = JSON::parse (dataDir().getChildFile ("projects.json"));
    if (auto* arr = pr.getArray())
        for (auto& x : *arr)
        {
            ProjectRecord r;
            r.path = x["path"].toString();
            r.modified = (int64) x["modified"];
            r.size = (int64) x["size"];
            if (auto* uo = x["uses"].getDynamicObject())
                for (auto& kv : uo->getProperties())
                    r.uses[kv.name.toString()] = (int) kv.value;
            if (auto* no = x["names"].getDynamicObject())
                for (auto& kv : no->getProperties())
                    r.names[kv.name.toString()] = kv.value.toString();
            projectCache[r.path] = std::move (r);
        }

    const auto q = JSON::parse (dataDir().getChildFile ("quarantine.json"));
    if (auto* arr = q.getArray())
        for (auto& x : *arr)
            quarantined.push_back ({ x["key"].toString(), x["name"].toString(), x["from"].toString(), x["to"].toString(), (int64) x["when"] });
}

void Library::saveSettings()
{
    auto* o = new DynamicObject();
    o->setProperty ("pluginFolders", filesToVar (pluginFolders));
    o->setProperty ("projectFolders", filesToVar (projectFolders));
    Array<var> fp;
    for (auto& f : formatPriority) fp.add (f);
    o->setProperty ("formatPriority", fp);
    o->setProperty ("staleMonths", staleMonths);
    o->setProperty ("theme", themeName);
    o->setProperty ("lang", lang);
    o->setProperty ("lastScan", lastScan.toMilliseconds());
    dataDir().getChildFile ("settings.json").replaceWithText (JSON::toString (var (o)));
}

void Library::captureMeta()
{
    for (const auto& p : plugins)
    {
        auto& m = meta[p.key];
        m.favorite = p.favorite;
        m.type = (int) p.userType;
        m.category = p.userCategory;
        m.dragFormat = p.dragFormat;
    }
}

void Library::saveUserData()
{
    captureMeta();
    auto* o = new DynamicObject();
    std::set<String> keys;
    for (auto& [k, m] : meta) keys.insert (k);
    for (auto& [k, s] : imageStates) keys.insert (k);
    for (const auto& k : keys)
    {
        auto* e = new DynamicObject();
        const auto it = meta.find (k);
        if (it != meta.end())
        {
            const auto& m = it->second;
            if (! m.favorite && m.type < 0 && m.category.isEmpty() && m.dragFormat.isEmpty() && imageStates.count (k) == 0)
            {
                delete e;
                continue;
            }
            e->setProperty ("fav", m.favorite);
            e->setProperty ("type", m.type);
            e->setProperty ("cat", m.category);
            e->setProperty ("drag", m.dragFormat);
        }
        const auto is = imageStates.find (k);
        if (is != imageStates.end())
        {
            auto* img = new DynamicObject();
            img->setProperty ("z", is->second.zoom);
            img->setProperty ("x", is->second.cx);
            img->setProperty ("y", is->second.cy);
            img->setProperty ("m", is->second.mode);
            e->setProperty ("img", var (img));
        }
        o->setProperty (k, var (e));
    }
    dataDir().getChildFile ("userdata.json").replaceWithText (JSON::toString (var (o)));
}

void Library::saveProjectCache()
{
    Array<var> arr;
    for (auto& [path, r] : projectCache)
    {
        auto* o = new DynamicObject();
        o->setProperty ("path", r.path);
        o->setProperty ("modified", r.modified);
        o->setProperty ("size", r.size);
        auto* u = new DynamicObject();
        for (auto& [k, n] : r.uses) u->setProperty (k, n);
        o->setProperty ("uses", var (u));
        auto* nm = new DynamicObject();
        for (auto& [k, n] : r.names) nm->setProperty (k, n);
        o->setProperty ("names", var (nm));
        arr.add (var (o));
    }
    dataDir().getChildFile ("projects.json").replaceWithText (JSON::toString (arr, true));
}

void Library::saveQuarantine()
{
    Array<var> arr;
    for (auto& q : quarantined)
    {
        auto* o = new DynamicObject();
        o->setProperty ("key", q.key);
        o->setProperty ("name", q.name);
        o->setProperty ("from", q.from);
        o->setProperty ("to", q.to);
        o->setProperty ("when", q.when);
        arr.add (var (o));
    }
    dataDir().getChildFile ("quarantine.json").replaceWithText (JSON::toString (arr));
}

void Library::applyScan (ScanResult&& r)
{
    captureMeta();
    std::map<String, Plugin> byKey;
    for (auto& f : r.files)
    {
        // the key comes from the file name, the same way project files are read,
        // so "Pro-Q 3.vst3" and "Pro-Q 3.clap" end up on one card and match the projects
        const auto key = f.isShell ? String ("wavesshell") : Scanner::keyForFileName (f.file.getFileName());
        auto& p = byKey[key];
        if (p.key.isEmpty())
        {
            p.key = key;
            p.name = f.name;
            p.vendor = f.vendor;
        }
        // prefer the nicest name: VST3 moduleinfo first
        if (f.format == Format::VST3 && ! f.isShell && f.name.isNotEmpty())
            p.name = f.name;
        if (p.vendor.isEmpty())
            p.vendor = f.vendor;
        p.files.push_back (std::move (f));
    }

    plugins.clear();
    for (auto& [k, p] : byKey)
    {
        const auto it = meta.find (k);
        if (it != meta.end())
        {
            p.favorite = it->second.favorite;
            p.userType = (PluginType) it->second.type;
            p.userCategory = it->second.category;
            p.dragFormat = it->second.dragFormat;
        }
        plugins.push_back (std::move (p));
    }

    if (! projectFolders.isEmpty() || ! r.projects.empty())
    {
        projectCache.clear();
        for (auto& pr : r.projects)
            projectCache[pr.path] = std::move (pr);
    }

    lastScan = Time::getCurrentTime();
    computeStats();
    saveSettings();
    saveProjectCache();
}

void Library::computeStats()
{
    std::map<String, Plugin*> idx;
    for (auto& p : plugins)
    {
        p.uses = p.projects = 0;
        p.firstUsed = p.lastUsed = Time();
        idx[p.key] = &p;
    }
    for (auto& [path, r] : projectCache)
    {
        const Time t (r.modified);
        for (auto& [k, n] : r.uses)
        {
            auto key = k;
            if (key.startsWith ("waveshell"))
                key = "wavesshell";
            auto it = idx.find (key);
            if (it == idx.end())
                continue;
            auto* p = it->second;
            p->uses += n;
            p->projects++;
            if (p->lastUsed.toMilliseconds() == 0 || t > p->lastUsed) p->lastUsed = t;
            if (p->firstUsed.toMilliseconds() == 0 || t < p->firstUsed) p->firstUsed = t;
        }
    }
}
